cmake_minimum_required(VERSION 3.16)
project(mpq-viewer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# StormLib (MIT) is built from source and linked statically.
set(STORMLIB_DIR "${CMAKE_SOURCE_DIR}/third_party/StormLib" CACHE PATH "Path to the StormLib source tree")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(STORM_SKIP_INSTALL ON CACHE BOOL "" FORCE)
set(STORM_USE_BUNDLED_LIBRARIES ON CACHE BOOL "" FORCE)
add_subdirectory(${STORMLIB_DIR} stormlib EXCLUDE_FROM_ALL)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(mpq-viewer
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/mpqarchive.cpp
    src/mpqarchive.h
    src/mpqmodel.cpp
    src/mpqmodel.h
    src/blp.cpp
    src/blp.h
)
target_include_directories(mpq-viewer PRIVATE ${STORMLIB_DIR}/src)
target_link_libraries(mpq-viewer PRIVATE Qt6::Widgets storm)
target_compile_definitions(mpq-viewer PRIVATE APP_VERSION="${PROJECT_VERSION}")

install(TARGETS mpq-viewer RUNTIME DESTINATION bin)
install(FILES packaging/mpq-viewer.desktop DESTINATION share/applications)
install(FILES packaging/mpq-viewer.svg DESTINATION share/icons/hicolor/scalable/apps)

# Optional tests: cmake -DBUILD_TESTING=ON
option(BUILD_TESTING "Build tests" OFF)
if(BUILD_TESTING)
    add_executable(test-core tests/test_core.cpp src/mpqarchive.cpp src/mpqmodel.cpp src/blp.cpp)
    target_include_directories(test-core PRIVATE ${STORMLIB_DIR}/src)
    target_link_libraries(test-core PRIVATE Qt6::Widgets storm)
endif()
if(BUILD_TESTING)
    add_executable(test-gui tests/test_gui.cpp src/mainwindow.cpp src/mpqarchive.cpp src/mpqmodel.cpp src/blp.cpp)
    target_include_directories(test-gui PRIVATE ${STORMLIB_DIR}/src)
    target_link_libraries(test-gui PRIVATE Qt6::Widgets storm)
    target_compile_definitions(test-gui PRIVATE APP_VERSION="test")
endif()
