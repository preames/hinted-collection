make -C ../../hintgc/build/ build
make
export pf_stats_file=scale-cpus.csv
rm -f scale-cpus.csv
python eval.py cpus
#export pf_stats_file=scale-size.csv
#rm -f scale-size.csv
#python eval.py size
#export pf_stats_file=numa.csv
#rm -f scale-cpus.csv
#python eval.py numa
