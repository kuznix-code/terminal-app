#pragma once

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFont>
#include <QLineEdit>
#include <QMainWindow>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>

#include "terminalwidget.h"

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    QTabWidget *tabs = nullptr;
    QSettings settings;

    QLineEdit *projectDir = nullptr;
    QComboBox *buildSystem = nullptr;
    QSpinBox *jobs = nullptr;
    QSpinBox *ninjaJobs = nullptr;
    QSpinBox *cargoJobs = nullptr;
    QLineEdit *makeFlags = nullptr;
    QLineEdit *cmakeArgs = nullptr;
    QLineEdit *mesonArgs = nullptr;
    QLineEdit *cargoArgs = nullptr;
    QLineEdit *customCommand = nullptr;
    QLineEdit *extraEnv = nullptr;
    QLineEdit *cflags = nullptr;
    QLineEdit *cxxflags = nullptr;
    QLineEdit *ldflags = nullptr;
    QLineEdit *installPrefix = nullptr;
    QLineEdit *buildDir = nullptr;
    QCheckBox *useNproc = nullptr;
    QCheckBox *useCompilerCache = nullptr;
    QCheckBox *verboseBuild = nullptr;
    QCheckBox *autoConfigure = nullptr;
    QWidget *buildPanel = nullptr;

    TerminalWidget *terminal() const;
    QString projectPath() const;
    QString shellQuote(const QString &value) const;
    QString detectBuildSystem(const QString &dir) const;
    void newTerminal();
    void closeTerminal(int index);
    void openBuildSettings();
    void openPreferences();
    void runBuild();
    void runConfigure();
    void runInstall();
    void runClean();
    void chooseDirectory();
    void showSystemInfo();
    void showBuildEnvironment();
    void applyBuildSettings();
    void applyTerminalSettings();
    void runCommand(const QString &command, const QString &label);
    void updateTitle();
    void applyThemeToAllTerminals();
};
