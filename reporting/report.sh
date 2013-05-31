mkdir -p generated
rm -f generated/*
# serial (emerald!)
python make-scale-graph.py
# parallel
python make-scale-graph2.py ../benchmarks/micro/results/gainestown-scale-size.csv gainestown-4p
python make-scale-graph2.py ../benchmarks/micro/results/emerald-scale-size.csv emerald-4p

python make-cpu-scale-graph.py ../benchmarks/micro/results/emerald-scale-cpu.csv emerald
python make-cpu-scale-graph.py ../benchmarks/micro/results/gainestown-scale-cpu.csv gainestown
python make-cpu-scale-graph.py ../benchmarks/micro/results/laptop-scale-cpu.csv laptop
python make-cpu-scale-graph.py ../benchmarks/micro/results/jktqos-scale-cpu.csv jktqos

#python make-graph.py
#python make-histogram.py
python make-micro-table.py "../benchmarks/micro/results/laptop-1p/" all > generated/micro-table-laptop-1p.tex
python make-micro-table.py "../benchmarks/micro/results/laptop-1p/" basic > generated/micro-table-laptop-1p-basic.tex
python make-micro-table.py "../benchmarks/micro/results/laptop-1p/" opts > generated/micro-table-laptop-1p-opts.tex
python make-micro-table.py "../benchmarks/micro/results/laptop-4p/" basic > generated/micro-table-laptop-4p-basic.tex
python make-micro-table.py "../benchmarks/micro/results/emerald-4p/" basic > generated/micro-table-emerald-4p-basic.tex
python make-micro-table.py "../benchmarks/micro/results/emerald-8p/" basic > generated/micro-table-emerald-8p-basic.tex
python make-micro-table.py "../benchmarks/micro/results/emerald-16p/" basic > generated/micro-table-emerald-16p-basic.tex
python make-micro-table.py "../benchmarks/micro/results/emerald-32p/" basic > generated/micro-table-emerald-32p-basic.tex
python make-micro-table.py "../benchmarks/micro/results/emerald-64p/" basic > generated/micro-table-emerald-64p-basic.tex

python make-micro-table.py "../benchmarks/micro/results/gainestown-4p/" basic > generated/micro-table-gainestown-4p-basic.tex
python make-micro-table.py "../benchmarks/micro/results/gainestown-8p/" basic > generated/micro-table-gainestown-8p-basic.tex
python make-micro-table.py "../benchmarks/micro/results/gainestown-16p/" basic > generated/micro-table-gainestown-16p-basic.tex
python make-micro-table.py "../benchmarks/micro/results/gainestown-32p/" basic > generated/micro-table-gainestown-32p-basic.tex

python make-micro-scale-table.py "../benchmarks/micro/results/laptop-1p/" "../benchmarks/micro/results/laptop-4p/" > generated/micro-compare-table-laptop-1p-4p.tex

#python make-spec-time-table.py "../benchmarks/results/spec-emerald-16p-ref/" > generated/spec-emerald-16p-time-table.tex
#python make-spec-mem-table.py "../benchmarks/results/spec-emerald-16p-ref/" > generated/spec-emerald-16p-mem-table.tex
python make-spec-time-table.py "../benchmarks/results/before-lb/spec-emerald-16p-ref/" > generated/spec-emerald-16p-time-table-before-lb.tex
python make-spec-time-table.py "../benchmarks/results/spec-emerald-16p-ref/" > generated/spec-emerald-16p-time-table.tex
python make-spec-mem-table.py "../benchmarks/results/spec-emerald-16p-ref/" > generated/spec-emerald-16p-mem-table.tex
python make-spec-time-table.py "../benchmarks/results/spec-emerald-4p-ref/" > generated/spec-emerald-4p-time-table.tex
python make-spec-mem-table.py "../benchmarks/results/spec-emerald-4p-ref/" > generated/spec-emerald-4p-mem-table.tex

python make-spec-time-table.py "../benchmarks/results/spec-gainestown-16p-ref/" > generated/spec-gainestown-16p-time-table.tex
python make-spec-mem-table.py "../benchmarks/results/spec-gainestown-16p-ref/" > generated/spec-gainestown-16p-mem-table.tex
python make-spec-time-table.py "../benchmarks/results/spec-gainestown-4p-ref/" > generated/spec-gainestown-4p-time-table.tex
python make-spec-mem-table.py "../benchmarks/results/spec-gainestown-4p-ref/" > generated/spec-gainestown-4p-mem-table.tex


python make-spec-time-table.py "../benchmarks/results/spec-laptop-4p-ref/" > generated/spec-laptop-4p-time-table.tex
python make-spec-mem-table.py "../benchmarks/results/spec-laptop-4p-ref/" > generated/spec-laptop-4p-mem-table.tex
python make-spec-time-table.py "../benchmarks/results/spec-laptop-1p-ref/" > generated/spec-laptop-1p-time-table.tex
python make-spec-mem-table.py "../benchmarks/results/spec-laptop-1p-ref/" > generated/spec-laptop-1p-mem-table.tex

python make-spec-mem-graph.py ../benchmarks/results/spec-laptop-1p-ref/ laptop-1p
python make-spec-mem-graph.py ../benchmarks/results/spec-laptop-4p-ref/ laptop-4p

python make-spec-time-table.py ../benchmarks/results/spec-laptop-4p-ref-freehack/ libquantum > generated/spec-laptop-4p-freehack-time-table.tex
python make-spec-mem-table.py ../benchmarks/results/spec-laptop-4p-ref-freehack/ libquantum > generated/spec-laptop-4p-freehack-mem-table.tex

python make-clang-time-table.py > generated/clang-time-table.tex
python make-clang-mem-table.py > generated/clang-mem-table.tex
chmod u+x generated/commands.sh
./generated/commands.sh
texi2pdf --batch template.tex
