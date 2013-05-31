import hintgc_format

#todo: create a plot2d_diff function
#todo: extract diff helper functions
#todo: extract filter functions

import scipy
import scipy.stats
    
def print_stats(data):
    data = [float(x) for x in data]
    print len(data)
    #print sum(data)
    print min(data)
    print max(data)
    print scipy.stats.mstats.gmean(data)
    print scipy.stats.scoreatpercentile(data, 50)
    #print scipy.stats.scoreatpercentile(data, 95)
    #print scipy.stats.scoreatpercentile(data, 90)
    print scipy.stats.scoreatpercentile(data, 99)
    #print scipy.stats.scoreatpercentile(data, 99.9)
    print scipy.stats.scoreatpercentile(data, 99.95)
    print scipy.stats.scoreatpercentile(data, 99.99)
    

def hist2d(gccsvfile, pfcsvfile, rootname, xselect):
    ofile = open(rootname + ".dat", 'w')

    gcbins = hintgc_format.bin_column(gccsvfile, xselect)
    pfbins = hintgc_format.bin_column(pfcsvfile, xselect)

    gckeysi = [int(i) for i in gcbins.keys()]
    pfkeysi = [int(i) for i in pfbins.keys()]

        

    a = hintgc_format.get_column(gccsvfile, xselect)
    print_stats(a)
    a = hintgc_format.get_column(pfcsvfile, xselect)
    print_stats(a)
    lowest = min( min(gckeysi), min(pfkeysi))
    highest = max( max(gckeysi), max(pfkeysi))

    for i in range(lowest, highest+1, 10):
        key = str(i)
        if key not in gcbins:
            gcbins[key] = 0
        if key not in pfbins:
            pfbins[key] = 0

    for i in range(250, highest+1, 10):
        key = str(i)
        gcval = gcbins[key]
        pfval = pfbins[key]
        ofile.write( str(key) + ' ' + str(gcval) + ' ' + str(pfval) + '\n')
    
    with open(rootname + ".plt", 'w') as pltfile:
        pltfile.write("set term epslatex\n")
        pltfile.write("set output '"+rootname+".tex'\n")
        pltfile.write("set ylabel \""+hintgc_format.labels[xselect]+"\"\n")
        pltfile.write("set xlabel \"Count\"\n")
        pltfile.write("set yrange [*:*]\n")
        pltfile.write("plot \""+rootname+".dat\" using 2:1 title \"GC\" lt rgb \"red\", \""+rootname+".dat\" using 3:1 title \"PF\" lt rgb \"blue\"\n")

    with open("generated/commands.sh", "a") as cmds:
        cmds.write("gnuplot "+rootname+".plt && epstopdf "+rootname+".eps\n")

def perc_hist2d(gccsvfile, rootname, xselect):
    ofile = open(rootname + ".dat", 'w')

    gcbins = hintgc_format.perc_bin_column(gccsvfile, xselect)
    

    gckeysi = [int(i) for i in gcbins.keys()]
    
    lowest = min(gckeysi)
    highest = max(gckeysi)

    for i in range(lowest, highest+1, 10):
        key = str(i)
        if key not in gcbins:
            gcbins[key] = 0

    for i in range(lowest, highest+1, 10):
        key = str(i)
        gcval = gcbins[key]
        ofile.write( str(key) + ' ' + str(gcval) + '\n')
    
    with open(rootname + ".plt", 'w') as pltfile:
        pltfile.write("set term epslatex\n")
        pltfile.write("set output '"+rootname+".tex'\n")
        pltfile.write("set xlabel \""+hintgc_format.labels[xselect]+"\"\n")
        pltfile.write("set ylabel \"Count\"\n")
        pltfile.write("set yrange [*:*]\n")
        pltfile.write("plot \""+rootname+".dat\" using 1:2 title \"Free\" lt rgb \"red\"")

    with open("generated/commands.sh", "a") as cmds:
        cmds.write("gnuplot "+rootname+".plt && epstopdf "+rootname+".eps\n")





hist2d("/home/reames/tmp/clang_build_gc_stats-release.csv", "/home/reames/tmp/clang_build_pf_stats-release.csv", "generated/pause_dist", hintgc_format.runtime)

perc_hist2d("/home/reames/tmp/clang_build_pf_stats-release.csv", "generated/frac_free", hintgc_format.percent_freed)

perc_hist2d("../benchmarks/micro/results/micro-run-merged.csv", "generated/frac_free2", hintgc_format.percent_freed)

frac_free_data("/home/reames/tmp/clang_build_gc_stats-release.csv")
exit(1)
