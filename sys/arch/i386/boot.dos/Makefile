#     $NetBSD: Makefile,v 1.18 1995/03/23 19:46:45 jtc Exp $
#
# Ported to boot 386BSD by Julian Elischer (julian@tfs.com)
# September 1992
#
# Permission to use, copy, modify and distribute this software and its
# documentation is hereby granted, provided that both the copyright
# notice and this permission notice appear in all copies of the
# software, derivative works or modified versions, and any portions
# thereof, and that both notices appear in supporting documentation.
#
# CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
# CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
# ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
#
# Carnegie Mellon requests users of this software to return to
#
#  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
#  School of Computer Science
#  Carnegie Mellon University
#  Pittsburgh PA 15213-3890
#
# any improvements or extensions that they make and grant Carnegie Mellon
# the rights to redistribute these changes.
#

S=    ${.CURDIR}/../../..

machine-links:
	-rm -f machine && ln -s ${.CURDIR}/../include machine
	-rm -f ${MACHINE_ARCH} && ln -s ${.CURDIR}/../include ${MACHINE_ARCH}

all: machine-links boot.com

NOPROG=       noprog
NOMAN=        noman

CFLAGS=	-O6 -DKERNEL -D_KERNEL -DI386_CPU -DI486_CPU -DI586_CPU
CFLAGS+=-DDO_BAD144 -I. -I${.CURDIR} -I$S -I${.CURDIR}/../.. -I${.CURDIR}/../boot

# Uncomment this to make the boot block talk to a serial port.
#CPPFLAGS+=-DSERIAL
# Uncomment this to make boot blocks read DOS files
CPPFLAGS+= -DDOSREAD -DDEBUG

# start.o should be first
OBJS= start.o table.o boot.o asm.o bios.o dossys.o io.o disk.o sys.o version.o
VPATH=        ${.CURDIR}/../boot

boot.com: ${OBJS}
	${LD} -Bstatic -e start -N -T 0x100 -o dosboot ${OBJS}
	cp dosboot dosboot.sym
	@strip dosboot
	@sh ${.CURDIR}/../boot/rmaouthdr dosboot dosboot.tmp
	@mv -f dosboot.tmp boot.com
	@ls -l boot.com

install: boot.com
	cp boot.com ${DESTDIR}/usr/mdec/boot.com


CLEANFILES+=boot.com dosboot dosboot.sym machine ${MACHINE_ARCH}

.include <bsd.prog.mk>
