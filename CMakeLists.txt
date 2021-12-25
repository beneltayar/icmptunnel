cmake_minimum_required(VERSION 3.5)
project(rawsock C)

set(CMAKE_C_STANDARD 11)

add_executable(server server.c utils.h utils.c)
add_executable(client client.c utils.h utils.c)
