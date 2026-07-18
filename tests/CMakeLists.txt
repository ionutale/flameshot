# Links test against pre-built flameshot object files.
# Requires a successful "cmake --build build --target flameshot" first.

cmake_minimum_required(VERSION 3.22)
project(flameshot_tests LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Test Svg Network)

# Collect all flameshot .o files from a prior build
file(GLOB_RECURSE FLAMESHOT_O
    ${CMAKE_SOURCE_DIR}/../build/src/CMakeFiles/flameshot.dir/*.o
)
file(GLOB_RECURSE EXTERNAL_O
    ${CMAKE_SOURCE_DIR}/../build/_deps/qtcolorwidgets-build/CMakeFiles/QtColorWidgets.dir/*.o
)
# Exclude main.cpp.o from the object list (test has its own main)
list(FILTER FLAMESHOT_O EXCLUDE REGEX "main\.cpp\.o$")

add_executable(test_tools test_tools.cpp ${FLAMESHOT_O} ${EXTERNAL_O})

# Include paths from the main build
target_include_directories(test_tools PRIVATE
    ${CMAKE_SOURCE_DIR}/../src
    ${CMAKE_SOURCE_DIR}/../build/src
    ${CMAKE_SOURCE_DIR}/../build/src/flameshot_autogen/include
    ${CMAKE_SOURCE_DIR}/../build/_deps/qtcolorwidgets-build
    ${CMAKE_SOURCE_DIR}/../build/_deps/qtcolorwidgets-src/include
)

# Match compile definitions from main build
target_compile_definitions(test_tools PRIVATE
    PREDEFINED_COLOR_PALETTE_LARGE=false
    APP_PREFIX="/usr/local"
    APP_VERSION="v14.0.0"
    FLAMESHOT_GIT_HASH="test"
    USE_KDSINGLEAPPLICATION=1
)

target_link_libraries(test_tools PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test Qt6::Svg Qt6::Network
    ${CMAKE_SOURCE_DIR}/../build/_deps/qhotkey-build/libqhotkey.a
    ${CMAKE_SOURCE_DIR}/../build/_deps/kdsingleapplication-build/src/libkdsingleapplication-qt6.a
)

if(APPLE)
    # QHotKey on macOS uses Carbon framework
    find_library(CARBON_LIBRARY Carbon)
    target_link_libraries(test_tools PRIVATE ${CARBON_LIBRARY})
endif()

# Add qhotkey include path
target_include_directories(test_tools PRIVATE
    ${CMAKE_SOURCE_DIR}/../build/_deps/qhotkey-build
    ${CMAKE_SOURCE_DIR}/../build/_deps/qhotkey-src
)

add_test(NAME test_tools COMMAND test_tools)
