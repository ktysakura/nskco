cmake_minimum_required(VERSION 3.0.0)
PROJECT(nskco)
INCLUDE_DIRECTORIES(/home/syh/nskco/core)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

include_directories(
	core
)

# 第三方库
set(LIBS colib pthread dl)

subdirs(
	core
	sample
)
