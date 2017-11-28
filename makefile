CC = gcc
DEBUG = -g

mysh: mysh.o
	$(CC) $(DEBUG) -o mysh mysh.o

mysh.o: mysh.c mysh.h
	$(CC) $(DEBUG) -c mysh.c