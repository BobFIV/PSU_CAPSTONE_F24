#
# Copyright (c) 2019 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(cellular_fundamentals)

# NORDIC SDK APP START
target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/lte.c)
target_sources(app PRIVATE src/coap_onem2m.c)
target_sources(app PRIVATE src/i2c.c)
target_sources(app PRIVATE src/gnss.c)
target_sources(app PRIVATE src/battery.c)
# NORDIC SDK APP END
