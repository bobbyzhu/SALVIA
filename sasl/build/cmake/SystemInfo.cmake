# define a set of string with may-be useful readable name
# this file is meant to be included in a CMakeLists.txt
# not as a standalone CMake script
set(SOFTART_COMPILER_NAME "")
set(SOFTART_COMPILER_SHOWNAME "")
set(SOFTART_PLATFORM_NAME "")
set(SOFTART_PLATFORM_BITNESS "")

if(WIN32)
  # Compilers
  # taken from http://predef.sourceforge.net/precomp.html#sec34
  if(MSVC)
    if(MSVC_VERSION EQUAL 1200)
      set(SOFTART_COMPILER_NAME "vc-6.0")
	  set(SOFTART_COMPILER_SHOWNAME "msvc6")
    endif(MSVC_VERSION EQUAL 1200)
    if(MSVC_VERSION EQUAL 1300)
      set(SOFTART_COMPILER_NAME "vc-7.0")
	  set(SOFTART_COMPILER_SHOWNAME "msvc7")
    endif(MSVC_VERSION EQUAL 1300)
    if(MSVC_VERSION EQUAL 1310)
      set(SOFTART_COMPILER_NAME "vc-7.1") #Visual Studio 2003
	  set(SOFTART_COMPILER_SHOWNAME "msvc71")
    endif(MSVC_VERSION EQUAL 1310)
    if(MSVC_VERSION EQUAL 1400)
      set(SOFTART_COMPILER_NAME "vc-8.0") #Visual Studio 2005
	  set(SOFTART_COMPILER_SHOWNAME "msvc8")
    endif(MSVC_VERSION EQUAL 1400)
    if(MSVC_VERSION EQUAL 1500)
      set(SOFTART_COMPILER_NAME "vc-9.0") #Visual Studio 2008
	  set(SOFTART_COMPILER_SHOWNAME "msvc9")
    endif(MSVC_VERSION EQUAL 1500)
    if(MSVC_VERSION EQUAL 1600)
      set(SOFTART_COMPILER_NAME "vc-10.0") #Visual Studio 2010
	  set(SOFTART_COMPILER_SHOWNAME "msvc10")
    endif(MSVC_VERSION EQUAL 1600)
  endif(MSVC)
  
  if( MINGW )
  	exec_program( gcc ARGS -dumpversion OUTPUT_VARIABLE GCC_VERSION )
	string( REPLACE "." "" GCC_VERSION_STR_FULL ${GCC_VERSION} )
	string( REGEX MATCH "[0-9]+\\.[0-9]+" GCC_VERSION_MAJOR_MINOR ${GCC_VERSION} )
	string( REPLACE "." "" GCC_VERSION_STR_MAJOR_MINOR ${GCC_VERSION_MAJOR_MINOR} )
    set(SOFTART_COMPILER_NAME "mgw-"${GCC_VERSION})
	set(SOFTART_COMPILER_SHOWNAME "mgw${GCC_VERSION_STR_FULL}")
  endif( MINGW )
  
  if(CMAKE_GENERATOR MATCHES "Win64")
    set(SOFTART_PLATFORM_NAME "x64")
    set(SOFTART_PLATFORM_BITNESS 64)
  else(CMAKE_GENERATOR MATCHES "Win64")
    set(SOFTART_PLATFORM_NAME "win32")
    set(SOFTART_PLATFORM_BITNESS 32)
  endif(CMAKE_GENERATOR MATCHES "Win64")
endif(WIN32)

if(UNIX)
  set(SOFTART_COMPILER_NAME "gcc-"${GCC_VERSION})
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    set(SOFTART_PLATFORM_NAME "x64")
    set(SOFTART_PLATFORM_BITNESS 64)
  else(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    set(SOFTART_PLATFORM_NAME "x86")
    set(SOFTART_PLATFORM_BITNESS 32)
  endif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
endif(UNIX)
