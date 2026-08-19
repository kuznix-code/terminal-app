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

    connect(
        m_process,
        &QProcess::readyReadStandardOutput,
        this,
        [this] {
            appendOutput(m_process->readAllStandardOutput());
        });

    connect(
        m_process,
        &QProcess::readyReadStandardError,
        this,
        [this] {
            appendOutput(m_process->readAllStandardError());
        });

    connect(
        m_process,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        [this](int code, QProcess::ExitStatus) {
            m_promptPosition = textCursor().position();
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
                    QStringLiteral("Kuznix Terminal: failed to start shell: %1")
                        .arg(m_process->errorString()));
                m_promptPosition = textCursor().position();
            }
        });
}

void TerminalWidget::start(const QString &shell, const QString &workingDir) {
    if (m_process->state() != QProcess::NotRunning)
        return;

    m_shell = shell.isEmpty()
        ? qEnvironmentVariable("SHELL", "/bin/bash")
        : shell;

    m_workingDirectory = workingDir.isEmpty()
        ? QDir::homePath()
        : workingDir;

    if (!QDir(m_workingDirectory).exists())
        m_workingDirectory = QDir::homePath();

    m_process->setWorkingDirectory(m_workingDirectory);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("TERM", "dumb");
    env.insert("COLORTERM", "");
    env.insert("NO_COLOR", "1");
    env.insert("CLICOLOR", "0");
    env.insert("PROMPT_COMMAND", "");
    env.insert("PS1", m_prompt);
    env.insert("PS2", "> ");

    const QFileInfo shellInfo(m_shell);
    QStringList arguments;

    if (shellInfo.fileName() == "bash") {
        arguments = {"--noprofile", "--norc", "-i"};
    } else if (shellInfo.fileName() == "zsh") {
        arguments = {"-f", "-i"};
    } else {
        arguments = {"-i"};
    }

    m_process->setProcessEnvironment(env);
    clear();
    m_promptPosition = 0;

    m_process->start(m_shell, arguments);

    if (!m_process->waitForStarted(1500)) {
        appendPlainText(
            QStringLiteral("Kuznix Terminal: failed to start shell: %1")
                .arg(m_process->errorString()));
        m_promptPosition = textCursor().position();
        return;
    }

    if (shellInfo.fileName() == "bash" ||
        shellInfo.fileName() == "zsh") {
        m_process->write(
            QByteArrayLiteral(
                "PS1='kuznix$ '; PS2='> '; PROMPT_COMMAND=''\n"));
    }
}

QString TerminalWidget::stripAnsi(const QString &text) const {
    QString result = text;

    static const QRegularExpression csi(
        QStringLiteral("\\x1B\\[[0-?]*[ -/]*[@-~]"));

    static const QRegularExpression osc(
        QStringLiteral("\\x1B\\][^\\x07]*(?:\\x07|\\x1B\\\\)"));

    result.remove(osc);
    result.remove(csi);
    result.remove(QChar(0x1b));

    return result;
}

void TerminalWidget::appendOutput(const QByteArray &data) {
    if (data.isEmpty())
        return;

    QString text = stripAnsi(QString::fromLocal8Bit(data));
    text.remove(QChar('\r'));

    if (text.isEmpty())
        return;

    moveCursor(QTextCursor::End);
    insertPlainText(text);

    const int promptInChunk = text.lastIndexOf(m_prompt);

    if (promptInChunk >= 0) {
        m_promptPosition =
            textCursor().position() - text.length()
            + promptInChunk + m_prompt.length();
    } else {
        m_promptPosition = qBound(
            0,
            m_promptPosition,
            textCursor().position());
    }

    verticalScrollBar()->setValue(
        verticalScrollBar()->maximum());
}

void TerminalWidget::updatePromptPosition() {
    const int documentLength = document()->characterCount() - 1;

    m_promptPosition = qBound(
        0,
        m_promptPosition,
        qMax(0, documentLength));
}

void TerminalWidget::sendCommand(const QString &command) {
    if (m_process->state() != QProcess::Running) {
        appendPlainText("Kuznix Terminal: shell is not running.");
        m_promptPosition = textCursor().position();
        return;
    }

    m_process->write(command.toLocal8Bit());
    m_process->write("\n");
}

void TerminalWidget::stop() {
    if (m_process->state() == QProcess::NotRunning)
        return;

    m_process->terminate();

    if (!m_process->waitForFinished(500))
        m_process->kill();
}

void TerminalWidget::setTerminalColors(
    const QColor &background,
    const QColor &foreground,
    const QColor &selection) {

    QPalette p = palette();
    p.setColor(QPalette::Base, background);
    p.setColor(QPalette::Text, foreground);
    p.setColor(QPalette::Highlight, selection);
    p.setColor(QPalette::HighlightedText, QColor(Qt::white));
    setPalette(p);
}

void TerminalWidget::setTerminalFont(const QFont &font) {
    setFont(font);
}

void TerminalWidget::setScrollback(int lines) {
    setMaximumBlockCount(qBound(1000, lines, 1000000));
}

void TerminalWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_C &&
        (event->modifiers() & Qt::ControlModifier)) {
        if (m_process->state() == QProcess::Running)
            m_process->write("\x03");
        return;
    }

    if (event->key() == Qt::Key_D &&
        (event->modifiers() & Qt::ControlModifier)) {
        if (m_process->state() == QProcess::Running)
            m_process->write("\x04");
        return;
    }

    if (event->key() == Qt::Key_Return ||
        event->key() == Qt::Key_Enter) {

        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::End);
        updatePromptPosition();

        cursor.setPosition(
            m_promptPosition,
            QTextCursor::KeepAnchor);

        const QString command = cursor.selectedText();

        if (m_process->state() == QProcess::Running) {
            m_process->write(command.toLocal8Bit());
            m_process->write("\n");
        }

        moveCursor(QTextCursor::End);
        return;
    }

    if (event->key() == Qt::Key_Home) {
        QTextCursor cursor = textCursor();
        cursor.setPosition(m_promptPosition);
        setTextCursor(cursor);
        return;
    }

    if (event->key() == Qt::Key_Backspace &&
        textCursor().position() <= m_promptPosition) {
        return;
    }

    if (event->key() == Qt::Key_Left &&
        textCursor().position() <= m_promptPosition) {
        return;
    }

    if (textCursor().position() < m_promptPosition)
        moveCursor(QTextCursor::End);

    QPlainTextEdit::keyPressEvent(event);
}
