# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/builder/temp/libevent-2.1.12-stable

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/builder/temp/libevent-2.1.12-stable/build

# Include any dependencies generated for this target.
include CMakeFiles/http-server.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/http-server.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/http-server.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/http-server.dir/flags.make

CMakeFiles/http-server.dir/sample/http-server.c.o: CMakeFiles/http-server.dir/flags.make
CMakeFiles/http-server.dir/sample/http-server.c.o: ../sample/http-server.c
CMakeFiles/http-server.dir/sample/http-server.c.o: CMakeFiles/http-server.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/builder/temp/libevent-2.1.12-stable/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/http-server.dir/sample/http-server.c.o"
	/opt/cross_env/rk3588s/gcc-linaro-11.3.1-2022.06-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT CMakeFiles/http-server.dir/sample/http-server.c.o -MF CMakeFiles/http-server.dir/sample/http-server.c.o.d -o CMakeFiles/http-server.dir/sample/http-server.c.o -c /home/builder/temp/libevent-2.1.12-stable/sample/http-server.c

CMakeFiles/http-server.dir/sample/http-server.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/http-server.dir/sample/http-server.c.i"
	/opt/cross_env/rk3588s/gcc-linaro-11.3.1-2022.06-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/builder/temp/libevent-2.1.12-stable/sample/http-server.c > CMakeFiles/http-server.dir/sample/http-server.c.i

CMakeFiles/http-server.dir/sample/http-server.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/http-server.dir/sample/http-server.c.s"
	/opt/cross_env/rk3588s/gcc-linaro-11.3.1-2022.06-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/builder/temp/libevent-2.1.12-stable/sample/http-server.c -o CMakeFiles/http-server.dir/sample/http-server.c.s

# Object files for target http-server
http__server_OBJECTS = \
"CMakeFiles/http-server.dir/sample/http-server.c.o"

# External object files for target http-server
http__server_EXTERNAL_OBJECTS =

bin/http-server: CMakeFiles/http-server.dir/sample/http-server.c.o
bin/http-server: CMakeFiles/http-server.dir/build.make
bin/http-server: lib/libevent_extra-2.1.so.7.0.1
bin/http-server: lib/libevent_core-2.1.so.7.0.1
bin/http-server: CMakeFiles/http-server.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/builder/temp/libevent-2.1.12-stable/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable bin/http-server"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/http-server.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/http-server.dir/build: bin/http-server
.PHONY : CMakeFiles/http-server.dir/build

CMakeFiles/http-server.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/http-server.dir/cmake_clean.cmake
.PHONY : CMakeFiles/http-server.dir/clean

CMakeFiles/http-server.dir/depend:
	cd /home/builder/temp/libevent-2.1.12-stable/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/builder/temp/libevent-2.1.12-stable /home/builder/temp/libevent-2.1.12-stable /home/builder/temp/libevent-2.1.12-stable/build /home/builder/temp/libevent-2.1.12-stable/build /home/builder/temp/libevent-2.1.12-stable/build/CMakeFiles/http-server.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/http-server.dir/depend
