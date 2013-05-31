

# each block is roughly 1.68 MB

#from subprocess import call
#call(["bash", "./eval-inner.sh", str(30*60), str(50)])
#exit(0)

import sys
if len(sys.argv) != 2:
    print "Illegal arguments to script"
    print 'Number of arguments:', len(sys.argv), 'arguments.'
    print 'Argument List:', str(sys.argv)
    exit()    

mode = sys.argv[1]
assert mode in ["size", "cpus", "numa"]

if mode == "cpus":
    # Note: We want to deliberately oversubscribe here - that's interesting
    num_cores = 64;
    for cpu in range(1,5*num_cores+1):
        from subprocess import call
        call(["bash", "./eval-inner.sh", str(30*60), str(50), str(cpu)])
    exit()
elif mode == "numa":
    for cpu in range(0,4):
        for mem in range(0,4):
            from subprocess import call
            call(["bash", "./numa-inner.sh", str(cpu), str(mem)])
    exit()
else:
    pass # just continue on to rest of code


if False:
    small_steps = [2*x for x in range(1,30)] # inc of 3 mb to 100mb (roughly)
    mid_steps = [60*x for x in range(1, 30)] # inc of 100mb to 3gb
    large_steps = [296*x for x in range(6, 20)] #inc of 500mb to 10gb
    increments = small_steps
    increments.extend(mid_steps)
    increments.extend(large_steps)
    # run the same increments
    for size in increments:
        for dealloc in increments:
            if dealloc > size:
                continue
            from subprocess import call
            call(["bash", "./eval-inner.sh", str(size), str(dealloc)])
else:
    # This is the version used for size and frac alloc scalability
    # on emerald
    mid_steps = [60*x for x in range(2, 30)] # inc of 100mb, 200mb to 3gb
    large_steps = [296*x for x in range(6, 20)] #inc of 500mb to 10gb
    increments = []
    increments.extend(mid_steps)
    increments.extend(large_steps)
    for size in increments:
        for percent in [0,1,5,10,25,50,75,90,100]:
            from subprocess import call
            call(["bash", "./eval-inner.sh", str(size), str(percent), str(4)])
