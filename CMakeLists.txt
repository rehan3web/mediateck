cmake_minimum_required(VERSION 3.15)
project(MtkMetaTool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Statically link C++ runtime so the binary runs on any Windows machine without needing VC++ redistributable
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include_directories(include)

set(SOURCES
    src/main.cpp
    src/Logger.cpp
    src/SerialPort.cpp
    src/DeviceWatcher.cpp
    src/MtkHandshake.cpp
)

set(HEADERS
    include/MtkDefines.hpp
    include/Logger.hpp
    include/SerialPort.hpp
    include/DeviceWatcher.hpp
    include/MtkHandshake.hpp
)

add_executable(mtk_meta_tool ${SOURCES} ${HEADERS})

if (WIN32)
    target_link_libraries(mtk_meta_tool PRIVATE setupapi advapi32)
endif()

# Compiler specific flags
if (MSVC)
    target_compile_options(mtk_meta_tool PRIVATE /W4 /O2 /utf-8 /MT)
else()
    target_compile_options(mtk_meta_tool PRIVATE -Wall -Wextra -O2 -static -static-libgcc -static-libstdc++)
endif()

# Set output directory to bin/
set_target_properties(mtk_meta_tool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)
