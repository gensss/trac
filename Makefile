CC=gcc

trac :
	$(CC) -Wall -O2 -o tracd tracd.c 
	$(CC) -Wall -O2 -o trac trac.c 

static :
	$(CC) -Wall -static -O2 -o tracd tracd.c 
	$(CC) -Wall -static -O2 -o trac trac.c 

clean :
	rm trac tracd