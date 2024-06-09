cmake_minimum_required(VERSION 3.28)
project(ProcessInterface_UserMode)

include_directories(../ProcessInterfaceBase/SRC)

if (${STRUCTSHAPER_IS_WINDOWS})
    add_library(ProcessInterface_UserMode SHARED
            SRC/ProcessInterface_UserMode_Windows.cpp

            SRC/win/nt_defs.h
            SRC/win/nt_defs.cpp

            SRC/win/syscall.asm)
elseif (${STRUCTSHAPER_IS_LINUX})
    add_library(ProcessInterface_UserMode SHARED
            SRC/ProcessInterface_UserMode_Linux.cpp
            SRC/ProcessInterface_UserMode_Linux.cpp)
endif ()

target_compile_options(ProcessInterface_UserMode
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:${COMPILE_FLAGS}>
        $<$<COMPILE_LANGUAGE:C>:${COMPILE_FLAGS}>)

add_custom_command(TARGET ProcessInterface_UserMode POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ProcessInterface_UserMode> ${CMAKE_SOURCE_DIR}/Memory_Interfaces/${CMAKE_SHARED_LIBRARY_PREFIX}ProcessInterface_UserMode${CMAKE_SHARED_LIBRARY_SUFFIX})