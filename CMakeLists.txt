cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/main")
project(GoldenReferenceIntegration)