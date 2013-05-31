echo "--- Wrapping hintgc preload  ---"
export LD_PRELOAD=/home/reames/Files/projects/free-hint/hintgc/build/release/.libs/libgc.so
export cmd=$1
shift
echo "--- Command: " $cmd
$cmd $@
echo "--- Unrapping hintgc preload ---"
export LD_PRELOAD=
