cmake_minimum_required(VERSION 3.16)
project(Notebook VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt6
set(CMAKE_PREFIX_PATH "C:/Qt/6.8.3/msvc2022_64")
find_package(Qt6 REQUIRED COMPONENTS Widgets WebEngineWidgets WebChannel Network)

# Sources
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/Storage.cpp
    src/BridgeApi.cpp
    src/AutoStart.cpp
    src/Scheduler.cpp
    src/ApiServer.cpp
)

set(HEADERS
    src/MainWindow.h
    src/Storage.h
    src/BridgeApi.h
    src/AutoStart.h
    src/Scheduler.h
    src/ApiServer.h
    src/Note.h
    src/Reminder.h
)

# Resources
set(RESOURCES resources.qrc)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE src)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebChannel
    Qt6::Network
)

# Windows: include dwmapi for rounded corners
target_link_libraries(${PROJECT_NAME} PRIVATE dwmapi user32)

# Copy assets to output directory for qrc-less loading
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)

# MSVC: set UTF-8 for Chinese characters
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()
