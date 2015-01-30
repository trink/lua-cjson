project ( lua-cjson C )
cmake_minimum_required ( VERSION 2.8 )
include ( cmake/dist.cmake )
include ( lua )
# lua-cjson modules
add_definitions ( -DDIST_VERSION="${DIST_VERSION}" )
include_directories("${EP_BASE}/include")

option(USE_INTERNAL_FPCONV "Use internal strtod() / g_fmt() code for performance")
option(MULTIPLE_THREADS "Support multi-threaded apps with internal fpconv - recommended" ON)

if(NOT USE_INTERNAL_FPCONV)
    # Use libc number conversion routines (strtod(), sprintf())
    set(FPCONV_SOURCES fpconv.c)
else()
    # Use internal number conversion routines
    add_definitions(-DUSE_INTERNAL_FPCONV)
    set(FPCONV_SOURCES g_fmt.c dtoa.c)

    include(TestBigEndian)
    TEST_BIG_ENDIAN(IEEE_BIG_ENDIAN)
    if(IEEE_BIG_ENDIAN)
        add_definitions(-DIEEE_BIG_ENDIAN)
    endif()

    if(MULTIPLE_THREADS)
        set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
        find_package(Threads REQUIRED)
        if(NOT CMAKE_USE_PTHREADS_INIT)
            message(FATAL_ERROR "Pthreads not found - required by MULTIPLE_THREADS option")
        endif()
        add_definitions(-DMULTIPLE_THREADS)
    endif()
endif()

install_lua_module ( cjson lua_cjson.c strbuf.c ${FPCONV_SOURCES} lua_cjson.def)
install_doc ( manual.txt )

# Handle platforms missing isinf() macro (Eg, some Solaris systems).
include(CheckSymbolExists)
CHECK_SYMBOL_EXISTS(isinf math.h HAVE_ISINF)
if(NOT HAVE_ISINF)
    add_definitions(-DUSE_INTERNAL_ISINF)
endif()

if(WIN32)
    # Windows sprintf()/strtod() handle NaN/inf differently. Not supported.
    add_definitions(-DDISABLE_INVALID_NUMBERS)
    if(MINGW)
       set_target_properties(cjson PROPERTIES LINK_FLAGS -s)
       if (ADDRESS_MODEL EQUAL 32)
           set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
           set_target_properties(cjson PROPERTIES LINK_FLAGS "-s -m32")
       endif()
    endif()
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall -fPIC")
endif()

