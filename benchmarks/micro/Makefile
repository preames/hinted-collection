
# my machine
#export DBGUTILS_FLAGS=/home/reames/Files/projects/dbglib/libdbgutils.so 
#export CXX=clang++


#emerald
export DBGUTILS_FLAGS=

#shared
export LIBGCA_LOC=../../boehm-gc-7.2/build/release/.libs/libgc.a
export LIBPFA_LOC=../../hintgc/build/release/.libs/libgc.a

all:
# enable this line for baseline direct compare
	$(CXX) -O3 -I../../boehm-gc-7.2/src/include/ huge-heap.cpp $(LIBGCA_LOC) -L/usr/local/lib/ -lpapi  $(DBGUTILS_FLAGS) -lpthread -ldl -o gc.out
	$(CXX) -O3 -DENABLE_PF -I../../hintgc/src/include/ huge-heap.cpp $(LIBPFA_LOC) -L/usr/local/lib/ $(DBGUTILS_FLAGS) -lpthread -ldl  -L/usr/local/lib/ -lpapi -o hintgc.out
	$(CXX) -O3 -I../../boehm-gc-7.2/src/include/ huge-objects.cpp $(LIBGCA_LOC) -L/usr/local/lib/ -lpapi  $(DBGUTILS_FLAGS) -lpthread -ldl -o huge-objects.gc.out
	$(CXX) -O3 -DENABLE_PF -I../../hintgc/src/include/ huge-objects.cpp $(LIBPFA_LOC) -L/usr/local/lib/ $(DBGUTILS_FLAGS) -lpthread -ldl  -L/usr/local/lib/ -lpapi -o huge-objects.hintgc.out
	$(CXX) -O3 -DENABLE_PF -I../../hintgc/src/include/ worklist.cxx $(LIBPFA_LOC) -L/usr/local/lib/ -lpapi $(DBGUTILS_FLAGS) -lpthread -ldl -o worklist.out
	$(CXX) -O3 -DENABLE_PF -I../../hintgc/src/include/ gpugc-micro.cxx $(LIBPFA_LOC) -L/usr/local/lib/ -lpapi $(DBGUTILS_FLAGS) -lpthread -ldl -o gpugc-micro.out
	$(CXX) -O3 -DENABLE_PF -I../../hintgc/src/include/ gpugc-micro.cxx -S -emit-llvm 
	gcc test.c ../build/.libs/libgc.so -I../src/include -include gc.h
test:
	time ./gc.out

clean:
	rm -rf gpugc-micro.out gc.out hintgc.out worklist.out
