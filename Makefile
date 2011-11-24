
all: ttycmd

ttycmd: ttycmd.o
	gcc ttycmd.o -o ttycmd -lpthread

ttycmd.o: ttycmd.c
	gcc -c ttycmd.c

clean:
	rm *.o ttycmd

