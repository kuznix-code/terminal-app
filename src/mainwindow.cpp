#include "mainwindow.h"
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QPushButton>
#include <QStatusBar>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), settings("Kuznix", "TerminalApp") {
    setWindowTitle("Kuznix Terminal"); resize(1200, 760);
    auto *central = new QWidget(this); auto *root = new QVBoxLayout(central); root->setContentsMargins(0,0,0,0);
    tabs = new QTabWidget; tabs->setTabsClosable(true); tabs->setMovable(true); root->addWidget(tabs); setCentralWidget(central);
    auto *file = menuBar()->addMenu("&Terminal");
    file->addAction("New terminal", QKeySequence("Ctrl+Shift+T"), this, &MainWindow::newTerminal);
    file->addAction("Close terminal", QKeySequence("Ctrl+Shift+W"), this, [this]{ closeTerminal(tabs->currentIndex()); });
    file->addSeparator(); file->addAction("Quit", QKeySequence::Quit, qApp, &QApplication::quit);
    auto *dev = menuBar()->addMenu("&Developer");
    dev->addAction("Build settings", QKeySequence("Ctrl+Shift+B"), this, &MainWindow::openBuildSettings);
    dev->addAction("Configure", this, &MainWindow::runConfigure); dev->addAction("Build", QKeySequence("F6"), this, &MainWindow::runBuild);
    dev->addAction("Install", this, &MainWindow::runInstall); dev->addAction("System / toolchain info", this, &MainWindow::showSystemInfo);
    auto *view = menuBar()->addMenu("&View");
    view->addAction("Clear terminal", this, [this]{ if (terminal()) terminal()->clear(); });
    view->addAction("Reset shell", this, [this]{ if (terminal()) { terminal()->stop(); terminal()->start(qEnvironmentVariable("SHELL","/bin/sh"), QDir::homePath()); }});
    connect(tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTerminal); connect(tabs, &QTabWidget::currentChanged, this, &MainWindow::updateTitle);
    newTerminal(); statusBar()->showMessage("Ready — F6 builds with the configured developer settings");
}

TerminalWidget *MainWindow::terminal() const { return dynamic_cast<TerminalWidget*>(tabs->currentWidget()); }

void MainWindow::newTerminal() {
    auto *t = new TerminalWidget; t->start(qEnvironmentVariable("SHELL", "/bin/bash"), settings.value("terminal/dir", QDir::homePath()).toString());
    int i = tabs->addTab(t, "Terminal"); tabs->setCurrentIndex(i);
    t->setExitStatusCallback([this](int code){ statusBar()->showMessage(QString("Shell exited with code %1").arg(code), 4000); }); updateTitle();
}
void MainWindow::closeTerminal(int index) { if (index < 0) return; QWidget *w = tabs->widget(index); tabs->removeTab(index); w->deleteLater(); if (tabs->count() == 0) newTerminal(); }
QString MainWindow::expandedJobs() const { return useNproc && useNproc->isChecked() ? "$(nproc)" : QString::number(jobs ? jobs->value() : 4); }
QStringList MainWindow::environmentList() const { return (extraEnv ? extraEnv->text() : QString()).split(' ', Qt::SkipEmptyParts); }

void MainWindow::openBuildSettings() {
    if (buildPanel) { buildPanel->raise(); buildPanel->activateWindow(); return; }
    buildPanel = new QWidget(nullptr, Qt::Window); buildPanel->setAttribute(Qt::WA_DeleteOnClose); buildPanel->setWindowTitle("Developer Build Settings — Kuznix Terminal"); buildPanel->resize(720, 620);
    auto *root = new QVBoxLayout(buildPanel); auto *projectBox = new QGroupBox("Project / build system"); auto *pf = new QFormLayout(projectBox);
    auto *dirRow = new QWidget; auto *dl = new QHBoxLayout(dirRow); dl->setContentsMargins(0,0,0,0); projectDir = new QLineEdit(settings.value("build/project", QDir::currentPath()).toString()); auto *browse = new QPushButton("Browse…"); dl->addWidget(projectDir); dl->addWidget(browse); connect(browse, &QPushButton::clicked, this, &MainWindow::chooseDirectory); pf->addRow("Project directory", dirRow);
    buildSystem = new QComboBox; buildSystem->addItems({"Auto detect", "Meson + Ninja", "CMake + Ninja", "CMake + Make", "Make", "Ninja", "Cargo", "Custom"}); buildSystem->setCurrentText(settings.value("build/system", "Auto detect").toString()); pf->addRow("Build system", buildSystem); root->addWidget(projectBox);
    auto *parallel = new QGroupBox("Parallelism / scheduler"); auto *pform = new QFormLayout(parallel); jobs = new QSpinBox; jobs->setRange(1,512); jobs->setValue(settings.value("build/jobs",4).toInt()); ninjaJobs = new QSpinBox; ninjaJobs->setRange(1,512); ninjaJobs->setValue(settings.value("build/ninja_jobs",4).toInt()); cargoJobs = new QSpinBox; cargoJobs->setRange(1,512); cargoJobs->setValue(settings.value("build/cargo_jobs",4).toInt()); useNproc = new QCheckBox("Use $(nproc) instead of fixed jobs"); useNproc->setChecked(settings.value("build/nproc",true).toBool()); pform->addRow("Make / CMake jobs",jobs); pform->addRow("Ninja jobs",ninjaJobs); pform->addRow("Cargo jobs",cargoJobs); pform->addRow("",useNproc); root->addWidget(parallel);
    auto *flags = new QGroupBox("Compiler / build flags"); auto *ff = new QFormLayout(flags); makeFlags = new QLineEdit(settings.value("build/makeflags","-O2 -pipe").toString()); cmakeArgs = new QLineEdit(settings.value("build/cmake_args","-DCMAKE_BUILD_TYPE=Release").toString()); mesonArgs = new QLineEdit(settings.value("build/meson_args","--buildtype=release").toString()); cargoArgs = new QLineEdit(settings.value("build/cargo_args","--release").toString()); extraEnv = new QLineEdit(settings.value("build/env","CC=gcc CXX=g++").toString()); ff->addRow("C/C++ flags",makeFlags); ff->addRow("CMake arguments",cmakeArgs); ff->addRow("Meson arguments",mesonArgs); ff->addRow("Cargo arguments",cargoArgs); ff->addRow("Environment",extraEnv); root->addWidget(flags);
    auto *extras = new QGroupBox("Developer features"); auto *ef = new QVBoxLayout(extras); useCompilerCache = new QCheckBox("Enable compiler cache (ccache/sccache when available)"); useCompilerCache->setChecked(settings.value("build/cache",false).toBool()); verboseBuild = new QCheckBox("Verbose compiler/build output"); verboseBuild->setChecked(settings.value("build/verbose",false).toBool()); ef->addWidget(useCompilerCache); ef->addWidget(verboseBuild); root->addWidget(extras);
    auto *buttons = new QHBoxLayout; auto *save = new QPushButton("Save & Apply"); auto *cancel = new QPushButton("Close"); buttons->addStretch(); buttons->addWidget(cancel); buttons->addWidget(save); root->addLayout(buttons); connect(save,&QPushButton::clicked,this,&MainWindow::applyBuildSettings); connect(cancel,&QPushButton::clicked,buildPanel,&QWidget::close); connect(buildPanel,&QObject::destroyed,this,[this]{buildPanel=nullptr;}); buildPanel->show();
}
void MainWindow::chooseDirectory() { const QString d=QFileDialog::getExistingDirectory(buildPanel,"Select project directory",projectDir->text()); if(!d.isEmpty()) projectDir->setText(d); }
void MainWindow::applyBuildSettings() { settings.setValue("build/project",projectDir->text()); settings.setValue("build/system",buildSystem->currentText()); settings.setValue("build/jobs",jobs->value()); settings.setValue("build/ninja_jobs",ninjaJobs->value()); settings.setValue("build/cargo_jobs",cargoJobs->value()); settings.setValue("build/nproc",useNproc->isChecked()); settings.setValue("build/makeflags",makeFlags->text()); settings.setValue("build/cmake_args",cmakeArgs->text()); settings.setValue("build/meson_args",mesonArgs->text()); settings.setValue("build/cargo_args",cargoArgs->text()); settings.setValue("build/env",extraEnv->text()); settings.setValue("build/cache",useCompilerCache->isChecked()); settings.setValue("build/verbose",verboseBuild->isChecked()); settings.sync(); statusBar()->showMessage("Build settings saved",3000); }
void MainWindow::runCommand(const QString &command,const QString &label) { if(!terminal()) newTerminal(); const QString dir=settings.value("build/project",QDir::currentPath()).toString(); QString env=settings.value("build/env","").toString(); QString wrapped="cd '"+dir+"' && "; if(!env.isEmpty()) wrapped+=env+" "; wrapped+=command; terminal()->sendCommand("printf '\\n\\033[1;36m== "+label+" ==\\033[0m\\n' && "+wrapped); }
void MainWindow::runConfigure() { const QString sys=settings.value("build/system","Auto detect").toString(); if(sys=="Meson + Ninja"||sys=="Auto detect") runCommand("meson setup build "+settings.value("build/meson_args","--buildtype=release").toString()+" --reconfigure","Meson configure"); else if(sys.startsWith("CMake")) runCommand("cmake -S . -B build -G "+QString(sys.contains("Ninja")?"Ninja":"Unix Makefiles")+" "+settings.value("build/cmake_args","-DCMAKE_BUILD_TYPE=Release").toString(),"CMake configure"); else if(sys=="Cargo") runCommand("cargo metadata --no-deps","Cargo check"); else statusBar()->showMessage("No configure step is required",3000); }
void MainWindow::runBuild() { const QString sys=settings.value("build/system","Auto detect").toString(); const QString j=useNproc&&useNproc->isChecked()?"$(nproc)":QString::number(jobs?jobs->value():settings.value("build/jobs",4).toInt()); const QString verbose=(verboseBuild&&verboseBuild->isChecked())?" -v":""; QString cmd; if(sys=="Meson + Ninja"||sys=="Auto detect") cmd="ninja -C build -j "+QString::number(ninjaJobs?ninjaJobs->value():4)+verbose; else if(sys=="CMake + Ninja"||sys=="CMake + Make") cmd="cmake --build build --parallel "+j+verbose; else if(sys=="Make") cmd="make -j"+j+" "+settings.value("build/makeflags","").toString(); else if(sys=="Ninja") cmd="ninja -j "+QString::number(ninjaJobs?ninjaJobs->value():4)+verbose; else if(sys=="Cargo") cmd="cargo build -j "+QString::number(cargoJobs?cargoJobs->value():4)+" "+settings.value("build/cargo_args","--release").toString(); else cmd="make -j"+j; runCommand(cmd,"Build"); }
void MainWindow::runInstall() { const QString sys=settings.value("build/system","Auto detect").toString(); if(sys.startsWith("CMake")||sys=="Meson + Ninja"||sys=="Auto detect") runCommand(sys.startsWith("CMake")?"cmake --install build":"meson install -C build","Install"); else if(sys=="Cargo") runCommand("cargo install --path .","Cargo install"); else runCommand("sudo make install","Install"); }
void MainWindow::showSystemInfo() { runCommand("printf 'CPU jobs: '; nproc; printf '\\nCompiler: '; ${CXX:-c++} --version | head -n1; printf 'CMake: '; (cmake --version|head -n1) 2>/dev/null||true; printf 'Meson: '; (meson --version) 2>/dev/null||true; printf 'Ninja: '; (ninja --version) 2>/dev/null||true; printf 'Cargo: '; (cargo --version) 2>/dev/null||true; printf 'Make: '; (make --version|head -n1) 2>/dev/null||true","Toolchain information"); }
void MainWindow::updateTitle() { setWindowTitle(QString("Kuznix Terminal — %1 tabs").arg(tabs->count())); }
