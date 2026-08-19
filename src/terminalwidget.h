#pragma once
#include <QPlainTextEdit>
#include <QProcess>

class TerminalWidget : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget *parent = nullptr);
    void start(const QString &shell, const QString &workingDir);
    void sendCommand(const QString &command);
    void stop();
    QProcess *process() const { return m_process; }

signals:
    void titleChanged(const QString &title);
    void exitStatusChanged(int code);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    QProcess *m_process;
    QString m_shell;
    int m_promptPosition = 0;
    void appendOutput(const QByteArray &data);
};
