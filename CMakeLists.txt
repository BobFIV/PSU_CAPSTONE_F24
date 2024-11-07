#
# Copyright (c) 2024 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(dect-nr)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/mesh.c)
target_sources(app PRIVATE src/mesh-root.c)
target_sources(app PRIVATE src/dect-nr.c)
target_sources(app PRIVATE src/uart.c)
