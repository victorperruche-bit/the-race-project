cmake_minimum_required(VERSION 3.10)
project(TheRaceProject CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Mode debug par défaut si non spécifié
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Options de compilation
add_compile_options(-Wall -Wextra)

# Exécutable principal
add_executable(the_race
    src/main.cpp
    src/Robot.cpp
    src/CapteurUltrason.cpp
    src/CorrecteurPD.cpp
)

target_include_directories(the_race PRIVATE include)
