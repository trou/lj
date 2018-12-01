CC:=gcc
COPTS:=-O2 -Wall -Werror -fPIC -std=gnu11

lj.so: lj.c
	$(CC) $(COPTS) -shared -ljbig -o lj.so lj.c

test_jbig: test_jbig.c
	$(CC) $(COPTS) -o test_jbig -ldl test_jbig.c
	
