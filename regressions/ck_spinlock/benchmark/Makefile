.PHONY: all clean

OBJECTS=ck_ticket.THROUGHPUT ck_ticket.LATENCY			\
	ck_mcs.THROUGHPUT ck_mcs.LATENCY			\
	ck_dec.THROUGHPUT ck_dec.LATENCY			\
	ck_cas.THROUGHPUT ck_cas.LATENCY			\
	ck_fas.THROUGHPUT ck_fas.LATENCY			\
	ck_clh.THROUGHPUT ck_clh.LATENCY			\
	linux_spinlock.THROUGHPUT linux_spinlock.LATENCY	\
	ck_ticket_pb.THROUGHPUT ck_ticket_pb.LATENCY		\
	ck_anderson.THROUGHPUT ck_anderson.LATENCY

all: $(OBJECTS)

ck_ticket.THROUGHPUT: ck_ticket.c
	$(CC) -DTHROUGHPUT $(CFLAGS) -o ck_ticket.THROUGHPUT ck_ticket.c

ck_ticket.LATENCY: ck_ticket.c
	$(CC) -DLATENCY $(CFLAGS) -o ck_ticket.LATENCY ck_ticket.c

ck_mcs.THROUGHPUT: ck_mcs.c
	$(CC) -DTHROUGHPUT $(CFLAGS) -o ck_mcs.THROUGHPUT ck_mcs.c

ck_mcs.LATENCY: ck_mcs.c
	$(CC) -DLATENCY $(CFLAGS) -o ck_mcs.LATENCY ck_mcs.c

ck_dec.THROUGHPUT: ck_dec.c
	$(CC) -DTHROUGHPUT $(CFLAGS) -o ck_dec.THROUGHPUT ck_dec.c

ck_dec.LATENCY: ck_dec.c
	$(CC) -DLATENCY $(CFLAGS) -o ck_dec.LATENCY ck_dec.c

ck_cas.THROUGHPUT: ck_cas.c
	$(CC) -DTHROUGHPUT $(CFLAGS) -o ck_cas.THROUGHPUT ck_cas.c

ck_cas.LATENCY: ck_cas.c
	$(CC) -DLATENCY $(CFLAGS) -o ck_cas.LATENCY ck_cas.c

ck_fas.THROUGHPUT: ck_fas.c
	$(CC) -DTHROUGHPUT $(CFLAGS) -o ck_fas.THROUGHPUT ck_fas.c

ck_fas.LATENCY: ck_fas.c
	$(CC) -DLATENCY $(CFLAGS) -o ck_fas.LATENCY ck_fas.c

ck_clh.THROUGHPUT: ck_clh.c
	$(CC) -DTHROUGHPUT $(CFLAGS) -o ck_clh.THROUGHPUT ck_clh.c

ck_clh.LATENCY: ck_clh.c
	$(CC) -DLATENCY $(CFLAGS) -o ck_clh.LATENCY ck_clh.c

linux_spinlock.THROUGHPUT: linux_spinlock.c
	$(CC) -DTHROUGHPUT $(CFLAGS) -o linux_spinlock.THROUGHPUT linux_spinlock.c

linux_spinlock.LATENCY: linux_spinlock.c
	$(CC) -DLATENCY $(CFLAGS) -o linux_spinlock.LATENCY linux_spinlock.c

ck_ticket_pb.THROUGHPUT: ck_ticket_pb.c
	$(CC) -DTHROUGHPUT $(CFLAGS) -o ck_ticket_pb.THROUGHPUT ck_ticket_pb.c

ck_ticket_pb.LATENCY: ck_ticket_pb.c
	$(CC) -DLATENCY $(CFLAGS) -o ck_ticket_pb.LATENCY ck_ticket_pb.c

ck_anderson.THROUGHPUT: ck_anderson.c
	$(CC) -DTHROUGHPUT $(CFLAGS) -o ck_anderson.THROUGHPUT ck_anderson.c

ck_anderson.LATENCY: ck_anderson.c
	$(CC) -DLATENCY $(CFLAGS) -o ck_anderson.LATENCY ck_anderson.c

clean:
	rm -rf *.dSYM $(OBJECTS)

include ../../../build/regressions.build
CFLAGS+=-D_GNU_SOURCE -pthread -lm
