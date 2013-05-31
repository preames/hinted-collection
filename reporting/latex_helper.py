import os.path;
import sys;

# Currently, only knows how to build single header row rable with dual bars occasionally.
def write_tex_table(cols, headers, rows, preheader = ""):
    assert len(rows) > 0
    numcols = sum(cols)
    # really, number of non-zero entries
    assert len(headers) <= len(cols)
    assert numcols > 1
    for row in rows:
        assert len(row) == numcols

    # stringify every entry
    # insert spaces to avoid empty cell issues
    for i in range(len(rows)):
        for c in range(len(rows[i])):
            rows[i][c] = str(rows[i][c])
            if rows[i][c] == "":
                rows[i][c] = "~"

    sys.stdout.write("\\begin{tabular}{ | l |");
    for col in cols[1:]:
        if col == 1:
            sys.stdout.write(" r |");
        elif col == 0:
            sys.stdout.write("|");
        else:
            assert False # multicol not yet supported
    print(" }");

#    sys.stdout.write("\\begin{tabular}{ | l ||");
#    for col in headers[1:]:
#        sys.stdout.write(" c |");
#    print(" }");
    print("\\hline");
    if preheader != "":
        print preheader
    print( " & ".join(headers) + "\\\\" ) #escaped newline
    print("\\hline");

    for row in rows:
        print " & ".join(row) + "\\\\"
        
    print("\hline");
    print("\end{tabular}");


def unit_test():
    headers = ["name", "data"]
    rows = [ ["a", 1], ["b", 2] ]
    write_tex_table([1,1], headers, rows)
