# $1 is size, $2 is delloc

echo "inner: --cpunodebind=$1 --membind=$2"
#disable until after warmup
export pf_stats_default_disable=1 
export gc_run=0
numactl --cpunodebind=2 --membind=2 ./hintgc.out
export gc_run=1
numactl --cpunodebind=2 --membind=2 ./hintgc.out
