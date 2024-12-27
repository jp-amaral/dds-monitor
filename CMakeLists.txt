cmake_minimum_required(VERSION 3.10)
project(DDSMonitor)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find FastDDS package
find_package(fastdds REQUIRED)
find_package(fastcdr REQUIRED)

# Add the executable
add_executable(monitor monitor.cpp)

# Add the current directory as an include directory
target_include_directories(monitor PRIVATE ${CMAKE_SOURCE_DIR})

# Link Fast DDS libraries
target_link_libraries(monitor
    fastdds
    fastcdr
)
