#include "terminalwidget.h"

#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextOption>

TerminalWidget::TerminalWidget(QWidget *parent)
    : QPlainTextEdit(parent),
      m_process(new QProcess(this)) {

    setUndoRedoEnabled(false);
    setWordWrapMode(QTextOption::NoWrap);
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    setMaximumBlockCount(10000);
    setCursorWidth(2);
    setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);

    setAcceptRichText(false);

    connect(
        m_process,
        &QProcess::readyReadStandardOutput,
        this,
        [this] {
            appendOutput(
                m_process->readAllStandardOutput());
        });

    connect(
        m_process,
        &QProcess::readyReadStandardError,
        this,
        [this] {
            appendOutput(
                m_process->readAllStandardError());
        });

    connect(
        m_process,
        qOverload<int, QProcess::ExitStatus>(
            &QProcess::finished),
        this,
        [this](int code, QProcess::ExitStatus) {

            m_promptPosition =
                textCursor().position();

            if (m_exitStatusCallback)
                m_exitStatusCallback(code);
        });

    connect(
        m_process,
        &QProcess::errorOccurred,
        this,
        [this](QProcess::ProcessError error) {

            if (error == QProcess::FailedToStart) {
                appendPlainText(
                    QStringLiteral(
                        "\nKuznix Terminal: failed to start shell: %1")
                        .arg(m_process->errorString()));

                m_promptPosition =
                    textCursor().position();
            }
        });
}

void TerminalWidget::start(
    const QString &shell,
    const QString &workingDir) {

    if (m_process->state() != QProcess::NotRunning)
        return;

    m_shell =
        shell.isEmpty()
            ? qEnvironmentVariable(
                  "SHELL",
                  "/bin/bash")
            : shell;

    m_workingDirectory =
        workingDir.isEmpty()
            ? QDir::homePath()
            : workingDir;

    QDir workingDirObject(m_workingDirectory);

    if (!workingDirObject.exists())
        m_workingDirectory = QDir::homePath();

    m_process->setWorkingDirectory(
        m_workingDirectory);

    QProcessEnvironment env =
        QProcessEnvironment::systemEnvironment();

    /*
     * The terminal widget is not a PTY terminal emulator.
     * We therefore deliberately use a simple prompt and
     * disable terminal colour/control sequences.
     */
    env.insert(
        QStringLiteral("TERM"),
        QStringLiteral("dumb"));

    env.insert(
        QStringLiteral("COLORTERM"),
        QString());

    env.insert(
        QStringLiteral("NO_COLOR"),
        QStringLiteral("1"));

    env.insert(
        QStringLiteral("CLICOLOR"),
        QStringLiteral("0"));

    env.insert(
        QStringLiteral("PROMPT_COMMAND"),
        QString());

    env.insert(
        QStringLiteral("PS1"),
        m_prompt);

    env.insert(
        QStringLiteral("PS2"),
        QStringLiteral("> "));

    /*
     * Bash startup files frequently override PS1.
     * Start bash without them so that our prompt remains
     * synchronized with m_promptPosition.
     */
    const QFileInfo shellInfo(m_shell);

    QStringList arguments;

    if (shellInfo.fileName() == QStringLiteral("bash")) {
        arguments = {
            QStringLiteral("--noprofile"),
            QStringLiteral("--norc"),
            QStringLiteral("-i")
        };
    } else if (shellInfo.fileName() == QStringLiteral("zsh")) {
        /*
         * zsh uses ZDOTDIR for startup files. We still pass
         * an interactive shell and explicitly set the prompt
         * again immediately after startup.
         */
        arguments = {
            QStringLiteral("-f"),
            QStringLiteral("-i")
        };
    } else {
        arguments = {
            QStringLiteral("-i")
        };
    }

    m_process->setProcessEnvironment(env);

    clear();

    m_promptPosition = 0;

    m_process->start(
        m_shell,
        arguments);

    if (!m_process->waitForStarted(1500)) {
        appendPlainText(
            QStringLiteral(
                "Kuznix Terminal: failed to start shell: %1")
                .arg(m_process->errorString()));

        m_promptPosition =
            textCursor().position();

        return;
    }

    /*
     * Make absolutely sure shells that support PS1 use our
     * controlled prompt even if their startup mechanism changes it.
     */
    if (shellInfo.fileName() == QStringLiteral("bash") ||
        shellInfo.fileName() == QStringLiteral("zsh")) {

        const QByteArray promptCommand =
            QByteArrayLiteral(
                "PS1='kuznix$ '; "
                "PS2='> '; "
                "PROMPT_COMMAND=''\n");

        m_process->write(promptCommand);
    }
}

QString TerminalWidget::stripAnsi(
    const QString &text) const {

    QString result = text;

    /*
     * CSI sequences:
     * ESC [ ... command
     */
    static const QRegularExpression csi(
        QStringLiteral(
            "\\x1B\\[[0-?]*[ -/]*[@-~]"));

    /*
     * OSC sequences:
     * ESC ] ... BEL
     * ESC ] ... ESC \
     */
    static const QRegularExpression osc(
        QStringLiteral(
            "\\x1B\\][^\\x07]*(?:\\x07|\\x1B\\\\)"));

    result.remove(osc);
    result.remove(csi);

    /*
     * Remove remaining ESC characters.
     */
    result.remove(QChar(0x1b));

    return result;
}

void TerminalWidget::appendOutput(
    const QByteArray &data) {

    if (data.isEmpty())
        return;

    QString text =
        stripAnsi(
            QString::fromLocal8Bit(data));

    /*
     * CR is not useful for this simple text terminal.
     * Removing it prevents strange carriage-return
     * behaviour from corrupting the editable prompt.
     */
    text.remove(QChar('\r'));

    if (text.isEmpty())
        return;

    moveCursor(QTextCursor::End);

    insertPlainText(text);

    /*
     * Find the most recent prompt in this output.
     *
     * Because our prompt is controlled and Bash is started
     * without startup files, this reliably identifies the
     * start of editable input.
     */
    const int promptInChunk =
        text.lastIndexOf(m_prompt);

    if (promptInChunk >= 0) {
        m_promptPosition =
            textCursor().position()
            - text.length()
            + promptInChunk
            + m_prompt.length();
    } else {
        /*
         * Don't allow the prompt position to point beyond
         * the end of the document.
         */
        m_promptPosition =
            qBound(
                0,
                m_promptPosition,
                textCursor().position());
    }

    verticalScrollBar()->setValue(
        verticalScrollBar()->maximum());
}

void TerminalWidget::updatePromptPosition() {
    const int documentLength =
        document()->characterCount() - 1;

    m_promptPosition =
        qBound(
            0,
            m_promptPosition,
            qMax(0, documentLength));
}

void TerminalWidget::sendCommand(
    const QString &command) {

    if (m_process->state() != QProcess::Running) {
        appendPlainText(
            QStringLiteral(
                "Kuznix Terminal: shell is not running."));

        m_promptPosition =
            textCursor().position();

        return;
    }

    /*
     * Programmatic commands must never be inserted into
     * the document themselves. Bash will echo them.
     */
    m_process->write(
        command.toLocal8Bit());

    m_process->write(
        QByteArrayLiteral("\n"));
}

void TerminalWidget::stop() {
    if (m_process->state() != QProcess::NotRunning) {
        m_process->terminate();

        /*
         * Give the shell a short opportunity to exit cleanly.
         */
        if (!m_process->waitForFinished(500))
            m_process->kill();
    }
}

void TerminalWidget::setTerminalColors(
    const QColor &background,
    const QColor &foreground,
    const QColor &selection) {

    QPalette p = palette();

    p.setColor(
        QPalette::Base,
        background);

    p.setColor(
        QPalette::Text,
        foreground);

    p.setColor(
        QPalette::Highlight,
        selection);

    p.setColor(
        QPalette::HighlightedText,
        QColor(Qt::white));

    setPalette(p);
}

void TerminalWidget::setTerminalFont(
    const QFont &font) {

    setFont(font);
}

void TerminalWidget::setScrollback(
    int lines) {

    setMaximumBlockCount(
        qBound(
            1000,
            lines,
            1000000));
}

void TerminalWidget::keyPressEvent(
    QKeyEvent *event) {

    /*
     * Ctrl+C -> send SIGINT-equivalent byte to the shell.
     */
    if (event->key() == Qt::Key_C &&
        (event->modifiers() &
         Qt::ControlModifier)) {

        if (m_process->state() ==
            QProcess::Running) {

            m_process->write(
                QByteArrayLiteral("\x03"));
        }

        return;
    }

    /*
     * Ctrl+D -> EOF.
     */
    if (event->key() == Qt::Key_D &&
        (event->modifiers() &
         Qt::ControlModifier)) {

        if (m_process->state() ==
            QProcess::Running) {

            m_process->write(
                QByteArrayLiteral("\x04"));
        }

        return;
    }

    /*
     * Enter executes only the text after the prompt.
     *
     * This is the important fix for:
     *
     * bash: ... No such file or directory
     */
    if (event->key() == Qt::Key_Return ||
        event->key() == Qt::Key_Enter) {

        QTextCursor cursor =
            textCursor();

        cursor.movePosition(
            QTextCursor::End);

        /*
         * Ensure the prompt position is valid.
         */
        updatePromptPosition();

        /*
         * Select only the editable command area.
         */
        cursor.setPosition(
            m_promptPosition,
            QTextCursor::KeepAnchor);

        const QString command =
            cursor.selectedText();

        /*
         * Send exactly what was typed.
         * The visible prompt is never included.
         */
        if (m_process->state() ==
            QProcess::Running) {

            m_process->write(
                command.toLocal8Bit());

            m_process->write(
                QByteArrayLiteral("\n"));
        }

        /*
         * Do not manually add another newline here.
         * Bash echoes the command and prints the next prompt.
         */
        moveCursor(QTextCursor::End);

        return;
    }

    /*
     * Home should go to the beginning of the command,
     * not to the beginning of the terminal history.
     */
    if (event->key() == Qt::Key_Home) {

        QTextCursor cursor =
            textCursor();

        cursor.setPosition(
            m_promptPosition);

        setTextCursor(cursor);

        return;
    }

    /*
     * Backspace cannot delete the prompt.
     */
    if (event->key() ==
            Qt::Key_Backspace &&
        textCursor().position() <=
            m_promptPosition) {

        return;
    }

    /*
     * Left arrow cannot move into the prompt.
     */
    if (event->key() == Qt::Key_Left &&
        textCursor().position() <=
            m_promptPosition) {

        return;
    }

    /*
     * Prevent editing anywhere before the prompt.
     */
    if (textCursor().position() <
        m_promptPosition) {

        moveCursor(
            QTextCursor::End);
    }

    QPlainTextEdit::keyPressEvent(event);
}
