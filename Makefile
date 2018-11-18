CC:=gcc
COPTS:=-O2 -Wall -Werror -fPIC

lj.so: lj.c
	$(CC) $(COPTS) -shared -ljbig lj.c
