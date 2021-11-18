cmake_minimum_required(VERSION 3.20)
project(distribution_system C)

set(CMAKE_C_STANDARD 99)

add_compile_options(-std=c99 -Wall -pedantic)
SET(LINK_FLAGS "-L /home/damire/space/distibution_system/distribution_system -lruntime")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LINK_FLAGS}")

add_executable(distribution_system
        pa23.c
        auxiliary.c auxiliary.h
        bank_robbery.c
        pa2345.h
        banking.h banking.c)
