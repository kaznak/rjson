
CC=gcc -Wall -O3 

rjson : rjson.o

clean :
	rm -f rjson
	rm -f *.o
