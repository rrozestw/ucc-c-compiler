include ../src_config.mk
include config.mk

DEP_FILE = Makefile.deps

OBJ     = sdb.o util.o tracee.o prompt.o arch/arch.o cmds.o \
          breakpoint.o signal.o \
          arch/${ARCH}/arch.o os/${OS_NAME}/ptrace.o os/${OS_NAME}/os.o \
          ../util/dynarray.o ../util/alloc.o

CFLAGS  += -Wno-unused-parameter -Wno-override-init -Wmissing-prototypes # until merged into ../
CFLAGS2  = $(filter-out -pedantic,$(CFLAGS))
CPPFLAGS += -D_XOPEN_SOURCE -Iarch/${ARCH}

sdb: ${OBJ}
	${CC} ${LDFLAGS} -o $@ ${OBJ}

.c.o:
	${CC} ${CPPFLAGS} ${CFLAGS2} -c -o $@ $<

simple: simple.s
	as -o simple.o simple.s
	ld -Ttext=0x200000 -o simple simple.o

clean:
	rm -f ${OBJ} sdb

deps:
	cc ${CPPFLAGS} -MM *.c > ${DEP_FILE}
	PRE=arch/         ; cc ${CPPFLAGS} -MM $$PRE/*.c | sed 's;^;'$$PRE';' >> ${DEP_FILE}
	PRE=os/${OS_NAME}/; cc ${CPPFLAGS} -MM $$PRE/*.c | sed 's;^;'$$PRE';' >> ${DEP_FILE}

.PHONY: clean

include ${DEP_FILE}
