webteld: src/webteld.o src/datathread.o src/navwebtel.o src/journal.o
	cc -g -Wall -o webteld src/webteld.o src/datathread.o src/journal.o src/navwebtel.o -lssh

src/navwebtel.o : src/navwebtel.c src/navwebtel.h
	cc -g -Wall -c src/navwebtel.c -o src/navwebtel.o

src/datathread.o: src/datathread.c src/datathread.h
	cc -g -Wall -c src/datathread.c -o src/datathread.o

src/webteld.o : src/webteld.c
	cc -g -Wall -c src/webteld.c -o src/webteld.o

src/journal.o: src/journal.c src/journal.h
	cc -g -Wall -c src/journal.c -o src/journal.o

clean:
	env rm -f src/*.o webteld core *.csv
