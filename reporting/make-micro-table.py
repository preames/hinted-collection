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

import latex_helper


files = []
if True:
    files.append( ["Linked List (Dead, Hinted)", "microbench_single_1Mll_dead_hinted.csv"] )
    files.append( ["Linked List (Dead, Unhinted)", "microbench_single_1Mll_dead_unhinted.csv"] )
    files.append( ["Linked List (Live, Hinted)", "microbench_single_1Mll_live_hinted.csv"] )
    files.append( ["Linked List (Live, Unhinted)", "microbench_single_1Mll_live_unhinted.csv"] )
    files.append( ["Fan In (Dead, Hinted)", "microbench_fan_in_dead_hinted.csv"] )
    files.append( ["Fan In (Live, Hinted)", "microbench_fan_in_live_hinted.csv"] )
    files.append( ["Fan In (Live, Unhinted)", "microbench_fan_in_live_unhinted.csv"] )

    files.append( ["2560 x 1k element LL", "microbench_2560_by_100ll.csv"] )
    files.append( ["256 x 10k element LL", "microbench_256_by_10kll.csv"] )
    files.append( ["1/3 Cleanup", "microbench_6x1Mll_partial_cleanup.csv"] )
    files.append( ["Deep Turnover", "microbench_deep_turnover.csv"] )
    
    
#files.append( ["", "microbench_single_10kll_dead_hinted.csv"] )
#files.append( ["", "microbench_single_10kll_dead_unhinted.csv"] )
#files.append( ["", "microbench_single_10kll_live_hinted.csv"] )
#files.append( ["", "microbench_single_10kll_live_unhinted.csv"] )
    #broken
    files.append( ["Unbalanced (Live)", "microbench_unbalenced_live.csv"] )
    files.append( ["Unbalanced (Partly Dead)", "microbench_unbalenced_part_dead.csv"] )
    #files.append( ["VP Array", "microbench_vp_array.csv"] )
    #files.append( ["VP LL", "microbench_vp_ll.csv"] )
    #files.append( ["Divergence", "microbench_divergence.csv"] )


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
        s = "~"+s
    return s

# where are the data files stored?
input_location = sys.argv[1];
import os
assert os.path.exists(input_location)    

config = sys.argv[2]
assert config in ["all", "basic", "opts"]

rows = []
for mb in files:
    if os.path.exists(input_location + "edge-filtered-header/"):
        f = input_location + "edge-filtered-header/" + mb[1]
    else:
        f = input_location + mb[1]
    assert os.path.exists(f)
    #print mb
    heap_size = hintgc_format.get_column(f, hintgc_format.heap_size)
    heap_size = [int(x) for x in heap_size]

    #print heap_size
    # they're only _mostly_ equal - fragementation (FIXME)
    #assert_all_equal(heap_size)

    size_map = { 33398784.0 : "31.8 MB", 24899584.0 : "23.7 MB", 92938240.0 : "88.6 MB", 193601536.0 : "184.6 MB", 59383808.0 : "56.6 MB", 780804096.0 : "744.6 MB", 27119616.0 : "25.9 MB", 29339648.0 : "28 MB" }

    heap_size = size_map[numpy.mean(heap_size)]
    

    def gc_flag(row):
        return int(row[5])

    flags = hintgc_format.get_column_pf(f, gc_flag)
    assert_all_equal(flags);
    assert flags[0] == 0
    flags = hintgc_format.get_column_gc(f, gc_flag)
    assert_all_equal(flags);
    assert flags[0] == 1
    
    pfruntime = hintgc_format.get_column_pf(f, hintgc_format.runtime)
    pfruntime = [int(x) for x in pfruntime]
    
    gcruntime = hintgc_format.get_column_gc(f, hintgc_format.runtime)
    gcruntime = [int(x) for x in gcruntime]
    #print pfruntime
    #print gcruntime
    gc = numpy.mean(gcruntime)
    pf = numpy.mean(pfruntime)
    speedup = "n/a"
    if pf != 0:
        speedup = gc/pf
    speedup = "\\textbf{" + format_ratio(speedup) + "}"

#    f = input_location + "parallel/" + mb[1]
#    scratch = hintgc_format.get_column_pf(f, hintgc_format.runtime)
#    scratch = [int(x) for x in scratch]
#    pf_par = numpy.mean(scratch)

    if config in ["all", "opts"]:

        f = input_location + "edge-filtered-none/" + mb[1]
        scratch = hintgc_format.get_column_pf(f, hintgc_format.runtime)
        scratch = [int(x) for x in scratch]
        pf_none = numpy.mean(scratch)
        f = input_location + "edge-filtered-range/" + mb[1]
        scratch = hintgc_format.get_column_pf(f, hintgc_format.runtime)
        scratch = [int(x) for x in scratch]
        pf_range = numpy.mean(scratch)
        f = input_location + "edge-filtered-both/" + mb[1]
        scratch = hintgc_format.get_column_pf(f, hintgc_format.runtime)
        scratch = [int(x) for x in scratch]
        pf_both = numpy.mean(scratch)

        f = input_location + "no-combine/" + mb[1]
        scratch = hintgc_format.get_column_pf(f, hintgc_format.runtime)
        scratch = [int(x) for x in scratch]
        no_oc = numpy.mean(scratch)
        
        f = input_location + "neither/" + mb[1]
        scratch = hintgc_format.get_column_pf(f, hintgc_format.runtime)
        scratch = [int(x) for x in scratch]
        base = numpy.mean(scratch)
        
    if config == "all":
        row = [mb[0], heap_size, format_time(gc), format_time(pf),speedup, format_time(no_oc), format_time(pf_none), format_time(pf_range), format_time(pf_both), format_time(base) ]
    elif config == "basic":
        row = [mb[0], heap_size, format_time(gc), format_time(pf), speedup ]
    elif config == "opts":
        row = [mb[0], heap_size, format_time(base), format_time(no_oc), format_time(pf_none), format_time(pf), format_time(pf_range), format_time(pf_both) ]
    else:
        assert False
    rows.append( row )

if config == "all":
    cols = [1,1,0,1,1,0,1,0,1,1,1,1,1]
    headers = ["benchmark", "heap size", "gc", "hintgc", "\\textbf{speedup}", "no-oc", "no-ef", "range", "both", "base"]
elif config == "basic":
    cols = [1,1,0,1,1,0,1]
    headers = ["benchmark", "heap size", "gc", "hintgc", "\\textbf{speedup}"]
elif config == "opts":
    cols = [1,1,0,1,0,1,0,1,1,1,1]
    headers = ["benchmark", "heap size", "base", "no-oc", "no-ef", "header", "range", "both"]
else:
    assert False

latex_helper.write_tex_table(cols, headers, rows)
    

