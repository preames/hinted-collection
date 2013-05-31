# NOTE: GCC uses the xrealloc family of routines
# these aren't fully supported by Boehm (seemingly) and seem to result in issues
gdb --args  /usr/lib/gcc/x86_64-linux-gnu/4.6/cc1 -quiet -imultilib . -imultiarch x86_64-linux-gnu /home/reames/benchmarks/C/sqlite-autoconf-3070800/sqlite3.c -quiet -dumpbase sqlite3.c -mtune=generic -march=x86-64 -auxbase sqlite3  -fstack-protector -o /tmp/cctUHIAk.s

