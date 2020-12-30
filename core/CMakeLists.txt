cmake_minimum_required(VERSION 3.0.0)
aux_source_directory(. SRC)
# Use c and asm
enable_language(C ASM)
add_library(nsk_core ${SRC} coctx_swap.S)

