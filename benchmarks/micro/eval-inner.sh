# $1 is size, $2 is delloc

echo "inner: size=$1 -- dealloc=$2 -- GC_MARKERS=$3"
#disable until after warmup
export pf_stats_default_disable=1 
export max_alloc_number=$1
export dealloc_percentage=$2
export GC_MARKERS=$3
echo $2
export gc_run=0
./hintgc.out
export gc_run=1
./hintgc.out
