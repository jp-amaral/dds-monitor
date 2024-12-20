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

# Link Fast DDS libraries
target_link_libraries(monitor
    fastdds
    fastcdr
)
