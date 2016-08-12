CC = ./ucc

a: a.o
	${CC} -o $@ $< -v

a.o: a.c
	${CC} -o $@ $< -c
