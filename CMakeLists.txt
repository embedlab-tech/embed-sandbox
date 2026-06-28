# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(embed-sandbox)

target_sources(app PRIVATE
    src/main.c
    src/uart_manager.c
    src/generator.c
    src/shell_commands.c
    src/scenarios.c
    src/statistics.c
)
