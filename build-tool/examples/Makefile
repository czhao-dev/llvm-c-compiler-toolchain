.PHONY: all clean

all: app

app: main.o utils.o
	cc -o app main.o utils.o

main.o: main.c common.h
	cc -c main.c

utils.o: utils.c common.h
	cc -c utils.c

clean:
	rm -f app main.o utils.o
