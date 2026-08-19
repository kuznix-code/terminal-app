#pragma once
#include <QPlainTextEdit>
#include <QProcess>

class TerminalWidget : public QPlainTextEdit {
public:
    explicit TerminalWidget(QWidget *parent = nullptr);
    void start(const QString &shell, const QString &workingDir);
    void sendCommand(const QString &command);
    void stop();
    QProcess *process() const { return m_process; }

    using ExitStatusCallback = std::function<void(int)>;
    void setExitStatusCallback(ExitStatusCallback callback) { m_exitStatusCallback = std::move(callback); }

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    QProcess *m_process;
    QString m_shell;
    int m_promptPosition = 0;
    ExitStatusCallback m_exitStatusCallback;
    void appendOutput(const QByteArray &data);
};
