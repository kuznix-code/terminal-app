project('kuznix-terminal', 'cpp', version: '1.0.0', default_options: ['cpp_std=c++20', 'warning_level=3'])

qt6 = import('qt6')
qt = dependency('qt6', modules: ['Core', 'Gui', 'Widgets'])

sources = files('src/main.cpp', 'src/mainwindow.cpp', 'src/terminalwidget.cpp')
headers = files('src/mainwindow.h', 'src/terminalwidget.h')

executable('kuznix-terminal', sources + headers,
  dependencies: qt,
  install: true,
)

install_data('README.md', install_dir: get_option('datadir') / 'doc' / 'kuznix-terminal')
