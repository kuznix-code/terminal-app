#include "mainwindow.h"

#include <QApplication>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTabBar>
#include <QVBoxLayout>

namespace {
QString defaultProjectDirectory() {
    QDir dir(QDir::currentPath());

    if (dir.exists("meson.build") ||
        dir.exists("CMakeLists.txt") ||
        dir.exists("Cargo.toml") ||
        dir.exists("Makefile") ||
        dir.exists("build.ninja")) {
        return dir.absolutePath();
    }

    if (dir.dirName() == "build") {
        dir.cdUp();

        if (dir.exists("meson.build") ||
            dir.exists("CMakeLists.txt") ||
            dir.exists("Cargo.toml") ||
            dir.exists("Makefile")) {
            return dir.absolutePath();
        }
    }

    return QDir::homePath();
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      settings("Kuznix", "TerminalApp") {
    resize(1200, 760);
    setWindowTitle("Kuznix Terminal");

    const QString storedProject =
        settings.value("build/project", defaultProjectDirectory()).toString();

    if (storedProject.endsWith("/build") ||
        storedProject.endsWith("\\build")) {
        QDir candidate(storedProject);
        candidate.cdUp();

        if (QFileInfo::exists(candidate.filePath("meson.build")) ||
            QFileInfo::exists(candidate.filePath("CMakeLists.txt"))) {
            settings.setValue("build/project", candidate.absolutePath());
        }
    }

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(2, 2, 2, 2);

    tabs = new QTabWidget;
    tabs->setTabsClosable(true);
    tabs->setMovable(true);
    tabs->setDocumentMode(false);

    root->addWidget(tabs);
    setCentralWidget(central);

    // Deliberately old-school 2008/2009 KDE/Windows-style appearance.
    qApp->setStyleSheet(
        "QMainWindow,QDialog,QWidget{"
        "background:#d8d8d8;"
        "color:#202020;"
        "}"

        "QMenuBar{"
        "background:qlineargradient("
        "x1:0,y1:0,x2:0,y2:1,"
        "stop:0 #f6f6f6,"
        "stop:.45 #e7e7e7,"
        "stop:1 #c8c8c8);"
        "border:1px solid #8a8a8a;"
        "}"

        "QMenuBar::item{"
        "padding:4px 9px;"
        "border:1px solid transparent;"
        "}"

        "QMenuBar::item:selected{"
        "background:#c9d9ec;"
        "border:1px solid #7d9fc5;"
        "}"

        "QMenu{"
        "background:#f3f3f3;"
        "border:1px solid #777;"
        "padding:2px;"
        "}"

        "QMenu::item{"
        "padding:5px 24px 5px 8px;"
        "}"

        "QMenu::item:selected{"
        "background:#316ac5;"
        "color:white;"
        "}"

        "QTabWidget::pane{"
        "border:1px solid #737373;"
        "background:#252525;"
        "}"

        "QTabBar::tab{"
        "background:qlineargradient("
        "x1:0,y1:0,x2:0,y2:1,"
        "stop:0 #eeeeee,"
        "stop:1 #bcbcbc);"
        "border:1px solid #777;"
        "padding:5px 14px;"
        "margin-right:1px;"
        "}"

        "QTabBar::tab:selected{"
        "background:#f8f8f8;"
        "border-bottom-color:#f8f8f8;"
        "}"

        "QPushButton{"
        "background:qlineargradient("
        "x1:0,y1:0,x2:0,y2:1,"
        "stop:0 #ffffff,"
        "stop:1 #c9c9c9);"
        "border:1px solid #707070;"
        "border-radius:2px;"
        "padding:4px 12px;"
        "}"

        "QPushButton:hover{"
        "background:#e9f2fc;"
        "border-color:#4c7fb4;"
        "}"

        "QPushButton:pressed{"
        "background:#b9c9da;"
        "}"

        "QLineEdit,QComboBox,QSpinBox{"
        "background:white;"
        "border:1px solid #777;"
        "padding:3px;"
        "}"

        "QGroupBox{"
        "border:1px solid #8b8b8b;"
        "margin-top:9px;"
        "padding-top:8px;"
        "}"

        "QGroupBox::title{"
        "subcontrol-origin:margin;"
        "left:8px;"
        "padding:0 3px;"
        "}"

        "QStatusBar{"
        "background:#d0d0d0;"
        "border-top:1px solid #888;"
        "}"
    );

    auto *terminalMenu = menuBar()->addMenu("&Terminal");

    terminalMenu->addAction(
        "New terminal",
        QKeySequence("Ctrl+Shift+T"),
        this,
        &MainWindow::newTerminal);

    terminalMenu->addAction(
        "Close terminal",
        QKeySequence("Ctrl+Shift+W"),
        this,
        [this] {
            closeTerminal(tabs->currentIndex());
        });

    terminalMenu->addAction(
        "Restart shell",
        QKeySequence("Ctrl+Shift+R"),
        this,
        [this] {
            if (auto *t = terminal()) {
                t->stop();
                t->clear();
                t->start(
                    settings.value(
                        "terminal/shell",
                        qEnvironmentVariable("SHELL", "/bin/bash"))
                        .toString(),
                    projectPath());
            }
        });

    terminalMenu->addSeparator();

    terminalMenu->addAction(
        "Preferences",
        QKeySequence("Ctrl+,"),
        this,
        &MainWindow::openPreferences);

    terminalMenu->addSeparator();

    terminalMenu->addAction(
        "Quit",
        QKeySequence::Quit,
        qApp,
        &QApplication::quit);

    auto *developer = menuBar()->addMenu("&Developer");

    developer->addAction(
        "Build settings",
        QKeySequence("Ctrl+Shift+B"),
        this,
        &MainWindow::openBuildSettings);

    developer->addAction(
        "Configure project",
        QKeySequence("F5"),
        this,
        &MainWindow::runConfigure);

    developer->addAction(
        "Build",
        QKeySequence("F6"),
        this,
        &MainWindow::runBuild);

    developer->addAction(
        "Clean",
        QKeySequence("Ctrl+F6"),
        this,
        &MainWindow::runClean);

    developer->addAction(
        "Install",
        QKeySequence("Ctrl+F7"),
        this,
        &MainWindow::runInstall);

    developer->addSeparator();

    developer->addAction(
        "Show build environment",
        this,
        &MainWindow::showBuildEnvironment);

    developer->addAction(
        "System / toolchain information",
        this,
        &MainWindow::showSystemInfo);

    auto *view = menuBar()->addMenu("&View");

    view->addAction(
        "Clear terminal",
        this,
        [this] {
            if (terminal())
                terminal()->clear();
        });

    view->addAction(
        "Scroll to bottom",
        this,
        [this] {
            if (terminal())
                terminal()->moveCursor(QTextCursor::End);
        });

    view->addAction(
        "Preferences",
        this,
        &MainWindow::openPreferences);

    connect(
        tabs,
        &QTabWidget::tabCloseRequested,
        this,
        &MainWindow::closeTerminal);

    connect(
        tabs,
        &QTabWidget::currentChanged,
        this,
        &MainWindow::updateTitle);

    newTerminal();

    statusBar()->showMessage(
        "Ready — F6 = Build, F5 = Configure, Ctrl+Shift+B = Build Settings");
}

TerminalWidget *MainWindow::terminal() const {
    return dynamic_cast<TerminalWidget *>(tabs->currentWidget());
}

QString MainWindow::projectPath() const {
    QString path =
        settings.value(
            "build/project",
            defaultProjectDirectory())
            .toString()
            .trimmed();

    if (path.isEmpty())
        path = defaultProjectDirectory();

    QDir dir(path);

    return dir.exists()
               ? dir.absolutePath()
               : defaultProjectDirectory();
}

QString MainWindow::shellQuote(const QString &value) const {
    QString result = value;
    result.replace("'", "'\\''");
    return "'" + result + "'";
}

QString MainWindow::detectBuildSystem(const QString &dir) const {
    QDir d(dir);

    if (d.exists("meson.build"))
        return "Meson + Ninja";

    if (d.exists("CMakeLists.txt"))
        return "CMake + Ninja";

    if (d.exists("Cargo.toml"))
        return "Cargo";

    if (d.exists("build.ninja"))
        return "Ninja";

    if (d.exists("Makefile") || d.exists("makefile"))
        return "Make";

    return "Custom";
}

void MainWindow::newTerminal() {
    auto *t = new TerminalWidget;

    const QString shell =
        settings.value(
            "terminal/shell",
            qEnvironmentVariable("SHELL", "/bin/bash"))
            .toString();

    t->start(
        shell,
        settings.value(
            "terminal/dir",
            projectPath())
            .toString());

    const QColor bg(
        settings.value(
            "terminal/background",
            "#10151b")
            .toString());

    const QColor fg(
        settings.value(
            "terminal/foreground",
            "#e8edf2")
            .toString());

    const QColor sel(
        settings.value(
            "terminal/selection",
            "#316ac5")
            .toString());

    t->setTerminalColors(bg, fg, sel);

    // QFont::fromString() is an instance method.
    QFont terminalFont =
        QFontDatabase::systemFont(QFontDatabase::FixedFont);

    terminalFont.fromString(
        settings.value(
            "terminal/font",
            terminalFont.toString())
            .toString());

    t->setTerminalFont(terminalFont);

    t->setScrollback(
        settings.value(
            "terminal/scrollback",
            10000)
            .toInt());

    const int index =
        tabs->addTab(
            t,
            QString("Terminal %1")
                .arg(tabs->count() + 1));

    tabs->setCurrentIndex(index);

    t->setExitStatusCallback(
        [this](int code) {
            statusBar()->showMessage(
                QString("Shell exited with code %1")
                    .arg(code),
                4000);
        });

    updateTitle();
}

void MainWindow::closeTerminal(int index) {
    if (index < 0 || index >= tabs->count())
        return;

    QWidget *widget = tabs->widget(index);

    tabs->removeTab(index);
    widget->deleteLater();

    if (tabs->count() == 0)
        newTerminal();

    updateTitle();
}

void MainWindow::openBuildSettings() {
    if (buildPanel) {
        buildPanel->raise();
        buildPanel->activateWindow();
        return;
    }

    buildPanel = new QWidget(nullptr, Qt::Window);
    buildPanel->setAttribute(Qt::WA_DeleteOnClose);
    buildPanel->setWindowTitle(
        "Kuznix Terminal — Developer Build Settings");
    buildPanel->resize(780, 760);

    auto *root = new QVBoxLayout(buildPanel);

    auto *projectBox = new QGroupBox("Project");
    auto *pf = new QFormLayout(projectBox);

    auto *dirRow = new QWidget;
    auto *dl = new QHBoxLayout(dirRow);
    dl->setContentsMargins(0, 0, 0, 0);

    projectDir = new QLineEdit(projectPath());

    auto *browse = new QPushButton("Browse…");

    dl->addWidget(projectDir);
    dl->addWidget(browse);

    connect(
        browse,
        &QPushButton::clicked,
        this,
        &MainWindow::chooseDirectory);

    pf->addRow(
        "Source directory",
        dirRow);

    buildSystem = new QComboBox;

    buildSystem->addItems({
        "Auto detect",
        "Meson + Ninja",
        "CMake + Ninja",
        "CMake + Make",
        "Make",
        "Ninja",
        "Cargo",
        "Custom"
    });

    const QString configuredSystem =
        settings.value(
            "build/system",
            "Auto detect")
            .toString();

    buildSystem->setCurrentText(configuredSystem);

    pf->addRow(
        "Build system",
        buildSystem);

    buildDir =
        new QLineEdit(
            settings.value(
                "build/dir",
                "build")
                .toString());

    pf->addRow(
        "Build directory",
        buildDir);

    installPrefix =
        new QLineEdit(
            settings.value(
                "build/prefix",
                "")
                .toString());

    installPrefix->setPlaceholderText(
        "leave empty for project/system default");

    pf->addRow(
        "Install prefix",
        installPrefix);

    customCommand =
        new QLineEdit(
            settings.value(
                "build/custom",
                "")
                .toString());

    customCommand->setPlaceholderText(
        "e.g. ./build.sh -j$(nproc)");

    pf->addRow(
        "Custom build command",
        customCommand);

    root->addWidget(projectBox);

    auto *parallel = new QGroupBox("Parallelism");
    auto *pform = new QFormLayout(parallel);

    jobs = new QSpinBox;
    jobs->setRange(1, 1024);
    jobs->setValue(
        settings.value(
            "build/jobs",
            4)
            .toInt());

    ninjaJobs = new QSpinBox;
    ninjaJobs->setRange(1, 1024);
    ninjaJobs->setValue(
        settings.value(
            "build/ninja_jobs",
            4)
            .toInt());

    cargoJobs = new QSpinBox;
    cargoJobs->setRange(1, 1024);
    cargoJobs->setValue(
        settings.value(
            "build/cargo_jobs",
            4)
            .toInt());

    useNproc =
        new QCheckBox(
            "Use -j$(nproc) for the main job count");

    useNproc->setChecked(
        settings.value(
            "build/nproc",
            true)
            .toBool());

    pform->addRow(
        "Make / CMake jobs",
        jobs);

    pform->addRow(
        "Ninja jobs",
        ninjaJobs);

    pform->addRow(
        "Cargo jobs",
        cargoJobs);

    pform->addRow(
        "",
        useNproc);

    root->addWidget(parallel);

    auto *flags =
        new QGroupBox(
            "Compiler and build flags");

    auto *ff =
        new QFormLayout(flags);

    makeFlags =
        new QLineEdit(
            settings.value(
                "build/makeflags",
                "")
                .toString());

    makeFlags->setPlaceholderText(
        "extra Make flags");

    cflags =
        new QLineEdit(
            settings.value(
                "build/cflags",
                "")
                .toString());

    cxxflags =
        new QLineEdit(
            settings.value(
                "build/cxxflags",
                "")
                .toString());

    ldflags =
        new QLineEdit(
            settings.value(
                "build/ldflags",
                "")
                .toString());

    cmakeArgs =
        new QLineEdit(
            settings.value(
                "build/cmake_args",
                "-DCMAKE_BUILD_TYPE=Release")
                .toString());

    mesonArgs =
        new QLineEdit(
            settings.value(
                "build/meson_args",
                "--buildtype=release")
                .toString());

    cargoArgs =
        new QLineEdit(
            settings.value(
                "build/cargo_args",
                "--release")
                .toString());

    extraEnv =
        new QLineEdit(
            settings.value(
                "build/env",
                "")
                .toString());

    extraEnv->setPlaceholderText(
        "CC=gcc CXX=g++ MAKEFLAGS=…");

    ff->addRow(
        "Make flags",
        makeFlags);

    ff->addRow(
        "CFLAGS",
        cflags);

    ff->addRow(
        "CXXFLAGS",
        cxxflags);

    ff->addRow(
        "LDFLAGS",
        ldflags);

    ff->addRow(
        "CMake arguments",
        cmakeArgs);

    ff->addRow(
        "Meson arguments",
        mesonArgs);

    ff->addRow(
        "Cargo arguments",
        cargoArgs);

    ff->addRow(
        "Environment",
        extraEnv);

    root->addWidget(flags);

    auto *features =
        new QGroupBox("Build features");

    auto *ef =
        new QVBoxLayout(features);

    useCompilerCache =
        new QCheckBox(
            "Use ccache/sccache when available");

    useCompilerCache->setChecked(
        settings.value(
            "build/cache",
            false)
            .toBool());

    verboseBuild =
        new QCheckBox(
            "Verbose build output (-v where supported)");

    verboseBuild->setChecked(
        settings.value(
            "build/verbose",
            false)
            .toBool());

    autoConfigure =
        new QCheckBox(
            "Automatically configure when build directory is missing");

    autoConfigure->setChecked(
        settings.value(
            "build/auto_configure",
            true)
            .toBool());

    ef->addWidget(useCompilerCache);
    ef->addWidget(verboseBuild);
    ef->addWidget(autoConfigure);

    root->addWidget(features);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();

    auto *save =
        new QPushButton("Save & Apply");

    auto *cancel =
        new QPushButton("Close");

    buttons->addWidget(cancel);
    buttons->addWidget(save);

    root->addLayout(buttons);

    connect(
        save,
        &QPushButton::clicked,
        this,
        &MainWindow::applyBuildSettings);

    connect(
        cancel,
        &QPushButton::clicked,
        buildPanel,
        &QWidget::close);

    connect(
        buildPanel,
        &QObject::destroyed,
        this,
        [this] {
            buildPanel = nullptr;
        });

    buildPanel->show();
}

void MainWindow::chooseDirectory() {
    const QString d =
        QFileDialog::getExistingDirectory(
            buildPanel,
            "Select source directory",
            projectDir->text());

    if (!d.isEmpty())
        projectDir->setText(d);
}

void MainWindow::applyBuildSettings() {
    if (!buildPanel)
        return;

    settings.setValue(
        "build/project",
        projectDir->text().trimmed());

    settings.setValue(
        "build/system",
        buildSystem->currentText());

    settings.setValue(
        "build/dir",
        buildDir->text().trimmed());

    settings.setValue(
        "build/prefix",
        installPrefix->text().trimmed());

    settings.setValue(
        "build/custom",
        customCommand->text());

    settings.setValue(
        "build/jobs",
        jobs->value());

    settings.setValue(
        "build/ninja_jobs",
        ninjaJobs->value());

    settings.setValue(
        "build/cargo_jobs",
        cargoJobs->value());

    settings.setValue(
        "build/nproc",
        useNproc->isChecked());

    settings.setValue(
        "build/makeflags",
        makeFlags->text());

    settings.setValue(
        "build/cflags",
        cflags->text());

    settings.setValue(
        "build/cxxflags",
        cxxflags->text());

    settings.setValue(
        "build/ldflags",
        ldflags->text());

    settings.setValue(
        "build/cmake_args",
        cmakeArgs->text());

    settings.setValue(
        "build/meson_args",
        mesonArgs->text());

    settings.setValue(
        "build/cargo_args",
        cargoArgs->text());

    settings.setValue(
        "build/env",
        extraEnv->text());

    settings.setValue(
        "build/cache",
        useCompilerCache->isChecked());

    settings.setValue(
        "build/verbose",
        verboseBuild->isChecked());

    settings.setValue(
        "build/auto_configure",
        autoConfigure->isChecked());

    settings.sync();

    statusBar()->showMessage(
        "Build settings saved",
        3000);
}

void MainWindow::runCommand(
    const QString &command,
    const QString &label) {

    auto *t = terminal();

    if (!t) {
        newTerminal();
        t = terminal();
    }

    if (!t)
        return;

    QString env =
        settings.value(
            "build/env",
            "")
            .toString()
            .trimmed();

    if (settings.value(
            "build/cflags",
            "")
            .toString()
            .trimmed()
            .size()) {
        env +=
            " CFLAGS=" +
            shellQuote(
                settings.value(
                    "build/cflags")
                    .toString());
    }

    if (settings.value(
            "build/cxxflags",
            "")
            .toString()
            .trimmed()
            .size()) {
        env +=
            " CXXFLAGS=" +
            shellQuote(
                settings.value(
                    "build/cxxflags")
                    .toString());
    }

    if (settings.value(
            "build/ldflags",
            "")
            .toString()
            .trimmed()
            .size()) {
        env +=
            " LDFLAGS=" +
            shellQuote(
                settings.value(
                    "build/ldflags")
                    .toString());
    }

    if (settings.value(
            "build/cache",
            false)
            .toBool()) {
        env +=
            " CC='ccache ${CC:-gcc}'"
            " CXX='ccache ${CXX:-g++}'";
    }

    const QString dir =
        shellQuote(projectPath());

    const QString banner =
        "echo '==== Kuznix Terminal :: " +
        label +
        " ===='";

    const QString wrapped =
        "cd -- " +
        dir +
        " && " +
        (env.isEmpty()
             ? QString()
             : "export " + env + " && ") +
        banner +
        " && " +
        command;

    t->sendCommand(wrapped);

    statusBar()->showMessage(
        label + " started",
        2000);
}

void MainWindow::runConfigure() {
    const QString dir = projectPath();

    QString system =
        settings.value(
            "build/system",
            "Auto detect")
            .toString();

    if (system == "Auto detect")
        system = detectBuildSystem(dir);

    const QString bdValue =
        settings.value(
            "build/dir",
            "build")
            .toString()
            .trimmed();

    const QString bd =
        bdValue.isEmpty()
            ? "build"
            : bdValue;

    const QString b =
        shellQuote(bd);

    if (system == "Meson + Ninja") {
        runCommand(
            "if [ -f " + b +
            "/build.ninja ]; then meson setup " +
            b + " " +
            settings.value(
                "build/meson_args",
                "--buildtype=release")
                .toString() +
            " --reconfigure; else meson setup " +
            b + " " +
            settings.value(
                "build/meson_args",
                "--buildtype=release")
                .toString() +
            "; fi",
            "Meson configure");

    } else if (
        system == "CMake + Ninja" ||
        system == "CMake + Make") {

        const QString generator =
            system == "CMake + Ninja"
                ? "Ninja"
                : "Unix Makefiles";

        runCommand(
            "cmake -S . -B " +
            b +
            " -G " +
            shellQuote(generator) +
            " " +
            settings.value(
                "build/cmake_args",
                "-DCMAKE_BUILD_TYPE=Release")
                .toString(),
            "CMake configure");

    } else if (system == "Cargo") {
        runCommand(
            "cargo metadata --no-deps "
            "--format-version 1 >/dev/null "
            "&& echo 'Cargo project OK'",
            "Cargo check");
    } else {
        statusBar()->showMessage(
            "No configure step is required for " +
                system,
            3000);
    }
}

void MainWindow::runBuild() {
    QString system =
        settings.value(
            "build/system",
            "Auto detect")
            .toString();

    if (system == "Auto detect")
        system = detectBuildSystem(projectPath());

    const QString bdValue =
        settings.value(
            "build/dir",
            "build")
            .toString()
            .trimmed();

    const QString bd =
        bdValue.isEmpty()
            ? "build"
            : bdValue;

    const QString b =
        shellQuote(bd);

    const bool nproc =
        settings.value(
            "build/nproc",
            true)
            .toBool();

    const QString jobsArg =
        nproc
            ? "$(nproc)"
            : QString::number(
                  settings.value(
                      "build/jobs",
                      4)
                      .toInt());

    const QString ninjaArg =
        QString::number(
            settings.value(
                "build/ninja_jobs",
                4)
                .toInt());

    const QString cargoArg =
        QString::number(
            settings.value(
                "build/cargo_jobs",
                4)
                .toInt());

    const QString verbose =
        settings.value(
            "build/verbose",
            false)
            .toBool()
            ? " -v"
            : "";

    const bool autoConfig =
        settings.value(
            "build/auto_configure",
            true)
            .toBool();

    QString command;

    if (system == "Meson + Ninja") {
        const QString configure =
            "meson setup " +
            b +
            " " +
            settings.value(
                "build/meson_args",
                "--buildtype=release")
                .toString();

        command =
            (autoConfig
                 ? "if [ ! -f " +
                   b +
                   "/build.ninja ]; then " +
                   configure +
                   "; fi && "
                 : "") +
            "meson compile -C " +
            b +
            " -j " +
            ninjaArg +
            verbose;

    } else if (
        system == "CMake + Ninja" ||
        system == "CMake + Make") {

        const QString generator =
            system == "CMake + Ninja"
                ? "Ninja"
                : "Unix Makefiles";

        const QString configure =
            "cmake -S . -B " +
            b +
            " -G " +
            shellQuote(generator) +
            " " +
            settings.value(
                "build/cmake_args",
                "-DCMAKE_BUILD_TYPE=Release")
                .toString();

        command =
            (autoConfig
                 ? "if [ ! -f " +
                   b +
                   "/CMakeCache.txt ]; then " +
                   configure +
                   "; fi && "
                 : "") +
            "cmake --build " +
            b +
            " --parallel " +
            jobsArg +
            verbose;

    } else if (system == "Ninja") {
        command =
            "ninja -C " +
            b +
            " -j " +
            ninjaArg +
            verbose;

    } else if (system == "Cargo") {
        command =
            "cargo build -j " +
            cargoArg +
            " " +
            settings.value(
                "build/cargo_args",
                "--release")
                .toString();

    } else if (system == "Make") {
        command =
            "make -j" +
            jobsArg +
            " " +
            settings.value(
                "build/makeflags",
                "")
                .toString();

    } else {
        command =
            settings.value(
                "build/custom",
                "")
                .toString()
                .trimmed();

        if (command.isEmpty()) {
            command =
                "echo 'No custom build command configured.'; false";
        }
    }

    runCommand(
        command,
        "Build");
}

void MainWindow::runClean() {
    QString system =
        settings.value(
            "build/system",
            "Auto detect")
            .toString();

    if (system == "Auto detect")
        system = detectBuildSystem(projectPath());

    const QString bdValue =
        settings.value(
            "build/dir",
            "build")
            .toString()
            .trimmed();

    const QString bd =
        bdValue.isEmpty()
            ? "build"
            : bdValue;

    const QString b =
        shellQuote(bd);

    QString command;

    if (system == "Meson + Ninja") {
        command =
            "ninja -C " +
            b +
            " clean";

    } else if (
        system == "CMake + Ninja" ||
        system == "CMake + Make") {

        command =
            "cmake --build " +
            b +
            " --target clean";

    } else if (system == "Ninja") {
        command =
            "ninja -C " +
            b +
            " clean";

    } else if (system == "Cargo") {
        command = "cargo clean";

    } else if (system == "Make") {
        command = "make clean";

    } else {
        command =
            "echo 'No automatic clean command for this build system.'";
    }

    runCommand(
        command,
        "Clean");
}

void MainWindow::runInstall() {
    QString system =
        settings.value(
            "build/system",
            "Auto detect")
            .toString();

    if (system == "Auto detect")
        system = detectBuildSystem(projectPath());

    const QString bdValue =
        settings.value(
            "build/dir",
            "build")
            .toString()
            .trimmed();

    const QString bd =
        bdValue.isEmpty()
            ? "build"
            : bdValue;

    const QString b =
        shellQuote(bd);

    QString command;

    if (system == "Meson + Ninja") {
        command =
            "meson install -C " +
            b;

    } else if (
        system == "CMake + Ninja" ||
        system == "CMake + Make") {

        command =
            "cmake --install " +
            b;

    } else if (system == "Cargo") {
        command =
            "cargo install --path .";

    } else if (system == "Make") {
        command =
            "make install";

    } else {
        command =
            "echo 'No automatic install command for this build system.'";
    }

    const QString prefix =
        settings.value(
            "build/prefix",
            "")
            .toString()
            .trimmed();

    if (!prefix.isEmpty() &&
        system.startsWith("CMake")) {
        command +=
            " --prefix " +
            shellQuote(prefix);
    }

    runCommand(
        command,
        "Install");
}

void MainWindow::showSystemInfo() {
    runCommand(
        "printf '%s\\n' 'CPU jobs:' \"$(nproc)\"; "
        "printf '%s\\n' 'Compiler:' \"$(${CXX:-c++} "
        "--version 2>/dev/null | head -n1)\"; "
        "printf '%s\\n' 'CMake:' \"$(cmake --version "
        "2>/dev/null | head -n1)\"; "
        "printf '%s\\n' 'Meson:' \"$(meson --version "
        "2>/dev/null)\"; "
        "printf '%s\\n' 'Ninja:' \"$(ninja --version "
        "2>/dev/null)\"; "
        "printf '%s\\n' 'Cargo:' \"$(cargo --version "
        "2>/dev/null)\"; "
        "printf '%s\\n' 'Make:' \"$(make --version "
        "2>/dev/null | head -n1)\"; "
        "printf '%s\\n' 'Ccache:' \"$(ccache --version "
        "2>/dev/null | head -n1)\"",
        "Toolchain information");
}

void MainWindow::showBuildEnvironment() {
    runCommand(
        "printf '%s\\n' '--- Build environment ---'; "
        "pwd; "
        "printf '%s\\n' "
        "\"CFLAGS=$CFLAGS\" "
        "\"CXXFLAGS=$CXXFLAGS\" "
        "\"LDFLAGS=$LDFLAGS\" "
        "\"MAKEFLAGS=$MAKEFLAGS\"; "
        "printf '%s\\n' 'PATH:' \"$PATH\"; "
        "printf '%s\\n' 'Build system:' "
        "\"$(if [ -f meson.build ]; then "
        "echo Meson; "
        "elif [ -f CMakeLists.txt ]; then "
        "echo CMake; "
        "elif [ -f Cargo.toml ]; then "
        "echo Cargo; "
        "elif [ -f Makefile ]; then "
        "echo Make; "
        "else echo Custom; fi)\"",
        "Build environment");
}

void MainWindow::openPreferences() {
    QDialog dialog(this);

    dialog.setWindowTitle(
        "Kuznix Terminal Preferences");

    dialog.resize(620, 430);

    auto *root =
        new QVBoxLayout(&dialog);

    auto *appearance =
        new QGroupBox(
            "Terminal appearance");

    auto *form =
        new QFormLayout(appearance);

    auto *preset =
        new QComboBox;

    preset->addItems({
        "2009 Classic",
        "Midnight Blue",
        "Green Screen",
        "Custom"
    });

    preset->setCurrentText(
        settings.value(
            "terminal/preset",
            "2009 Classic")
            .toString());

    auto *fontButton =
        new QPushButton("Choose font…");

    auto *bgButton =
        new QPushButton("Background…");

    auto *fgButton =
        new QPushButton("Text…");

    auto *selButton =
        new QPushButton("Selection…");

    auto *scrollback =
        new QSpinBox;

    scrollback->setRange(
        1000,
        1000000);

    scrollback->setValue(
        settings.value(
            "terminal/scrollback",
            10000)
            .toInt());

    auto *shell =
        new QLineEdit(
            settings.value(
                "terminal/shell",
                qEnvironmentVariable(
                    "SHELL",
                    "/bin/bash"))
                .toString());

    auto *startDir =
        new QLineEdit(
            settings.value(
                "terminal/dir",
                projectPath())
                .toString());

    form->addRow(
        "Preset",
        preset);

    form->addRow(
        "Font",
        fontButton);

    form->addRow(
        "Background",
        bgButton);

    form->addRow(
        "Text",
        fgButton);

    form->addRow(
        "Selection",
        selButton);

    form->addRow(
        "Scrollback lines",
        scrollback);

    form->addRow(
        "Shell",
        shell);

    form->addRow(
        "Startup directory",
        startDir);

    root->addWidget(appearance);

    auto *hint =
        new QLabel(
            "The 2009 Classic preset uses a beveled "
            "grey desktop-style interface while keeping "
            "the terminal itself dark. Changes are saved locally.");

    hint->setWordWrap(true);

    root->addWidget(hint);

    auto *buttons =
        new QDialogButtonBox(
            QDialogButtonBox::Ok |
            QDialogButtonBox::Cancel);

    root->addWidget(buttons);

    QColor bg(
        settings.value(
            "terminal/background",
            "#10151b")
            .toString());

    QColor fg(
        settings.value(
            "terminal/foreground",
            "#e8edf2")
            .toString());

    QColor sel(
        settings.value(
            "terminal/selection",
            "#316ac5")
            .toString());

    // Correct QFont loading.
    QFont font =
        QFontDatabase::systemFont(
            QFontDatabase::FixedFont);

    font.fromString(
        settings.value(
            "terminal/font",
            font.toString())
            .toString());

    connect(
        fontButton,
        &QPushButton::clicked,
        &dialog,
        [&] {
            bool ok = false;

            const QFont f =
                QFontDialog::getFont(
                    &ok,
                    font,
                    &dialog,
                    "Terminal font");

            if (ok)
                font = f;
        });

    connect(
        bgButton,
        &QPushButton::clicked,
        &dialog,
        [&] {
            const QColor c =
                QColorDialog::getColor(
                    bg,
                    &dialog,
                    "Terminal background");

            if (c.isValid())
                bg = c;
        });

    connect(
        fgButton,
        &QPushButton::clicked,
        &dialog,
        [&] {
            const QColor c =
                QColorDialog::getColor(
                    fg,
                    &dialog,
                    "Terminal text");

            if (c.isValid())
                fg = c;
        });

    connect(
        selButton,
        &QPushButton::clicked,
        &dialog,
        [&] {
            const QColor c =
                QColorDialog::getColor(
                    sel,
                    &dialog,
                    "Selection color");

            if (c.isValid())
                sel = c;
        });

    connect(
        preset,
        &QComboBox::currentTextChanged,
        &dialog,
        [&](const QString &p) {
            if (p == "2009 Classic") {
                bg = QColor("#10151b");
                fg = QColor("#e8edf2");
                sel = QColor("#316ac5");
                font =
                    QFontDatabase::systemFont(
                        QFontDatabase::FixedFont);

            } else if (p == "Midnight Blue") {
                bg = QColor("#08111f");
                fg = QColor("#cfe7ff");
                sel = QColor("#205ea8");

            } else if (p == "Green Screen") {
                bg = QColor("#071107");
                fg = QColor("#74ff74");
                sel = QColor("#1c6b1c");
            }
        });

    connect(
        buttons,
        &QDialogButtonBox::accepted,
        &dialog,
        &QDialog::accept);

    connect(
        buttons,
        &QDialogButtonBox::rejected,
        &dialog,
        &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    settings.setValue(
        "terminal/preset",
        preset->currentText());

    settings.setValue(
        "terminal/background",
        bg.name());

    settings.setValue(
        "terminal/foreground",
        fg.name());

    settings.setValue(
        "terminal/selection",
        sel.name());

    settings.setValue(
        "terminal/font",
        font.toString());

    settings.setValue(
        "terminal/scrollback",
        scrollback->value());

    settings.setValue(
        "terminal/shell",
        shell->text().trimmed());

    settings.setValue(
        "terminal/dir",
        startDir->text().trimmed());

    settings.sync();

    applyTerminalSettings();
}

void MainWindow::applyTerminalSettings() {
    applyThemeToAllTerminals();

    for (int i = 0; i < tabs->count(); ++i) {
        if (auto *t =
                dynamic_cast<TerminalWidget *>(
                    tabs->widget(i))) {

            // Correct QFont loading.
            QFont terminalFont =
                QFontDatabase::systemFont(
                    QFontDatabase::FixedFont);

            terminalFont.fromString(
                settings.value(
                    "terminal/font")
                    .toString());

            t->setTerminalFont(
                terminalFont);

            t->setScrollback(
                settings.value(
                    "terminal/scrollback",
                    10000)
                    .toInt());
        }
    }
}

void MainWindow::applyThemeToAllTerminals() {
    const QColor bg(
        settings.value(
            "terminal/background",
            "#10151b")
            .toString());

    const QColor fg(
        settings.value(
            "terminal/foreground",
            "#e8edf2")
            .toString());

    const QColor sel(
        settings.value(
            "terminal/selection",
            "#316ac5")
            .toString());

    for (int i = 0; i < tabs->count(); ++i) {
        if (auto *t =
                dynamic_cast<TerminalWidget *>(
                    tabs->widget(i))) {

            t->setTerminalColors(
                bg,
                fg,
                sel);
        }
    }
}

void MainWindow::updateTitle() {
    setWindowTitle(
        QString(
            "Kuznix Terminal — %1 tabs — %2")
            .arg(tabs->count())
            .arg(projectPath()));
}
