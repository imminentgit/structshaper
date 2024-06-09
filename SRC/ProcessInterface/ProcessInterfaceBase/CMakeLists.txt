cmake_minimum_required(VERSION 3.28)
project(ProcessInterfaceBase)

add_library(ProcessInterfaceBase STATIC
        SRC/ProcessInterfaceBase.cpp
        SRC/ProcessInterfaceBase.h
)

target_compile_options(ProcessInterfaceBase
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:${COMPILE_FLAGS}>
        $<$<COMPILE_LANGUAGE:C>:${COMPILE_FLAGS}>)
