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
    

rows = []
if True:
    def gc_flag(row):
        return int(row[5])


    # these data files are not mixed
    pff = "../benchmarks/results/clang_build_pf_stats-release.csv"
    gcf = "../benchmarks/results/clang_build_gc_stats-release.csv"
    flags = hintgc_format.get_column(pff, gc_flag)
    assert_all_equal(flags);
    assert flags[0] == 0
    flags = hintgc_format.get_column(gcf, gc_flag)
    assert_all_equal(flags);
    assert flags[0] == 1

    def get_stats(data):
        data = [float(x) for x in data]
        return [len(data), min(data), max(data), format_time(sum(data)/len(data)),
                scipy.stats.scoreatpercentile(data, 50),
                scipy.stats.scoreatpercentile(data, 95),
                #scipy.stats.scoreatpercentile(data, 90),
                scipy.stats.scoreatpercentile(data, 99),
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
    rows.append( ["Hinted"] + get_stats(pfruntime)[1:] )
    rows.append( ["Tracing"] + get_stats(gcruntime)[1:] )

cols = [1,0,1,1,1,1,1,1]
headers = ["version", "min", "max", "mean", "median", "95th", "99th"]
latex_helper.write_tex_table(cols, headers, rows)
    

