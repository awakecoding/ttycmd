
all: ttycmd

ttycmd: ttycmd.o
	gcc ttycmd.o -o ttycmd -lpthread -lbluetooth `pkg-config --libs opencv`

ttycmd.o: ttycmd.c
	gcc -c `pkg-config --cflags opencv` ttycmd.c `pkg-config --libs opencv`

clean:
	rm *.o ttycmd

