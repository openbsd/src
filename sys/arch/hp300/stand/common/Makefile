#	$OpenBSD: Makefile,v 1.2 1997/07/14 08:23:51 downsj Exp $

LIB=	common

NOMAN=	noman

SRCS=	apci.c autoconf.c cons.c ct.c dca.c dcm.c dnkbd.c \
	fhpib.c hd.c hil.c hpib.c if_le.c ite.c ite_dv.c ite_gb.c ite_rb.c \
	ite_subr.c ite_tc.c ite_hy.c kbd.c kbdconf.c machdep.c \
	nhpib.c prf.c scsi.c sd.c version.c

CFLAGS= -O3 -msoft-float -I${.CURDIR}/../include \
	-I${.CURDIR}/../../.. -I${.CURDIR}/../../../..

${OBJS}:	${.CURDIR}/../Makefile.inc

NOPROFILE=
NOPIC=

install:

.include <bsd.lib.mk>
