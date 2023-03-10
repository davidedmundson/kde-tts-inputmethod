cmake_minimum_required(VERSION 3.16)
project(audiosource LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)

find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMPoQmTools)
include(ECMInstallIcons)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Multimedia Widgets)

set(BUILD_SHARED_LIBS NO)
add_subdirectory(whisper.cpp)

add_executable(testapp
    test.cpp
    processor.cpp
)

add_executable(audiosource
    audiosource.cpp
    processor.cpp
    main.cpp
)

target_link_libraries(audiosource PUBLIC
    Qt::Core
    Qt::Gui
    Qt::Multimedia
    Qt::Widgets
    ${CMAKE_CURRENT_BINARY_DIR}/whisper.cpp/libwhisper.a
)


target_link_libraries(testapp PUBLIC
    Qt::Core
    Qt::Gui
    Qt::Multimedia
    Qt::Widgets
    ${CMAKE_CURRENT_BINARY_DIR}/whisper.cpp/libwhisper.a
)
