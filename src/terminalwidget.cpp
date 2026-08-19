#include "terminalwidget.h"
#include <QKeyEvent>
#include <QTextCursor>
#include <QScrollBar>
#include <QApplication>
#include <QDir>

TerminalWidget::TerminalWidget(QWidget *parent) : QPlainTextEdit(parent), m_process(new QProcess(this)) {
    setUndoRedoEnabled(false);
    setWordWrapMode(QTextOption::NoWrap);
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    setStyleSheet("QPlainTextEdit { background:#101318; color:#e6edf3; selection-background-color:#264f78; border:0; padding:8px; }");
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this] { appendOutput(m_process->readAllStandardOutput()); });
    connect(m_process, &QProcess::readyReadStandardError, this, [this] { appendOutput(m_process->readAllStandardError()); });
    connect(m_process, qOverload<int,QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus){ emit exitStatusChanged(code); });
}

void TerminalWidget::start(const QString &shell, const QString &workingDir) {
    m_shell = shell.isEmpty() ? qEnvironmentVariable("SHELL", "/bin/sh") : shell;
    if (m_process->state() != QProcess::NotRunning) return;
    m_process->setWorkingDirectory(workingDir.isEmpty() ? QDir::homePath() : workingDir);
    m_process->setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    m_process->start(m_shell, {"-i"});
    if (!m_process->waitForStarted(1500)) {
        appendPlainText("Failed to start shell: " + m_process->errorString());
    }
}

void TerminalWidget::appendOutput(const QByteArray &data) {
    QString text = QString::fromLocal8Bit(data);
    text.replace("\r", "");
    moveCursor(QTextCursor::End);
    insertPlainText(text);
    m_promptPosition = textCursor().position();
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void TerminalWidget::sendCommand(const QString &command) {
    if (m_process->state() == QProcess::Running) m_process->write(command.toLocal8Bit() + "\n");
}

void TerminalWidget::stop() { m_process->terminate(); }

void TerminalWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_C && event->modifiers() & Qt::ControlModifier) {
        if (m_process->state() == QProcess::Running) m_process->write("\x03");
        return;
    }
    if (event->key() == Qt::Key_Backspace && textCursor().position() <= m_promptPosition) return;
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        QTextCursor c = textCursor(); c.movePosition(QTextCursor::End); setTextCursor(c);
        QString line = document()->lastBlock().text();
        if (m_process->state() == QProcess::Running) m_process->write(line.toLocal8Bit() + "\n");
        QPlainTextEdit::keyPressEvent(event);
        m_promptPosition = textCursor().position();
        return;
    }
    if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down || event->key() == Qt::Key_Left) {
        if (textCursor().position() < m_promptPosition) return;
    }
    QPlainTextEdit::keyPressEvent(event);
}
