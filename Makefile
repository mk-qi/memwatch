# Makefile for memwatch
# use this file to build for Mac OS X or Linux systems

CC=gcc
OBJECTS=memwatch.o vmmap.o memory_search.o search_data.o vmmap_data.o process_utils.o parse_utils.o freeze_region.o search_data_list.o
CFLAGS=-g -Wall
LDFLAGS=-lreadline

all: memwatch value_test value_test_float

install: memwatch
	cp memwatch /usr/bin/memwatch

memwatch: $(OBJECTS)
	g++ $(LDFLAGS) -o memwatch $^

value_test: value_test.o
	g++ $(LDFLAGS) -o value_test $^

value_test_float: value_test_float.o
	g++ $(LDFLAGS) -o value_test_float $^

.cpp.o:
	g++ $(CFLAGS) -c $<

clean:
	-rm *.o memwatch value_test value_test_float

.PHONY: clean
