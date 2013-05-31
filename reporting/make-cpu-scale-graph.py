# This version of the script is tuned for the results generated around the
# thesis time.  The format and assumptions of the data change slightly.

import math
import csv
import hintgc_format

#todo: create a plot2d_diff function
#todo: extract diff helper functions
#todo: extract filter functions

# yselect is used on both data sets, xselect must be constant
def plot2d_data(rootname, x, y, y2, xlabel, ylabel, havekey = False):
    ofile = open(rootname + ".dat", 'w')
    assert len(x) == len(y)
    assert len(x) == len(y2)
    for i in range(len(x)):
        dat = [str(x[i]), str(y[i]), str(y2[i])]
        ofile.write( ' '.join( dat ) )
        ofile.write( '\n' )
    
    with open(rootname + ".plt", 'w') as pltfile:
        pltfile.write("set terminal epslatex size 3,1.4 font \",10\"\n");
#"set term epslatex\n")
        pltfile.write("set output '"+rootname+".tex'\n")
        if havekey:
            pltfile.write("set key above horizontal\n")
        else:
            pltfile.write("set key off\n")
        pltfile.write("set xlabel \""+xlabel+"\"\n")

        high = 100*(1+max(max(y1),max(y2))/100)
        low = 100*(-1+min(min(y1),min(y2))/100)
        if( high - low >= 1000):
            high += 100

        pltfile.write("set yrange ["+str(low)+":"+str(high)+"]\n");

        if high-low < 500:
            pltfile.write("set ytics "+str(low)+", 100 \n")
        elif high-low < 1000:
            pltfile.write("set ytics "+str(low)+", 250 \n")
        elif high-low < 2000:
            pltfile.write("set ytics "+str(low)+", 500 \n")
        else:
            pltfile.write("set ytics "+str(low)+", 1000 \n")

#        if max(max(y1),max(y2)) < 1000:
#            pltfile.write("set ytics (0,250,500,750)\n");
#        else:
#            pltfile.write("set ytics (0,500,1000,1500,2000)\n");
        pltfile.write("set ylabel \""+ylabel+"\"\n")
#        pltfile.write("set log x\n")
#        pltfile.write("set log y\n")
#        pltfile.write("set format x \"%.0sx10\^%T\"\n")
        pltfile.write("plot \""+rootname+".dat\" using 1:2 title \"Tracing\" lt rgb \"red\", \""+rootname+".dat\" using 1:3 title \"Hinted\" lt rgb \"blue\"\n")

    with open("generated/commands.sh", "a") as cmds:
        cmds.write("gnuplot "+rootname+".plt && epstopdf "+rootname+".eps\n")



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


with open("generated/commands.sh", "a") as cmds:
    cmds.write("# generate graphs tex/eps\n")


import sys
if len(sys.argv) != 3:
    print "Illegal arguments to script"
    print 'Number of arguments:', len(sys.argv), 'arguments.'
    print 'Argument List:', str(sys.argv)
    exit()    

# where are the data files stored?
input_location = sys.argv[1];
import os
assert os.path.exists(input_location)
f = input_location


basename = sys.argv[2]

#print mb
heap_size = hintgc_format.get_column_pf(f, hintgc_format.heap_size)
heap_size = [int(x) for x in heap_size]

#interwoven data
def gc_flag(row):
    return int(row[5])
flags = hintgc_format.get_column_pf(f, gc_flag)
assert_all_equal(flags);
assert flags[0] == 0
flags = hintgc_format.get_column_gc(f, gc_flag)
assert_all_equal(flags);
assert flags[0] == 1

scratch  = hintgc_format.get_column_pf(f, hintgc_format.runtime)
scratch = [int(x) for x in scratch]
pfrun = scratch

scratch  = hintgc_format.get_column_gc(f, hintgc_format.runtime)
scratch = [int(x) for x in scratch]
gcrun = scratch

assert len(pfrun) == len(gcrun)

#print len(pfrun)
#print len(gcrun)

x = [i for i in range(1,len(gcrun)+1)]

y1 = [int(t) for t in gcrun]
y2 = [int(t) for t in pfrun]
#print x
#print y1
#print y2

plot2d_data("generated/scale-cpu-"+basename+"-full", x, y1, y2, "Threads", "Time (msec)")

# Focus on the small end of that
if basename == "laptop":
    pass
elif basename == "emerald":
    x = x[0:64]
    y1 = y1[0:64]
    y2 = y2[0:64]
elif basename == "gainestown":
    x = x[0:20]
    y1 = y1[0:20]
    y2 = y2[0:20]
elif basename == "jktqos":
    x = x[0:64]
    y1 = y1[0:64]
    y2 = y2[0:64]
else:
    assert False

plot2d_data("generated/scale-cpu-"+basename, x, y1, y2, "Threads", "Time (msec)")

# Focus on the small end of that
if basename == "laptop":
    x = x[0:8]
    y1 = y1[0:8]
    y2 = y2[0:8]
elif basename == "emerald":
    x = x[0:32]
    y1 = y1[0:32]
    y2 = y2[0:32]
elif basename == "gainestown":
    x = x[0:8]
    y1 = y1[0:8]
    y2 = y2[0:8]
elif basename == "jktqos":
    x = x[0:32]
    y1 = y1[0:32]
    y2 = y2[0:32]
else:
    assert False
#print x
#print y1
#print y2
plot2d_data("generated/scale-cpu-"+basename+"-capped", x, y1, y2, "Threads", "Time (msec)")


