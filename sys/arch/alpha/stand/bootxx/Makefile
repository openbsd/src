#	$NetBSD: Makefile,v 1.3 1995/02/27 16:36:55 cgd Exp $
#
# BSD Boot blocks for the Alpha
#
# XXX should generate a primary boot block, too...

#INCPATH=-I../../.. -I.
INCPATH=-I/sys -I. -I/sys/arch/alpha/stand

# Boot relocation address
RELOC1=	20000000
# Low memory test program relocation address
#RELOC2=	20004000
# High memory test program relocation address
RELOC2=	fffffc0000230000

# Compiler and assembler flags used to generate boot blocks.
#
DEFS=	-DSMALL -DSTANDALONE
AFLAGS+=-DASSEMBLER ${INCPATH}
#CFLAGS= -O6 -mno-fp-regs ${INCPATH} ${DEFS}
CFLAGS= -mno-fp-regs ${INCPATH} ${DEFS} -finline-functions -O6

LIBS=	libsa/libsa.a -lc
SRCS=	boot.c conf.c prom.c test.c

all: bootxx

libsa/libsa.a::
	cd libsa; make

# This boot is supposed to be installed in this way:
#	cp boot <true_root_of_disk>/boot
BOBJS=	boot.o prom.o prom_disp.o
bootxx.coff: start.o ${BOBJS} ${LIBS}
	${LD} -Ttext ${RELOC1} -N \
	    -e start -o ${.TARGET} start.o ${BOBJS} ${LIBS}

bootxx: headersize bootxx.coff
	dd if=bootxx.coff of=bootxx.nohdr bs=`./headersize <bootxx.coff` skip=1
	dd if=bootxx.nohdr of=bootxx count=15

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
	rm -f a.out boot test_boot bootxx bootxx.nohdr bootxx.coff headersize
	cd libsa && make cleandir

install:
	install -c -o bin -g bin -m 444 bootxx ${DESTDIR}/usr/mdec/bootsd

depend: ${SRCS}
	mkdep -p ${INCPATH} ${DEFS} ${SRCS}
	cd libsa && make depend

newvers:
	sh newvers.sh

vers.c: newvers
