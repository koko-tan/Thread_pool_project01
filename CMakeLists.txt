cmake_minimum_required(VERSION 3.14)
project(Thread_pool_project01 C)

set(CMAKE_C_STANDARD 99)

add_executable(Thread_pool_project01 main.c Thread_pool.c Thread_pool.h)