# $OpenBSD: Makefile,v 1.2 2000/03/03 00:54:46 todd Exp $
# $NetBSD: Makefile,v 1.2 1996/02/01 22:29:25 mycroft Exp $

# Copyright (c) 1994,1995 Mark Brinicombe.
# Copyright (c) 1994 Brini.
# All rights reserved.
#
# This code is derived from software written for Brini by Mark Brinicombe
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#	This product includes software developed by Brini.
# 4. The name of the company nor the name of the author may be used to
#    endorse or promote products derived from this software without specific
#    prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES# LOSS OF USE, DATA, OR PROFITS OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# RiscBSD kernel project
#
# Makefile
#
# Makefile for RiscBSD bootloader for RISCOS
#
# Created      : 14/01/96
# Last updated : 14/01/96
#
#

S=	../../..

machine-links:
	-rm -f machine && \
	    ln -s ${.CURDIR}/../include machine
	-rm -f ${MACHINE_ARCH} && \
	    ln -s ${.CURDIR}/../include ${MACHINE_ARCH}

all: machine-links boot

NOPROG=	noprog
NOMAN=	noman

CFLAGS=	-O3 -fno-builtin -D_LOCORE -D_KERNEL -I. -I${.CURDIR} -I$S -I${.CURDIR}/../..
AFLAGS= -D_LOCORE -D_KERNEL -I. -I${.CURDIR} -I$S -I${.CURDIR}/../..

### find out what to use for libkern and libsa
#SAREL=
#KERNREL=
#.include "Makefile.inc"
.include "$S/lib/libsa/Makefile.inc"
.include "$S/lib/libkern/Makefile.inc"

all: boot ${SALIB} ${KERNLIB}

#.PATH:	$S/lib/libkern
#.PATH:	$S/lib/libkern/arch/arm32

# start.o should be first
OBJS=	start.o strcpy.o strlen.o strncmp.o strstr.o \
	svc32.o div.o exit.o enteros.o exitos.o setcpsr.o \
	memcpy.o memset.o ntohl.o _bsdboot.o _main.o \
	areahand.o vsprintf.o bsdbooter.o swiv.o

boot:	${OBJS}
	${LD} -Bstatic -e start -N -Ttext 0x8000 -o boot ${OBJS}
	cp boot boot.sym
	@strip boot
	@sh ${.CURDIR}/rmaouthdr boot boot.tmp
	@mv -f boot.tmp boot
	@ls -l boot

CLEANFILES+=boot boot.sym machine ${MACHINE_ARCH}

clean::
	rm -f a.out [Ee]rrs mklog core *.core
	rm -f ${PROG} ${OBJS} ${LOBJS} ${CLEANFILES}

.include <bsd.prog.mk>
