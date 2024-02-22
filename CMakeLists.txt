cmake_minimum_required(VERSION 3.27)
project(Mish)

set(CMAKE_CXX_STANDARD 14)

add_executable(Mish mish.cpp)

# Find the Readline include directory and library
find_path(READLINE_INCLUDE_DIR readline/readline.h)
find_library(READLINE_LIBRARY NAMES readline)

# Include the Readline directory
target_include_directories(${PROJECT_NAME} PRIVATE ${READLINE_INCLUDE_DIR})

# Link against the Readline library
target_link_libraries(${PROJECT_NAME} PRIVATE ${READLINE_LIBRARY})