CC=gcc
CFLAGS = -g -std=c99 -pedantic -Wall

CFLAGS += -I ../../linuxcan/include
LDFLAGS += -L ../../linuxcan/canlib
LDFLAGS += -l canlib

%.o:%.c
	$(CC) $(CFLAGS) -c $<

all: fd-tol-tester

fd-tol-tester: fd-tol-tester.o
	$(CC) $(CFLAGS) $(LDFLAGS) -L. $^ -o $@


.PHONY : depend dep all

dep:
	$(CC) $(CFLAGS) $(CPPFLAGS) -w -E -M *.c \
	  > depend

depend:
	@touch depend

clean:
	rm -f *.o *.a fd-tol-tester depend

-include depend
