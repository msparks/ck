.PHONY: clean

all: ck_ticket ck_mcs ck_dec ck_cas ck_fas ck_clh linux_spinlock \
     ck_ticket_pb ck_anderson

linux_spinlock: linux_spinlock.c
	$(CC) $(CFLAGS) -o linux_spinlock linux_spinlock.c

ck_ticket_pb: ck_ticket_pb.c
	$(CC) $(CFLAGS) -o ck_ticket_pb ck_ticket_pb.c

ck_clh: ck_clh.c
	$(CC) $(CFLAGS) -o ck_clh ck_clh.c

ck_anderson: ck_anderson.c
	$(CC) $(CFLAGS) -o ck_anderson ck_anderson.c

ck_fas: ck_fas.c
	$(CC) $(CFLAGS) -o ck_fas ck_fas.c

ck_ticket: ck_ticket.c
	$(CC) $(CFLAGS) -o ck_ticket ck_ticket.c

ck_cas: ck_cas.c
	$(CC) $(CFLAGS) -o ck_cas ck_cas.c

ck_mcs: ck_mcs.c
	$(CC) $(CFLAGS) -o ck_mcs ck_mcs.c

ck_dec: ck_dec.c
	$(CC) $(CFLAGS) -o ck_dec ck_dec.c

clean:
	rm -rf ck_ticket ck_mcs ck_dec ck_cas ck_fas ck_clh linux_spinlock ck_ticket_pb \
		ck_anderson test_and_set *.dSYM

include ../../../build/regressions.build
CFLAGS+=-pthread -D_GNU_SOURCE -lm
