cmake_minimum_required(VERSION 3.20)
project(distribution_system C)

set(CMAKE_C_STANDARD 99)

add_executable(distribution_system pa4.c utils.c process.c main_process.c ipc.c)
