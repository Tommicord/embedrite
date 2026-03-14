cmake_minimum_required(VERSION 4.1)
project(Embedrite C)

set(CMAKE_C_STANDARD 11)

add_executable(embdc
        main.c
        src/embedrite/lexan.c
        src/embedrite/lexan.h)
