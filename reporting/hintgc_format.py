

# gc num, heap size (bytes), pages dirty, pages live, bytes pending free, gc/pf,  ms, collected (bytes), followup ms, missed

def heap_size(row):
    return row[1]

def runtime(row):
    return row[6]

def amount_hinted(row):
    return row[4];

def percent_freed(row):
    return str( float(amount_hinted(row))/float(heap_size(row)) )

def percent_pages_pf(row):
    return str(100*float(row[2])/float(row[3]));

def percent_pf(row):
    return str(100*float(row[4])/float(row[1]));

def subject_reclaimed(row):
    return row[7]

def followup_runtime(row):
    return row[8]

def followup_reclaimed(row):
    return row[9]

def reclaimed_per_second(row):
    if int(row[6]) == 0:
        return "-1" 
    return str(float(row[7])/float(row[6]))

labels = dict()
labels[runtime] = "Runtime (sec)"
labels[heap_size] = "Heap Size (bytes)"
labels[percent_pages_pf] = "Percent of Pages w/PF"
labels[percent_pf] = "Percent of Heap PF"
labels[percent_freed] = "Percent Hinted"
labels[reclaimed_per_second] = "bytes reclaimed per second"



def bin_column(filename, xselect):
    with open(filename, 'rb') as csvfile:
        spamreader = csv.reader(csvfile)
        # bin the data
        bins = dict()
        for row in spamreader:
            assert len(row) == 10
            row = map(lambda x: x.strip(), row );

            if xselect(row) not in bins:
                bins[xselect(row)] = 0
            bins[xselect(row)] += 1
        return bins

import csv

def perc_bin_column(filename, xselect):
    with open(filename, 'rb') as csvfile:
        spamreader = csv.reader(csvfile)
        # bin the data
        bins = dict()
        for row in spamreader:
            assert len(row) == 10
            row = map(lambda x: x.strip(), row );

            val = xselect(row)
            f = float(val)
#            if f < .07:
 #               continue
            import math
            i = int(math.floor(f*100))
            if str(i) not in bins:
                bins[str(i)] = 0
            bins[str(i)] += 1
        return bins

def get_column(filename, xselect):
    with open(filename, 'rb') as csvfile:
        spamreader = csv.reader(csvfile)
        # bin the data
        rval = []
        for row in spamreader:
            assert len(row) == 10
            row = map(lambda x: x.strip(), row );

            rval.append( xselect(row) )
        return rval

# these two are purely helpers to handle alternating lines cleanly
def get_column_pf(filename, xselect):
    data = get_column(filename, xselect)
    return data[0::2]

def get_column_gc(filename, xselect):
    data = get_column(filename, xselect)
    if len(data):
        return data[1::2]
    return []
