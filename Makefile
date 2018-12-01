CC:=gcc
COPTS:=-O2 -Wall -Werror -fPIC

lj.so: lj.c
	$(CC) $(COPTS) -shared -ljbig -o lj.so lj.c

test: test.c
	$(CC) $(COPTS) -o test -ldl test.c
	
