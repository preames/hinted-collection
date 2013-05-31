echo "--- Wrapping instlib preload  ---"
export LD_PRELOAD=/home/reames/Files/projects/free-hint/instlib/instrumented_preload.so
export cmd=$1
shift
echo "--- Command: " $cmd
$cmd $@
echo "--- Unrapping instlib preload ---"
export LD_PRELOAD=
