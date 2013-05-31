#This is for my machine
# Add appropriate LDFLAGS and LD_LIBRARY_PATH
#export LD_LIBRARY_PATH=/usr/local/lib:/home/reames/Files/projects/dbglib/:$LD_LIBRARY_PATH
#export CFLAGS="-g"
# hack alert - no clean way to merge cflags, do this by overridding the command
#export CC="gcc -I/home/reames/Files/projects/dbglib/ -DIGNORE_FREE"
#export LD="gcc -
#export LDFLAGS="-L/usr/local/lib -lunwind -L/home/reames/Files/projects/dbglib/ -ldbgutils -L/usr/local/lib/ -lpapi"

#This is for Emerald
export LDFLAGS="-lpapi"
export CC="gcc -DIGNORE_FREE"


export CONFIGFLAGS="--prefix=/home/reames/Files/projects/free-hint/hintgc/install/ --enable-redirect-malloc --disable-gcj-support --enable-large-config --enable-parallel-mark --enable-handle-fork"

# Note: The original evaluation for the ISMM paper submission was done 
# without --enable-parallel-mark and with --enable-threads=no.   This
# has the effect of building a serial collector which can only handle
# one thread in the client program.
# Note: If you want to switch between the serial and parallel collector
# a full reconfigure, *make clobber*, and make is required.  You *must*
# delete everything or the build will be corrupt in non-obvious ways.


# The actual configure command
# RELEASE VARIANT
pushd release
../../src/configure $CONFIGFLAGS 
popd

# RELEASE w/ASSERTS
pushd release+asserts
../../src/configure $CONFIGFLAGS --enable-gc-assertions
popd

# DEBUG VARIANT
pushd debug
../../src/configure $CONFIGFLAGS --enable-gc-assertions  --enable-gc-debug
# need to take a closer look at the enable-threads flags  
popd

# --enable-threads=no - causes some weird behavior under debug mode.  Would like to disable for consistency, but unfortunately benchmarks require it.  (memslap as an example)
# --disable-gcj-support -- requires mark procs which we currently don't support
# pthreads screws up debugging
# --enable-gc-debug - needed for -g debug info, however, it also breaks free redirection through the instrument GC_free - it goes to GC_debug_free instead.  "FIXED" via a hack in the configure file.  Not suitable for merging or general use.
# --enable_handle_fork -- cleanly shuts down GC and restarts it in the forked process - only really needed for the GPUGC test harness since we use fork to get a shared clean memory state.
