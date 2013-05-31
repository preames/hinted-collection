import hintgc_format

#todo: create a plot2d_diff function
#todo: extract diff helper functions
#todo: extract filter functions

import numpy
import scipy
import scipy.stats

import os.path;
import sys;

if __name__ != "__main__":
    exit(0);

import sys

if len(sys.argv) not in [2,3]:
    print "Illegal arguments to script"
    print 'Number of arguments:', len(sys.argv), 'arguments.'
    print 'Argument List:', str(sys.argv)
    exit()    

# where are the data files stored?
input_location = sys.argv[1];
import os
assert os.path.exists(input_location)

# This is a hack used for reporting the free hack result
only_libquantum = False
if len(sys.argv) == 3 and sys.argv[2] == "libquantum":
    only_libquantum = True


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


rows = []
for mb in files:
    def gc_flag(row):
        return int(row[5])

    pff = input_location + mb[1]

    flags = hintgc_format.get_column(pff, gc_flag)
    assert_all_equal(flags);
    assert flags[0] == 0
#    flags = hintgc_format.get_column(gcf, gc_flag)
#    assert_all_equal(flags);
#    assert flags[0] == 1

    def get_stats(data):
        data = [float(x) for x in data]
        return [len(data), min(data), max(data), format_time(sum(data)/len(data)),
                scipy.stats.scoreatpercentile(data, 50),
                #scipy.stats.scoreatpercentile(data, 95),
                #scipy.stats.scoreatpercentile(data, 90),
                #scipy.stats.scoreatpercentile(data, 99),
                #scipy.stats.scoreatpercentile(data, 99.9),
                #scipy.stats.scoreatpercentile(data, 99.95),
                #scipy.stats.scoreatpercentile(data, 99.99)
                ]

    pfruntime = hintgc_format.get_column(pff, hintgc_format.runtime)
    pfruntime = [int(x) for x in pfruntime]
    gcruntime = hintgc_format.get_column(pff, hintgc_format.followup_runtime)
    gcruntime = [int(x) for x in gcruntime]
    #print pfruntime
    #print gcruntime
    row = [mb[0]]
    pfstats = get_stats( pfruntime )
    gcstats = get_stats( gcruntime )
    assert pfstats[0] == gcstats[0]
    row.extend( pfstats )
    row.extend( gcstats[1:] )

#    row = [mb[0], heap_size, format_time(gc), format_time(pf), speedup, format_time(base), format_time(no_oc), format_time(pf_none), format_time(pf_range), format_time(pf_both) ]
    rows.append( row )

cols = [1,0,1,0,1,1,1,1,0,1,1,1,1]
headers = ["benchmark", "count", "min", "max", "mean", "median", "min", "max", "mean", "median"]
preheader = "\\multicolumn{2}{|c ||}{}  & \\multicolumn{4}{c ||}{Hinted Collector} & \\multicolumn{4}{c |}{Tracing Collector} \\\\"
latex_helper.write_tex_table(cols, headers, rows, preheader)
    

