#include "terminalwidget.h"

#include <QDir>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>

TerminalWidget::TerminalWidget(QWidget *parent) : QPlainTextEdit(parent), m_process(new QProcess(this)) {
    setUndoRedoEnabled(false);
    setWordWrapMode(QTextOption::NoWrap);
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    setMaximumBlockCount(10000);
    setCursorWidth(2);
    setStyleSheet("QPlainTextEdit { background:#0f141a; color:#e8edf2; selection-background-color:#316ac5; border:1px solid #687585; padding:6px; }");

    connect(m_process, &QProcess::readyReadStandardOutput, this, [this] {
        appendOutput(m_process->readAllStandardOutput());
    });
    connect(m_process, &QProcess::readyReadStandardError, this, [this] {
        appendOutput(m_process->readAllStandardError());
    });
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
    env.insert("PS1", "kuznix$ ");
    env.insert("PS2", "> ");
    m_process->setProcessEnvironment(env);

    m_process->start(m_shell, {"-i"});
    if (!m_process->waitForStarted(1500)) {
        appendPlainText("Kuznix Terminal: failed to start shell: " + m_process->errorString());
    }
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
    text.replace('\r', '');
    moveCursor(QTextCursor::End);
    insertPlainText(text);
    m_promptPosition = textCursor().position();
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void TerminalWidget::sendCommand(const QString &command) {
    if (m_process->state() != QProcess::Running) {
        appendPlainText("Kuznix Terminal: shell is not running.");
        return;
    }
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
        setTextCursor(cursor);
        const QString line = document()->lastBlock().text();
        if (m_process->state() == QProcess::Running) m_process->write(line.toLocal8Bit() + "\n");
        QPlainTextEdit::keyPressEvent(event);
        m_promptPosition = textCursor().position();
        return;
    }
    if (event->key() == Qt::Key_Backspace && textCursor().position() <= m_promptPosition) return;
    if ((event->key() == Qt::Key_Left || event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) && textCursor().position() < m_promptPosition) return;
    QPlainTextEdit::keyPressEvent(event);
}
