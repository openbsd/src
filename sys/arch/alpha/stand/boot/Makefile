#	$NetBSD: Makefile,v 1.4 1995/06/28 00:58:44 cgd Exp $
#
# BSD Boot blocks for the Alpha
#
# XXX should generate a primary boot block, too...

INCPATH=-I../../../.. -I.

# Boot relocation address
RELOC1=	20020000
# Low memory test program relocation address
#RELOC2=	20004000
# High memory test program relocation address
RELOC2=	fffffc0000230000

# Compiler and assembler flags used to generate boot blocks.
#
DEFS=	-DSTANDALONE
AFLAGS+=-DASSEMBLER ${INCPATH}
CFLAGS= -mno-fp-regs ${INCPATH} ${DEFS}

LIBS=	libsa/libsa.a -lc
SRCS=	boot.c disk.c conf.c prom.c test.c

all: boot

libsa/libsa.a::
	cd libsa; make

# This boot is supposed to be installed in this way:
#	cp boot <true_root_of_disk>/boot
BOBJS=	boot.o conf.o disk.o prom.o prom_disp.o prom_swpal.o vers.o
boot: start.o ${BOBJS} ${LIBS}
	${LD} -Ttext ${RELOC1} -N \
	    -e start -o ${.TARGET} start.o ${BOBJS} ${LIBS}

# This boot is supposed to be installed in this way:
#	cp test_boot <true_root_of_disk>/test_boot
#
# It is used presumably in this way:
#	boot -fi "test_boot" dka300
TOBJS=	prom.o prom_disp.o prom_swpal.o test.o
test_boot: start.o ${TOBJS} ${LIBS}
	${LD} -T ${RELOC2} -N -e start -o ${.TARGET} start.o ${TOBJS} ${LIBS}

clean cleandir:
	rm -f .depend *.o *.exe *.i errs make.out core* vers.c
	rm -f a.out boot test_boot
	cd libsa && make cleandir

install:
	install -c -o root -g wheel -m 555 boot ${DESTDIR}/

depend: ${SRCS}
	mkdep -p ${INCPATH} ${DEFS} ${SRCS}
	cd libsa && make depend

newvers:
	sh newvers.sh

vers.c: newvers
