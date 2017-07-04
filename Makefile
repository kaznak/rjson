
CC=gcc -Wall -O3
# CC=gcc -Wall -O3 -static

all : rjson
	strip rjson

rjson : rjson.c

clean :
	rm -f rjson
	rm -f *.o
