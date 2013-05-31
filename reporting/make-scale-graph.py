import math
import csv
import hintgc_format

#todo: create a plot2d_diff function
#todo: extract diff helper functions
#todo: extract filter functions

# yselect is used on both data sets, xselect must be constant
def plot2d_data(rootname, x, y, y2, xlabel, ylabel):
    ofile = open(rootname + ".dat", 'w')
    assert len(x) == len(y)
    assert len(x) == len(y2)
    for i in range(len(x)):
        dat = [str(x[i]), str(y[i]), str(y2[i])]
        ofile.write( ' '.join( dat ) )
        ofile.write( '\n' )
    
    with open(rootname + ".plt", 'w') as pltfile:
        pltfile.write("set terminal epslatex size 3,1.5 font \",10\"\n");
#"set term epslatex\n")
        pltfile.write("set output '"+rootname+".tex'\n")
        pltfile.write("set key right bottom\n")
        pltfile.write("set xlabel \""+xlabel+"\"\n")
        pltfile.write("set xtics (0,1000,5000,8000)\n");
        pltfile.write("set ylabel \""+ylabel+"\"\n")
#        pltfile.write("set log x\n")
#        pltfile.write("set log y\n")
#        pltfile.write("set format x \"%.0sx10\^%T\"\n")
        pltfile.write("plot \""+rootname+".dat\" using 1:2 title \"BDW\" lt rgb \"red\", \""+rootname+".dat\" using 1:3 title \"Hinted\" lt rgb \"blue\"\n")

    with open("generated/commands.sh", "a") as cmds:
        cmds.write("gnuplot "+rootname+".plt && epstopdf "+rootname+".eps\n")

# zselect is used on both data sets, xselect, yselect must be constant
def plot3d_data(rootname, x, y, z, xlabel, ylabel, zlabel):
    ofile = open(rootname + ".dat", 'w')
    assert len(x) == len(y)
    assert len(y) == len(z)
    for i in range(len(x)):
#        dat = [str(x[i]),str(y[i]),str(z[i]), str(z2[i])
        dat = [str(x[i]),str(y[i]),str(z[i])]
        ofile.write( ' '.join( dat ) )
        ofile.write( '\n' )
    
    with open(rootname + ".plt", 'w') as pltfile:
        pltfile.write("set term epslatex\n")
        pltfile.write("set output '"+rootname+".tex'\n")
        pltfile.write("set xlabel \""+xlabel+"\"\n")
        pltfile.write("set ylabel \""+ylabel+"\"\n")
        pltfile.write("set zlabel \""+zlabel+"\"\n")
        pltfile.write("set log x\n")
#        pltfile.write("set log z\n")
        pltfile.write("splot \""+rootname+".dat\" using 1:2:3 title \"GC\" lt rgb \"red\", \""+rootname+".dat\" using 1:2:4 title \"PF\" lt rgb \"blue\"\n")

    with open("generated/commands.sh", "a") as cmds:
        cmds.write("gnuplot "+rootname+".plt && epstopdf "+rootname+".eps\n")

# needs to work for csvreader which has no length
def in_groups_of_two(l):
    """ Yield successive n-sized chunks from l.
    """
    i = 0
    rval = [[],[]]
    for row in l:
        rval[ i % 2] = row;
        if i % 2 != 0: 
            yield rval;
        i+=1

# zselect is used on both data sets, xselect, yselect must be constant
def plot3d_diff(datafile, rootname, xselect, yselect, zselect):
    ofile = open(rootname + ".dat", 'w')
    with open(datafile, 'rb') as csvfile:
        spamreader = csv.reader(csvfile)
        for [row, row2] in in_groups_of_two(spamreader):
            assert len(row) == 10
            assert len(row2) == 10;
            row = map(lambda x: x.strip(), row );
            row2 = map(lambda x: x.strip(), row2 );
            assert int(row[6-1]) == 1
            assert int(row2[6-1]) == 0

            # these should match
            assert xselect(row) == xselect(row2)
            assert yselect(row) == yselect(row2)
            # NOTE: artifacts from -1 default, need to filter!

            if float(zselect(row)) == -1 or float(zselect(row2)) == -1:
                print "skipping row"
                continue

            if float(hintgc_format.runtime(row)) < 20 or float(hintgc_format.runtime(row2)) < 20:
                print "skipping small data points"
                continue

                
            # somewhat hacky, rewrite after refactor
            dat = [xselect(row), yselect(row), zselect(row)]
            dat[2] = str( (float(dat[2]) - float(zselect(row2)))/float(dat[2]) ) 
            if float(dat[2]) < -1.0 or float(dat[2]) > 1.0:
                print dat
                print row
                print row2
                assert False

            ofile.write( ' '.join( dat ) )
            ofile.write( '\n' )
    
    with open(rootname + ".plt", 'w') as pltfile:
        pltfile.write("set term epslatex\n")
        pltfile.write("set output '"+rootname+".tex'\n")
        pltfile.write("set xlabel \""+hintgc_format.labels[xselect]+"\"\n")
        pltfile.write("set ylabel \""+hintgc_format.labels[yselect]+"\"\n")
        pltfile.write("set zlabel \""+hintgc_format.labels[zselect]+"\"\n")
        pltfile.write("set log x\n")
        pltfile.write("set zrange [-.1:1]\n")
        
        pltfile.write("splot \""+rootname+".dat\" using 1:2:3\n")

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

f = "../benchmarks/micro/results/laptop-1p/percentage-scale.csv"
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

scratch  = hintgc_format.get_column_pf(f, hintgc_format.percent_pf)
scratch = [float(x) for x in scratch]
frac = scratch
def bucket_to_match(x):
    if x < 0.5:
        return 0
    elif x < 3:
        return 1
    elif x < 7.5:
        return 5
    elif x < 17.5:
        return 10;
    elif x < 37.5:
        return 25
    elif x < 62.5:
        return 50
    elif x < 82.5:
        return 75
    elif x < 95:
        return 90
    else:
        return 100
#frac = [bucket_to_match(x) for x in frac]


scratch  = hintgc_format.get_column_pf(f, hintgc_format.runtime)
scratch = [int(x) for x in scratch]
pfrun = scratch

scratch  = hintgc_format.get_column_gc(f, hintgc_format.runtime)
scratch = [int(x) for x in scratch]
gcrun = scratch

together = zip(frac, heap_size, pfrun, gcrun)
# ruthless pruning
# in the version of the code that ran this, the stat code was slightly broken
# manually prune out some garbage data from bad fp math
def cond10(x):
    if x[1] < 100000000:
        return x[0] < 13 and x[1] > 8
    else:
        return x[0] < 10.5 and x[0] > 9.5
def cond25(x):
    if x[1] < 100000000:
        return x[0] < 35 and x[1] > 15
    else:
        return x[0] < 25.5 and x[0] > 24.5

def cond50(x):
    if x[1] < 100000000:
        return x[0] < 62 and x[1] > 40
    else:
        return x[0] < 51 and x[0] > 49

orig_together = together

together = filter( cond10, orig_together)
together = sorted(together, key=lambda x: x[1]) # by size

x = [t[1]/1000000 for t in together]
y2 = [t[2] for t in together]
y1 = [t[3] for t in together]
#print x
#print y1
#print y2

plot2d_data("generated/scale-graph-laptop-1p-10perc", x, y1, y2, "Heap Size (MB)", "Time (msec)")

together = filter( cond25, orig_together)
together = sorted(together, key=lambda x: x[1]) # by size

x = [t[1]/1000000 for t in together]
y2 = [t[2] for t in together]
y1 = [t[3] for t in together]
#print x
#print y1
#print y2

plot2d_data("generated/scale-graph-laptop-1p-25perc", x, y1, y2, "Heap Size (MB)", "Time (msec)")

together = filter( cond50, orig_together)
together = sorted(together, key=lambda x: x[1]) # by size

x = [t[1]/1000000 for t in together]
y2 = [t[2] for t in together]
y1 = [t[3] for t in together]
#print x
#print y1
#print y2

plot2d_data("generated/scale-graph-laptop-1p-50perc", x, y1, y2, "Heap Size (MB)", "Time (msec)")


#diff = [b/a for a,b in zip(gcrun,pfrun)]

#plot3d_data("generated/scale-graph1", heap_size, frac, diff, "","","")
#plot3d_data("generated/scale-graph2", heap_size, frac, gcrun, "","","")

# group them by perc
def group_by(together):
    together = sorted(together, key=lambda x: x[0])
    last = None
    sizes = []
    running = []
    for row in together:
        if last == None:
            pass
        elif row[0] != last:
            sizes.append( running )
            running = []
        last = row[0]
        running.append(row)
    return sizes




    














