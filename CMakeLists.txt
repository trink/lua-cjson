project(lua-cjson C)
cmake_minimum_required(VERSION 2.6)

option(USE_INTERNAL_FPCONV "Use internal strtod() / g_fmt() code for performance")
option(MULTIPLE_THREADS "Support multi-threaded apps with internal fpconv - recommended" ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
endif()

include_directories("${EP_BASE}/include")

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
            message(FATAL_ERROR
                    "Pthreads not found - required by MULTIPLE_THREADS option")
        endif()
        add_definitions(-DMULTIPLE_THREADS)
    endif()
endif()

if(STRBUF_MAX_SIZE)
	add_definitions(-DSTRBUF_MAX_SIZE=${STRBUF_MAX_SIZE})
endif()

# Handle platforms missing isinf() macro (Eg, some Solaris systems).
include(CheckSymbolExists)
CHECK_SYMBOL_EXISTS(isinf math.h HAVE_ISINF)
if(NOT HAVE_ISINF)
    add_definitions(-DUSE_INTERNAL_ISINF)
endif()

set(CJSON_SOURCE
lua_cjson.c 
strbuf.c 
${FPCONV_SOURCES}
)

if(WIN32)
    # Windows sprintf()/strtod() handle NaN/inf differently. Not supported.
    add_definitions(-DDISABLE_INVALID_NUMBERS)
    add_library(cjson SHARED ${CJSON_SOURCE})
    add_definitions(-DLUA_BUILD_AS_DLL -DLUA_CORE)
    if(MSVC)
       target_link_libraries(cjson "${EP_BASE}/lib/lua.lib")
    else(MINGW)
       set_target_properties(cjson PROPERTIES LINK_FLAGS -s)
       if (ADDRESS_MODEL EQUAL 32)
           set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
           set_target_properties(cjson PROPERTIES LINK_FLAGS "-s -m32")
       endif()
       target_link_libraries(cjson "${EP_BASE}/lib/liblua.dll")
    else()
       message(FATAL_ERROR "Only MSVC and MinGW compilers are supported on Windows")
    endif()
else()
    add_library(cjson STATIC ${CJSON_SOURCE})
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall -fPIC")
    target_link_libraries(cjson "${EP_BASE}/lib/libluajit-5.1.a" "${CMAKE_THREAD_LIBS_INIT}")
endif()

install(TARGETS cjson DESTINATION lib)
