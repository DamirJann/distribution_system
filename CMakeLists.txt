cmake_minimum_required(VERSION 3.20)
project(distribution_system C)

set(CMAKE_C_STANDARD 99)

add_library(distribution_system pa23.c c_process.c k_process.c auxiliary.c)
