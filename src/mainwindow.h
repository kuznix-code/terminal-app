#pragma once
#include <QMainWindow>
#include <QSettings>
#include <QProcess>
#include <QTabWidget>
#include <QSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QCheckBox>
#include "terminalwidget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void newTerminal();
    void closeTerminal(int index);
    void openBuildSettings();
    void runBuild();
    void runConfigure();
    void runInstall();
    void chooseDirectory();
    void showSystemInfo();
    void applyBuildSettings();

private:
    QTabWidget *tabs;
    QSettings settings;
    QLineEdit *projectDir;
    QComboBox *buildSystem;
    QLineEdit *makeFlags;
    QSpinBox *jobs;
    QSpinBox *ninjaJobs;
    QSpinBox *cargoJobs;
    QLineEdit *cmakeArgs;
    QLineEdit *mesonArgs;
    QLineEdit *cargoArgs;
    QLineEdit *extraEnv;
    QCheckBox *useNproc;
    QCheckBox *useCompilerCache;
    QCheckBox *verboseBuild;
    QWidget *buildPanel = nullptr;

    TerminalWidget *terminal() const;
    QString expandedJobs() const;
    QStringList environmentList() const;
    void runCommand(const QString &command, const QString &label);
    void updateTitle();
};
