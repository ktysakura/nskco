cmake_minimum_required(VERSION 3.0.0)
project(nskco)
aux_source_directory(. SRC)
# Use c and asm
enable_language(C ASM)
set(SOURCE_FILES ${SRC} coctx_swap.S)

add_library(nsk_core ${SOURCE_FILES})
add_library(nskco_shared SHARED ${SOURCE_FILES})

