cmake_minimum_required(VERSION 3.28)
project(Shared)

include_directories(./SRC)

add_library(Shared STATIC
        SRC/Utils/Utils.cpp
        SRC/Utils/Utils.h

        SRC/Logger/Logger.h

        SRC/Utils/ScopeGuard.h

        SRC/Defines.h

        SRC/Utils/IDAllocator.h

        SRC/Utils/Fnv1.h

        SRC/Utils/expected_result.h

        SRC/Utils/StaticInstance.h)

target_compile_options(Shared
        PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:${COMPILE_FLAGS}>
        $<$<COMPILE_LANGUAGE:C>:${COMPILE_FLAGS}>)