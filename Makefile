CC:=gcc
COPTS:=-O0 -Wall -Werror -fPIC -std=gnu11 -ggdb

lj.so: lj.c
	$(CC) $(COPTS) -Wl,-init,init -shared -ljbig -o lj.so lj.c

test_jbig: test_jbig.c
	$(CC) $(COPTS) -o test_jbig -ldl test_jbig.c
	
test_jpeg: test_jpeg.c
	$(CC) $(COPTS) -o test_jpeg -ldl test_jpeg.c

all: test_jpeg lj.so test_jbig
