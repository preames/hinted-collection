echo "--- Wrapping boehmgc preload  ---"
export LD_PRELOAD=/home/reames/Files/projects/free-hint/boehm-gc-7.2/build/.libs/libgc.so
echo "--- Command: " $1
$1
echo "--- Unrapping bohemgc preload ---"
export LD_PRELOAD=
