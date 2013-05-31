

all: build check

build:
	make -C release -j 3
	make -C release+asserts -j 3
	make -C debug -j 3

check:
	make -C release check
	make -C release+asserts check
	make -C debug check

clean:
	make -C release clean
	make -C release+asserts clean
	make -C debug clean

clobber:
	rm -rf release/*
	rm -rf release+asserts/*
	rm -rf debug/*
