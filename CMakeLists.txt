# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(mp3_player)

# Suppress warnings in third-party components (GCC 15 / ESP-IDF v6.0)
add_compile_options(-Wno-ignored-qualifiers -Wno-unused-variable)
