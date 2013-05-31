# does no useful allocation/deallocation for my purposes - never triggers a GC
gdb --args /home/reames/benchmarks/C/sqlite-autoconf-3070800/.libs/lt-sqlite3 -init sqlite-test.sql
