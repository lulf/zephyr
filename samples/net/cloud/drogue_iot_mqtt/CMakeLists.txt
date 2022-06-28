# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(drogue_iot_mqtt)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/dhcp.c)
target_sources(app PRIVATE src/protocol.c)
zephyr_include_directories(${APPLICATION_SOURCE_DIR}/src/tls_config)

#set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated/)
#
#generate_inc_file_for_target(
#    app
#    src/mqtts-cert.der
#    ${gen_dir}/mqtts-cert.der.inc
#    )
