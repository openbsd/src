#	$OpenBSD: Makefile,v 1.15 2007/10/18 18:08:02 deraadt Exp $
#	$NetBSD: Makefile,v 1.6 1997/04/25 22:17:33 veego Exp $

#	MOTOROLA MICROPROCESSOR & MEMORY TECHNOLOGY GROUP
#	M68000 Hi-Performance Microprocessor Division
#	M68040 Software Package 
#
#	M68040 Software Package Copyright (c) 1993, 1994 Motorola Inc.
#	All rights reserved.
#
#	THE SOFTWARE is provided on an "AS IS" basis and without warranty.
#	To the maximum extent permitted by applicable law,
#	MOTOROLA DISCLAIMS ALL WARRANTIES WHETHER EXPRESS OR IMPLIED,
#	INCLUDING IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A
#	PARTICULAR PURPOSE and any warranty against infringement with
#	regard to the SOFTWARE (INCLUDING ANY MODIFIED VERSIONS THEREOF)
#	and any accompanying written materials. 
#
#	To the maximum extent permitted by applicable law,
#	IN NO EVENT SHALL MOTOROLA BE LIABLE FOR ANY DAMAGES WHATSOEVER
#	(INCLUDING WITHOUT LIMITATION, DAMAGES FOR LOSS OF BUSINESS
#	PROFITS, BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR
#	OTHER PECUNIARY LOSS) ARISING OF THE USE OR INABILITY TO USE THE
#	SOFTWARE.  Motorola assumes no responsibility for the maintenance
#	and support of the SOFTWARE.  
#
#	You are hereby granted a copyright license to use, modify, and
#	distribute the SOFTWARE so long as this entire notice is retained
#	without alteration in any modified and/or redistributed versions,
#	and that such modified versions are clearly identified as such.
#	No licenses are granted by implication, estoppel or otherwise
#	under any patents or trademarks of Motorola, Inc.

#
#	Makefile 3.3 3/27/91
#
#	Makefile for 68040 Floating Point Software Package
#
.if ${MACHINE_ARCH} == "m68k"

TARGET = fpsp

FPSPAS=${CC} -x assembler-with-cpp -traditional-cpp -c -m68040 -I${.CURDIR}
LD		?= ld

#
# For the Library Version:
#
AR		?= ar
LIB_FILTER 	= sed 's/fpsp.defs/l_fpsp.defs/'
LIB_TARGET	= lib$(TARGET).a
#
# SYS selects the template set to use
#	templates are supplied for R3V6, CI5 and GEN(generic)
# PREFIX is a string that begins a temporary label in the assembler
#	R3V6 uses 'L%', CI5 likes '.L'
#
#SYS		= R3V6
#PREFIX		= L%%
#
#SYS		= CI5
#PREFIX		= .L
#
#SYS		= GEN
#PREFIX		= L_
#
SYS		= GCC
PREFIX		= L_

.SUFFIXES:	.o .s .sa .defs .h

.sa.s:
	sh ${.CURDIR}/asm2gas ${.IMPSRC} >${.TARGET}
.h.defs:
	sh ${.CURDIR}/asm2gas ${.IMPSRC} >${.TARGET}
.s.o:
	$(FPSPAS) -o ${.TARGET} ${.IMPSRC}

H_FILES = \
	fpsp.defs \
	l_fpsp.defs

O_FILES = \
	copyright.o \
	netbsd.o \
	bindec.o \
	binstr.o \
	decbin.o \
	do_func.o \
	gen_except.o \
	get_op.o \
	kernel_ex.o \
	res_func.o \
	round.o \
	sacos.o \
	sasin.o \
	satan.o \
	satanh.o \
	scosh.o \
	setox.o \
	sgetem.o \
	sint.o \
	slogn.o \
	slog2.o \
	smovecr.o \
	srem_mod.o \
	scale.o \
	ssin.o \
	ssinh.o \
	stan.o \
	stanh.o \
	sto_res.o \
	stwotox.o \
	tbldo.o \
	util.o \
	x_bsun.o \
	x_fline.o \
	x_operr.o \
	x_ovfl.o \
	x_snan.o \
	x_store.o \
	x_unfl.o \
	x_unimp.o \
	x_unsupp.o \
	bugfix.o

LIB_O_FILES	= \
	l_copyright.o \
	l_entry.o \
	l_do_func.o \
	l_round.o \
	l_sacos.o \
	l_sasin.o \
	l_satan.o \
	l_satanh.o \
	l_scale.o \
	l_scosh.o \
	l_setox.o \
	l_sgetem.o \
	l_sint.o \
	l_slog2.o \
	l_slogn.o \
	l_srem_mod.o \
	l_ssin.o \
	l_ssinh.o \
	l_stan.o \
	l_stanh.o \
	l_stwotox.o \
	l_support.o

S_FILES = \
	netbsd.s \
	bindec.s \
	binstr.s \
	decbin.s \
	do_func.s \
	get_op.s \
	gen_except.s \
	kernel_ex.s \
	res_func.s \
	round.s \
	sacos.s \
	sasin.s \
	satan.s \
	satanh.s \
	scosh.s \
	setox.s \
	sgetem.s \
	sint.s \
	slogn.s \
	slog2.s \
	smovecr.s \
	srem_mod.s \
	scale.s \
	ssin.s \
	ssinh.s \
	stan.s \
	stanh.s \
	sto_res.s \
	stwotox.s \
	tbldo.s \
	util.s \
	x_bsun.s \
	x_fline.s \
	x_operr.s \
	x_ovfl.s \
	x_snan.s \
	x_store.s \
	x_unfl.s \
	x_unimp.s \
	x_unsupp.s \
	bugfix.s

LIB_S_FILES	= \
	l_entry.sa l_entry.s \
	l_do_func.s \
	l_round.s \
	l_sacos.s \
	l_sasin.s \
	l_satan.s \
	l_satanh.s \
	l_scale.s \
	l_scosh.s \
	l_setox.s \
	l_sgetem.s \
	l_sint.s \
	l_slog2.s \
	l_slogn.s \
	l_srem_mod.s \
	l_ssin.s \
	l_ssinh.s \
	l_stan.s \
	l_stanh.s \
	l_stwotox.s \
	l_support.s

#
#	Build the target object.  The linkfile is created on the fly.
#	Change the SEG directives to suit your system.
#
$(TARGET).o:	$(O_FILES)
	$(LD) -r -o $(TARGET).o $(O_FILES)

#
#	Just about every file needs fpsp.h so:
#
$(O_FILES):	fpsp.defs

#
#-----------------------------------------------------------------------
#
#	For making a library version of the FPSP:
#
library:	$(LIB_TARGET)

$(LIB_TARGET):	$(LIB_O_FILES)
	rm -f $(LIB_TARGET)
	$(AR) crv $(LIB_TARGET) $(LIB_O_FILES)

$(LIB_O_FILES): l_fpsp.defs

#
# The entry points to the library version are created here
# by using two template files an awk script and a list of
# the entry routines for each function.
#
l_entry.sa: L_ENTRY.AWK L_LIST MONADIC.$(SYS) DYADIC.$(SYS) l_fpsp.h
	awk -f L_ENTRY.AWK SYS=$(SYS) PREFIX=$(PREFIX) - <L_LIST|sh>l_entry.sa

#
# Do_func.sa and round.sa need special editing to remove references that
# aren't needed in the library version.  Beware that changes in
# the source code may cause this editing to break....
#
l_do_func.s: do_func.s 
	$(LIB_FILTER) ${.ALLSRC} >${.TARGET}
	echo '/global.*do_func/,/^	rts/d' >.SCRIPT
	echo 'g/smovcr/d' >>.SCRIPT
	echo 'g/tblpre/d' >>.SCRIPT
	echo 'w' >>.SCRIPT
	echo 'q' >>.SCRIPT
	ed - ${.TARGET} <.SCRIPT
	rm .SCRIPT

l_round.s: round.s 
	$(LIB_FILTER) ${.ALLSRC} >${.TARGET}
	echo '/^not_E3:/-6,/^not_E3:/d' >.SCRIPT
	echo 'w' >>.SCRIPT
	echo 'q' >>.SCRIPT
	ed - ${.TARGET} <.SCRIPT
	rm .SCRIPT

l_copyright.s: copyright.s 
	$(LIB_FILTER) ${.ALLSRC} >${.TARGET}

l_sacos.s: sacos.s 
	$(LIB_FILTER) ${.ALLSRC} >${.TARGET}

l_sasin.s: sasin.s 
	$(LIB_FILTER) ${.ALLSRC} >${.TARGET}

l_satan.s: satan.s 
	$(LIB_FILTER) ${.ALLSRC} >${.TARGET}

l_satanh.s: satanh.s 
	$(LIB_FILTER) ${.ALLSRC} >${.TARGET}

l_scale.s: scale.s 
	$(LIB_FILTER) ${.ALLSRC} >${.TARGET}

l_scosh.s: scosh.s 
	$(LIB_FILTER) ${.ALLSRC} >${.TARGET}

l_setox.s: setox.s 
	$(LIB_FILTER) ${.ALLSRC} >${.TARGET}

l_sgetem.s: sgetem.s 
	$(LIB_FILTER) ${.ALLSRC} >${.TARGET}

l_sint.s: sint.s 
	$(LIB_FILTER) ${.ALLSRC} >${.TARGET}

l_slog2.s: slog2.s 
	$(LIB_FILTER) ${.ALLSRC} >${.TARGET}

l_slogn.s: slogn.s 
	$(LIB_FILTER) ${.ALLSRC} >${.TARGET}

l_srem_mod.s: srem_mod.s 
	$(LIB_FILTER) ${.ALLSRC} >${.TARGET}

l_ssin.s: ssin.s 
	$(LIB_FILTER) ${.ALLSRC} >${.TARGET}

l_ssinh.s: ssinh.s 
	$(LIB_FILTER) ${.ALLSRC} >${.TARGET}

l_stan.s: stan.s 
	$(LIB_FILTER) ${.ALLSRC} >${.TARGET}

l_stanh.s: stanh.s 
	$(LIB_FILTER) ${.ALLSRC} >${.TARGET}

l_stwotox.s: stwotox.s 
	$(LIB_FILTER) ${.ALLSRC} >${.TARGET}

#
#	Extract all files from SCCS directory
#
clean cleandir:
	rm -f $(H_FILES)
	rm -f $(S_FILES)
	rm -f $(O_FILES)
	rm -f $(TARGET).o
	rm -f $(LIB_S_FILES)
	rm -f $(LIB_O_FILES)
	rm -f $(LIB_TARGET)

clobber:	clean

_SUBDIRUSE:

.else
clean cleandir:

.endif

all:

depend:

install:

includes:

.include <bsd.obj.mk>
.include <bsd.own.mk>
