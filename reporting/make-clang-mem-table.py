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

def format_int(num):
    num = float(num)
    s = '%d' % num; 
    return s
    

rows = []
if False:
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
    
    leaked_p = "n/a"
    total = leaked+reclaimed
    if total != 0:
        leaked_p = format_ratio(100*float(leaked)/float(total))
   
    row = ["Clang", hinted, reclaimed, of_hinted, leaked, leaked_p]

#    row = [mb[0], heap_size, format_time(gc), format_time(pf), speedup, format_time(base), format_time(no_oc), format_time(pf_none), format_time(pf_range), format_time(pf_both) ]
    rows.append( row )

def get_stats(data):
    data = [float(x) for x in data]
    return [len(data),
            #print sum(data)
            min(data),
            max(data),
            scipy.stats.mstats.gmean(data),
            scipy.stats.scoreatpercentile(data, 50),
            #scipy.stats.scoreatpercentile(data, 95),
            #print scipy.stats.scoreatpercentile(data, 90),
            scipy.stats.scoreatpercentile(data, 99),
            #print scipy.stats.scoreatpercentile(data, 99.9),
            scipy.stats.scoreatpercentile(data, 99.95),
            scipy.stats.scoreatpercentile(data, 99.99),
            ]


def frac_free_data(filename):
    size = hintgc_format.get_column(filename, hintgc_format.heap_size)
    hinted = hintgc_format.get_column(filename, hintgc_format.amount_hinted)
    caught = hintgc_format.get_column(filename, hintgc_format.subject_reclaimed)
    leaked = hintgc_format.get_column(filename, hintgc_format.followup_reclaimed)

    tuples = zip(size, hinted, caught, leaked)

    def denom(row):
        return (max(float(row[2]),0)+max(float(row[3]),0))

    def compute_row(row):
        total = denom(row)
        sub = max(float(row[2]),0)
        fol = max(float(row[3]), 0)
        if total > 0: # artifical limit to avoid bullshit data
            return [sub/total, fol/total]
        else:
            return [0.0, 0.0]

# bin by size, then do that plot - closer to actually useful


    percentages = [ [compute_row(row), row] for row in tuples]
    fol = [row[0][1]*100 for row in percentages] 

    #print fol
    if False:
        def looks_like_garbage(item):
            if float(item[1][1]) != 0:
                if float(item[1][2])/float(item[1][1]) > .9 and int(item[1][3]) <= 30000:
                    return True
            else:
                return False
        c = 0
        print "Details"
        for item in percentages:
            if item[0][1]*100 > 20 and not looks_like_garbage(item):
                c += 1
                print item
        print c
    if True:
        def looks_like_garbage(item):
            if float(item[1][1]) != 0:
                if int(item[1][3]) <= 100000000:
                    return True
            else:
                return False
        c = 0
        print "Details"
        for item in percentages:
            if item[0][1]*100 > 20 and not looks_like_garbage(item):
                c += 1
                print item
        print c


    return get_stats(fol)

filename = "../benchmarks/results/clang_build_pf_stats-release.csv"
stats = frac_free_data(filename)
stats = [format_ratio(x) for x in stats[1:]]
rows.append( stats )


cols = [1,1,1,1,1,1,1]
headers = ["min", "max", "gmean", "median", "99th", "99.95", "99.99"]
latex_helper.write_tex_table(cols, headers, rows)

# manual test and confirm
if False:
    hinted = hintgc_format.get_column(filename, hintgc_format.amount_hinted)
    hinted = [float(x) for x in hinted]
    caught = hintgc_format.get_column(filename, hintgc_format.subject_reclaimed)
    caught = [float(x) for x in caught]
    leaked = hintgc_format.get_column(filename, hintgc_format.followup_reclaimed)
    leaked = [float(x) for x in leaked]
    
    print sum(hinted)
    print sum(caught)
    print sum(caught)/sum(hinted)
    print sum(leaked)
    print sum(caught) + sum(leaked)
    print sum(leaked)/(sum(caught) + sum(leaked))
