import csv
import hintgc_format

#todo: create a plot2d_diff function
#todo: extract diff helper functions
#todo: extract filter functions

# yselect is used on both data sets, xselect must be constant
def plot2d(datafile, rootname, xselect, yselect):
    ofile = open(rootname + ".dat", 'w')
    with open(datafile, 'rb') as csvfile:
        spamreader = csv.reader(csvfile)
        for row in spamreader:
            assert len(row) == 10
            row = map(lambda x: x.strip(), row );

            # rely on nasty scoping to get [x, y_gc, y_pf]
            if int(row[6-1]) == 1: # gc only
                dat = [xselect(row)]
                dat += [yselect(row)]
            elif int(row[6-1]) == 0: # pf
                dat += [yselect(row)]
                #print ' '.join( dat )
                ofile.write( ' '.join( dat ) )
                ofile.write( '\n' )
    
    with open(rootname + ".plt", 'w') as pltfile:
        pltfile.write("set term epslatex\n")
        pltfile.write("set output '"+rootname+".tex'\n")
        pltfile.write("set xlabel \""+hintgc_format.labels[xselect]+"\"\n")
        pltfile.write("set ylabel \""+hintgc_format.labels[yselect]+"\"\n")
        pltfile.write("set log y\n")
        pltfile.write("plot \""+rootname+".dat\" using 1:2 title \"GC\" lt rgb \"red\", \""+rootname+".dat\" using 1:3 title \"PF\" lt rgb \"blue\"\n")

    with open("generated/commands.sh", "a") as cmds:
        cmds.write("gnuplot "+rootname+".plt && epstopdf "+rootname+".eps\n")

# zselect is used on both data sets, xselect, yselect must be constant
def plot3d(datafile, rootname, xselect, yselect, zselect):
    ofile = open(rootname + ".dat", 'w')
    with open(datafile, 'rb') as csvfile:
        spamreader = csv.reader(csvfile)
        for row in spamreader:
            assert len(row) == 10
            row = map(lambda x: x.strip(), row );

            # rely on nasty scoping to get [x, y_gc, y_pf]
            if int(row[6-1]) == 1: # gc only
                dat = [xselect(row)]
                dat += [yselect(row)]
                dat += [zselect(row)]
            elif int(row[6-1]) == 0: # pf
                dat += [zselect(row)]
                #print ' '.join( dat )
                ofile.write( ' '.join( dat ) )
                ofile.write( '\n' )
    
    with open(rootname + ".plt", 'w') as pltfile:
        pltfile.write("set term epslatex\n")
        pltfile.write("set output '"+rootname+".tex'\n")
        pltfile.write("set xlabel \""+hintgc_format.labels[xselect]+"\"\n")
        pltfile.write("set ylabel \""+hintgc_format.labels[yselect]+"\"\n")
        pltfile.write("set zlabel \""+hintgc_format.labels[zselect]+"\"\n")
        pltfile.write("set log x\n")
        pltfile.write("set log z\n")
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


with open("generated/commands.sh", "w") as cmds:
    cmds.write("# generate graphs tex/eps\n")


datafile = '../benchmarks/micro/results/micro-run-merged.csv'

plot2d(datafile, "generated/plot_2d_1", hintgc_format.percent_pages_pf, hintgc_format.reclaimed_per_second)
plot2d(datafile, "generated/plot_2d_2", hintgc_format.percent_pages_pf, hintgc_format.runtime)
plot2d(datafile, "generated/plot_2d_3", hintgc_format.heap_size, hintgc_format.runtime)
plot2d(datafile, "generated/plot_2d_4", hintgc_format.heap_size, hintgc_format.reclaimed_per_second)
plot2d(datafile, "generated/plot_2d_5", hintgc_format.percent_pf, hintgc_format.runtime)
plot2d(datafile, "generated/plot_2d_6", hintgc_format.percent_pf, hintgc_format.reclaimed_per_second)
#plot2d_pf(datafile, "data2", percent_pages_pf, reclaimed_per_second)

plot3d_diff(datafile, "generated/plot_3d_1", hintgc_format.heap_size, hintgc_format.percent_pf, hintgc_format.runtime)
#plot3d_diff(datafile, "generated/plot_3d_1", heap_size, percent_pf, reclaimed_per_second)

# with open('results/micro-run-merged.csv', 'rb') as csvfile:
#     spamreader = csv.reader(csvfile)
#     for row in spamreader:
#         assert len(row) == 10
#         row = map(lambda x: x.strip(), row );

#         if int(row[6-1]) == 1: # gc only
#             print ' '.join( runtime_vs_percent_pages_pf(row) )
#             print ' '.join( rate_for_size_vs_percent_pages_pf(row) )
#             print ', '.join(row)
#             dat1_gc.write( ' '.join( runtime_vs_percent_pages_pf(row) ) )
#             dat1_gc.write( '\n' )
#         else: # pf
#             print ', '.join(row)
#             dat1_pf.write( ' '.join( runtime_vs_percent_pages_pf(row) ) )
#             dat1_pf.write( '\n' )
            
