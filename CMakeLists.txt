cmake_minimum_required(VERSION 3.10)
project(LanSwiftTransfer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2")
set(CMAKE_CXX_FLAGS_DEBUG   "-g -O0 -DDEBUG")

# Options
option(ENABLE_TRANSFER_LOG  "Enable verbose transfer logging" OFF)
set(DEFAULT_CHUNK_SIZE_MB   4   CACHE STRING "Default chunk size in MB")
set(DEFAULT_WINDOW_SIZE     16  CACHE STRING "Default sliding window size")

if(ENABLE_TRANSFER_LOG)
    add_definitions(-DENABLE_TRANSFER_LOG)
endif()
add_definitions(-DDEFAULT_CHUNK_SIZE_MB=${DEFAULT_CHUNK_SIZE_MB})
add_definitions(-DDEFAULT_WINDOW_SIZE=${DEFAULT_WINDOW_SIZE})

# Qt5
find_package(Qt5 REQUIRED COMPONENTS Core Network Widgets)

# OpenSSL for MD5
find_package(OpenSSL REQUIRED)

# Sources
set(CORE_SOURCES
    src/core/ThreadPool.h
    src/core/Protocol.h
    src/core/MD5Helper.h
    src/core/MD5Helper.cpp
    src/core/ResumeManager.h
    src/core/ResumeManager.cpp
    src/core/SendWorker.h
    src/core/SendWorker.cpp
    src/core/RecvWorker.h
    src/core/RecvWorker.cpp
    src/core/TransferManager.h
    src/core/TransferManager.cpp
)

set(UI_SOURCES
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/ProgressWidget.h
    src/ui/ProgressWidget.cpp
    src/ui/FileListView.h
    src/ui/FileListView.cpp
)

set(ALL_SOURCES
    src/main.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
)

add_executable(lanswift_transfer ${ALL_SOURCES})

target_include_directories(lanswift_transfer PRIVATE
    src
    src/core
    src/ui
)

target_link_libraries(lanswift_transfer
    Qt5::Core
    Qt5::Network
    Qt5::Widgets
    OpenSSL::Crypto
    pthread
)

# Output directory
set_target_properties(lanswift_transfer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install
install(TARGETS lanswift_transfer DESTINATION bin)
