.PHONY: clean distribution

OBJECTS=throughput

all: $(OBJECTS)

throughput: throughput.c ../../../include/ck_barrier.h ../../../src/ck_barrier_centralized.c
	$(CC) $(CFLAGS) -o throughput throughput.c ../../../src/ck_barrier_centralized.c

clean:
	rm -rf *.dSYM *~ *.o $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE -pthread 

