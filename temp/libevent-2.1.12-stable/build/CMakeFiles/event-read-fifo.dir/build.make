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
include CMakeFiles/event-read-fifo.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/event-read-fifo.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/event-read-fifo.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/event-read-fifo.dir/flags.make

CMakeFiles/event-read-fifo.dir/sample/event-read-fifo.c.o: CMakeFiles/event-read-fifo.dir/flags.make
CMakeFiles/event-read-fifo.dir/sample/event-read-fifo.c.o: ../sample/event-read-fifo.c
CMakeFiles/event-read-fifo.dir/sample/event-read-fifo.c.o: CMakeFiles/event-read-fifo.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/builder/temp/libevent-2.1.12-stable/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/event-read-fifo.dir/sample/event-read-fifo.c.o"
	/opt/cross_env/rk3588s/gcc-linaro-11.3.1-2022.06-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT CMakeFiles/event-read-fifo.dir/sample/event-read-fifo.c.o -MF CMakeFiles/event-read-fifo.dir/sample/event-read-fifo.c.o.d -o CMakeFiles/event-read-fifo.dir/sample/event-read-fifo.c.o -c /home/builder/temp/libevent-2.1.12-stable/sample/event-read-fifo.c

CMakeFiles/event-read-fifo.dir/sample/event-read-fifo.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/event-read-fifo.dir/sample/event-read-fifo.c.i"
	/opt/cross_env/rk3588s/gcc-linaro-11.3.1-2022.06-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/builder/temp/libevent-2.1.12-stable/sample/event-read-fifo.c > CMakeFiles/event-read-fifo.dir/sample/event-read-fifo.c.i

CMakeFiles/event-read-fifo.dir/sample/event-read-fifo.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/event-read-fifo.dir/sample/event-read-fifo.c.s"
	/opt/cross_env/rk3588s/gcc-linaro-11.3.1-2022.06-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/builder/temp/libevent-2.1.12-stable/sample/event-read-fifo.c -o CMakeFiles/event-read-fifo.dir/sample/event-read-fifo.c.s

# Object files for target event-read-fifo
event__read__fifo_OBJECTS = \
"CMakeFiles/event-read-fifo.dir/sample/event-read-fifo.c.o"

# External object files for target event-read-fifo
event__read__fifo_EXTERNAL_OBJECTS =

bin/event-read-fifo: CMakeFiles/event-read-fifo.dir/sample/event-read-fifo.c.o
bin/event-read-fifo: CMakeFiles/event-read-fifo.dir/build.make
bin/event-read-fifo: lib/libevent_extra-2.1.so.7.0.1
bin/event-read-fifo: lib/libevent_core-2.1.so.7.0.1
bin/event-read-fifo: CMakeFiles/event-read-fifo.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/builder/temp/libevent-2.1.12-stable/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable bin/event-read-fifo"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/event-read-fifo.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/event-read-fifo.dir/build: bin/event-read-fifo
.PHONY : CMakeFiles/event-read-fifo.dir/build

CMakeFiles/event-read-fifo.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/event-read-fifo.dir/cmake_clean.cmake
.PHONY : CMakeFiles/event-read-fifo.dir/clean

CMakeFiles/event-read-fifo.dir/depend:
	cd /home/builder/temp/libevent-2.1.12-stable/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/builder/temp/libevent-2.1.12-stable /home/builder/temp/libevent-2.1.12-stable /home/builder/temp/libevent-2.1.12-stable/build /home/builder/temp/libevent-2.1.12-stable/build /home/builder/temp/libevent-2.1.12-stable/build/CMakeFiles/event-read-fifo.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/event-read-fifo.dir/depend
