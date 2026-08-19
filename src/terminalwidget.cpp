#include "terminalwidget.h"

#include <QDir>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextCursor>

TerminalWidget::TerminalWidget(QWidget *parent) : QPlainTextEdit(parent), m_process(new QProcess(this)) {
    setUndoRedoEnabled(false);
    setWordWrapMode(QTextOption::NoWrap);
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    setMaximumBlockCount(10000);
    setCursorWidth(2);
    setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);

    connect(m_process, &QProcess::readyReadStandardOutput, this, [this] { appendOutput(m_process->readAllStandardOutput()); });
    connect(m_process, &QProcess::readyReadStandardError, this, [this] { appendOutput(m_process->readAllStandardError()); });
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus) {
                if (m_exitStatusCallback) m_exitStatusCallback(code);
                m_promptPosition = textCursor().position();
            });
}

void TerminalWidget::start(const QString &shell, const QString &workingDir) {
    if (m_process->state() != QProcess::NotRunning) return;
    m_shell = shell.isEmpty() ? qEnvironmentVariable("SHELL", "/bin/bash") : shell;
    m_process->setWorkingDirectory(workingDir.isEmpty() ? QDir::homePath() : workingDir);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("TERM", "dumb");
    env.insert("COLORTERM", "");
    env.insert("NO_COLOR", "1");
    env.insert("CLICOLOR", "0");
    env.insert("PROMPT_COMMAND", "");
    env.insert("PS1", m_prompt);
    env.insert("PS2", "> ");
    m_process->setProcessEnvironment(env);
    m_process->start(m_shell, {"-i"});
    if (!m_process->waitForStarted(1500))
        appendPlainText("Kuznix Terminal: failed to start shell: " + m_process->errorString());
}

QString TerminalWidget::stripAnsi(const QString &text) const {
    QString result = text;
    static const QRegularExpression csi(QStringLiteral("\\x1B\\[[0-?]*[ -/]*[@-~]"));
    static const QRegularExpression osc(QStringLiteral("\\x1B\\][^\\x07]*(?:\\x07|\\x1B\\\\)"));
    result.remove(osc);
    result.remove(csi);
    result.remove(QChar(0x1b));
    return result;
}

void TerminalWidget::appendOutput(const QByteArray &data) {
    QString text = stripAnsi(QString::fromLocal8Bit(data));
    text.remove(QChar('\r'));
    if (text.isEmpty()) return;
    moveCursor(QTextCursor::End);
    insertPlainText(text);

    const int promptInChunk = text.lastIndexOf(m_prompt);
    if (promptInChunk >= 0)
        m_promptPosition = textCursor().position() - text.length() + promptInChunk + m_prompt.length();
    else if (m_promptPosition > textCursor().position())
        m_promptPosition = textCursor().position();
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void TerminalWidget::sendCommand(const QString &command) {
    if (m_process->state() != QProcess::Running) {
        appendPlainText("Kuznix Terminal: shell is not running.");
        return;
    }
    moveCursor(QTextCursor::End);
    m_process->write(command.toLocal8Bit());
    m_process->write("\n");
}

void TerminalWidget::stop() {
    if (m_process->state() != QProcess::NotRunning) m_process->terminate();
}

void TerminalWidget::setTerminalColors(const QColor &background, const QColor &foreground, const QColor &selection) {
    QPalette p = palette();
    p.setColor(QPalette::Base, background);
    p.setColor(QPalette::Text, foreground);
    p.setColor(QPalette::Highlight, selection);
    p.setColor(QPalette::HighlightedText, QColor(Qt::white));
    setPalette(p);
}

void TerminalWidget::setTerminalFont(const QFont &font) { setFont(font); }
void TerminalWidget::setScrollback(int lines) { setMaximumBlockCount(qBound(1000, lines, 1000000)); }

void TerminalWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_C && event->modifiers() & Qt::ControlModifier) {
        if (m_process->state() == QProcess::Running) m_process->write("\x03");
        return;
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.setPosition(m_promptPosition, QTextCursor::KeepAnchor);
        const QString command = cursor.selectedText();
        if (m_process->state() == QProcess::Running) m_process->write(command.toLocal8Bit() + "\n");
        moveCursor(QTextCursor::End);
        QPlainTextEdit::insertPlainText("\n");
        m_promptPosition = textCursor().position();
        return;
    }

    if (textCursor().position() < m_promptPosition) moveCursor(QTextCursor::End);
    if (event->key() == Qt::Key_Backspace && textCursor().position() <= m_promptPosition) return;
    QPlainTextEdit::keyPressEvent(event);
}
