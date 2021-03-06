import hintgc_format

#todo: create a plot2d_diff function
#todo: extract diff helper functions
#todo: extract filter functions

import numpy
import scipy
import scipy.stats

import os.path;
import sys;

# yselect is used on both data sets, xselect must be constant
def plot_stacked_histogram(rootname, x, y, y2, xlabel, ylabel):
    ofile = open(rootname + ".dat", 'w')
    assert len(x) == len(y)
    assert len(x) == len(y2)
    for i in range(len(x)):
        dat = [str(x[i]), str(y[i]), str(y2[i])]
        ofile.write( ' '.join( dat ) )
        ofile.write( '\n' )
    
    with open(rootname + ".plt", 'w') as pltfile:
        pltfile.write("set terminal epslatex size 3,2 font \",10\"\n");
#"set term epslatex\n")
        pltfile.write("set output '"+rootname+".tex'\n")
        pltfile.write("set key off\n")
#        pltfile.write("set key below horizontal box\n")
#        pltfile.write("set xlabel \""+xlabel+"\"\n")
#        pltfile.write("set xtics (0,1000,5000,8000)\n");
#        pltfile.write("set ylabel \""+ylabel+"\"\n")
        pltfile.write("set style data histograms\n")
        pltfile.write("set style histogram rowstacked\n")
        pltfile.write("set style fill solid 1.0 border -1\n")
        pltfile.write("set boxwidth 1 relative\n")
        pltfile.write("set xtics rotate\n")
#set style fill solid 1.0 border -1
        pltfile.write("set yrange [0:120] \n")
#set datafile separator " "
#plot '1.dat' using 2 t "Var 1", '' using 3:xticlabels(1) t "Var 2"

#        pltfile.write("set log x\n")
#        pltfile.write("set log y\n")
#        pltfile.write("set format x \"%.0sx10\^%T\"\n")
        pltfile.write("plot \""+rootname+".dat\" using 2:xticlabels(1) title \"Reclaimed\" lt rgb \"#A9A9A9\", \""+rootname+".dat\" using 3:xticlabels(1) title \"Leaked\" lt rgb \"#8B0000\"\n")

    with open("generated/commands.sh", "a") as cmds:
        cmds.write("gnuplot "+rootname+".plt && epstopdf "+rootname+".eps\n")


if __name__ != "__main__":
    exit(0);

if len(sys.argv) not in [2,3]:
    print "Illegal arguments to script"
    print 'Number of arguments:', len(sys.argv), 'arguments.'
    print 'Argument List:', str(sys.argv)
    exit()    

# where are the data files stored?
input_location = sys.argv[1];
import os
assert os.path.exists(input_location)

version = sys.argv[2]

# This is a hack used for reporting the free hack result
only_libquantum = False
#if len(sys.argv) == 3 and sys.argv[2] == "libquantum":
#    only_libquantum = True


import latex_helper


files = []
if not only_libquantum:
    files.append( ["bzip", "hintgc-spec-bzip.csv"] )
    files.append( ["cactusADM", "hintgc-spec-cactusADM.csv"] )
    files.append( ["calculix", "hintgc-spec-calculix.csv"] )
#xrealloc
#files.append( ["gcc", "hintgc-spec-gcc.csv"] )
    files.append( ["gobmk", "hintgc-spec-gobmk.csv"] )
    files.append( ["gromacs", "hintgc-spec-gromacs.csv"] )
    files.append( ["h264ref", "hintgc-spec-h264ref.csv"] )
    files.append( ["hmmr", "hintgc-spec-hmmr.csv"] )
    files.append( ["lbm", "hintgc-spec-lbm.csv"] )
files.append( ["libquantum", "hintgc-spec-libquantum.csv"] )
if not only_libquantum:
    files.append( ["mcf", "hintgc-spec-mcf.csv"] )
    files.append( ["milc", "hintgc-spec-milc.csv"] )
    files.append( ["perlbench", "hintgc-spec-perlbench.csv"] )
    files.append( ["sjeng", "hintgc-spec-sjeng.csv"] )
    files.append( ["sphinx3", "hintgc-spec-sphinx3.csv"] )
#unknown cause
#files.append( ["wrf", "hintgc-spec-wrf.csv"] )

def assert_all_equal(data):
    if len(data):
        c = data[0]
        for d in data:
            if not d == c:
                print "Not equal"
                print d
                print c
                assert False

def coerce_to_int(data):
    return [int(x) for x in data]

def format_ratio(num):
    if num == "n/a":
        return num
    num = float(num)
    return '%.2f' % num; 

def format_time(num):
    num = float(num)
    s = '%2.2f' % num; 
    if s.startswith("0."):
        s = " "+s
    return s


def format_bytes(num):
    units = ["bytes", "KB", "MB", "GB", "TB", "PB"] 
    num = float(num)
    if num == 0:
        return "0 bytes"
    p = 0
    while num >= 1000:
        num = num / 1000
        p = p + 1
    return ("%.2f" % num) + " " + units[p]

    
rows = []
for mb in files:
    def gc_flag(row):
        return int(row[5])


    # these data files are not mixed
    pff = input_location + mb[1]
    #gcf = "../benchmarks/results/spec-test-gc/" + mb[1]
    flags = hintgc_format.get_column(pff, gc_flag)
    assert_all_equal(flags);
    assert flags[0] == 0
#    flags = hintgc_format.get_column(gcf, gc_flag)
#    assert_all_equal(flags);
#    assert flags[0] == 1

    scratch  = hintgc_format.get_column(pff, hintgc_format.subject_reclaimed)
    scratch = [int(x) for x in scratch]
    reclaimed = sum(scratch)

    scratch  = hintgc_format.get_column(pff, hintgc_format.amount_hinted)
    scratch = [int(x) for x in scratch]
    hinted = sum(scratch)
    hinted_a = scratch

    scratch  = hintgc_format.get_column(pff, hintgc_format.followup_reclaimed)
    scratch = [int(x) for x in scratch]
    leaked = sum(scratch)
    leaked_a = scratch

    def helper(x):
        if x[0] != 0:
            return x[1]
        else:
            return 0;

        # don't both with the filter, doesn't change the result, and makes
        # it more confusing
    # filter leak by nop rows
        #leaked = sum( [helper(x) for x in zip(hinted_a, leaked_a)] )

    of_hinted = "n/a"
    if hinted != 0:
        of_hinted = format_ratio(100*float(reclaimed)/float(hinted))

    if of_hinted == "n/a":
        of_hinted = "0"

    
    leaked_p = "n/a"
    total = leaked+reclaimed
    if total != 0:
        leaked_p = format_ratio(100*float(leaked)/float(total))
    
    if leaked_p == "n/a":
        leaked_p = "0"
   
    row = [mb[0], format_bytes(hinted), format_bytes(reclaimed), 
           of_hinted, format_bytes(leaked), leaked_p]

#    row = [mb[0], heap_size, format_time(gc), format_time(pf), speedup, format_time(base), format_time(no_oc), format_time(pf_none), format_time(pf_range), format_time(pf_both) ]
    rows.append( row )

x = [row[0] for row in rows]
y1 = [row[3] for row in rows]
y2 = [row[5] for row in rows]

plot_stacked_histogram("generated/spec-"+version+"-mem-graph", x, y1, y2, "", "")
