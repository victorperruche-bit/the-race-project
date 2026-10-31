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

# Tests automatiques (recentrage + rotation hors portee)
add_executable(test_robot
    tests/test_robot.cpp
    src/Robot.cpp
    src/CapteurUltrason.cpp
    src/CorrecteurPD.cpp
)
target_include_directories(test_robot PRIVATE include)

# ctest : lance les tests via "ctest" dans le dossier build
enable_testing()
add_test(NAME tests_robot COMMAND test_robot)
