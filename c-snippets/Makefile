CC = ~/code/ucc/ucc
CFLAGS += -g -U__GNUC__
CPPFLAGS += -I/usr/include/x86_64-linux-gnu/ \
            -I/usr/lib/gcc/x86_64-linux-gnu/4.7/include/ \
            -D__extension__=

.PHONY: all clean

all: autotab-ucc autotab-gcc

autotab-gcc: autotab.c
	gcc -o $@ $<

autotab-ucc: autotab.o
	${CC} ${CFLAGS} ${CPPFLAGS} -o $@ $<

%.o: %.s
	${CC} -c ${CFLAGS} ${CPPFLAGS} -o $@ $<

%.s: %.i
	${CC} -S ${CFLAGS} ${CPPFLAGS} -o $@ -xc $<

%.i: %.c
	cpp -P >$@ <$< ${CPPFLAGS}

clean:
	rm -f *.i *.s *.o autotab-[gu]cc
