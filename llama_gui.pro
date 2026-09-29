QT += core gui widgets

TARGET   = llama_gui
TEMPLATE = app
CONFIG  += c++17

# Apple clang 21+ 要求 __yield 有显式声明（Qt 的 qyieldcpu.h 未包含 <arm_acle.h>）
macx-clang {
    MARCH = $$system(uname -m)
    equals(MARCH, arm64): QMAKE_CXXFLAGS += -include arm_acle.h
}

SOURCES += src/main.cpp src/mainwindow.cpp
HEADERS += src/mainwindow.h
RESOURCES += resources.qrc
