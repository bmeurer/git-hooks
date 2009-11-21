project(git-hooks C)

# default to Debug builds
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug)
endif(NOT CMAKE_BUILD_TYPE)

# disable assertions for non-debug builds
if(NOT CMAKE_BUILD_TYPE MATCHES Debug)
	add_definitions(-DNDEBUG)
endif(NOT CMAKE_BUILD_TYPE MATCHES Debug)

# set minimum CMake version
cmake_minimum_required(VERSION 2.4)

# check for certain header files
include(CheckIncludeFile)
check_include_file(ctype.h HAVE_CTYPE_H)
check_include_file(dirent.h HAVE_DIRENT_H)
check_include_file(errno.h HAVE_ERRNO_H)
check_include_file(signal.h HAVE_SIGNAL_H)
check_include_file(stdarg.h HAVE_STDARG_H)
check_include_file(stdio.h HAVE_STDIO_H)
check_include_file(stdlib.h HAVE_STDLIB_H)
check_include_file(string.h HAVE_STRING_H)
check_include_file(sys/wait.h HAVE_SYS_WAIT_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(unistd.h HAVE_UNISTD_H)

# generate config.h from config.h.in
configure_file(config.h.in config.h)

# force inclusion of config.h
add_definitions(-DHAVE_CONFIG_H)
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# GNU C compiler flags
if(CMAKE_COMPILER_IS_GNUCC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall -Werror -Wextra -Winit-self -Wredundant-decls -Wformat-nonliteral -Wformat-security -Wswitch-enum -Wundef")
endif(CMAKE_COMPILER_IS_GNUCC)

# build and install the git-run-hooks program
add_executable(git-run-hooks git-run-hooks.c)
install(TARGETS git-run-hooks
	DESTINATION bin)

# install documentation
install(FILES "README"
	DESTINATION "share/doc/git-hooks")

add_subdirectory(hooks)
add_subdirectory(template)
