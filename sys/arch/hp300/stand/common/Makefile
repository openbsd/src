#	$OpenBSD: Makefile,v 1.6 2006/08/17 06:31:10 miod Exp $

LIB=	common

NOMAN=	noman

.include "${.CURDIR}/../Makefile.inc"

SRCS=	apci.c autoconf.c cons.c ct.c dca.c dcm.c dnkbd.c \
	fhpib.c hd.c hil.c hpib.c if_le.c ite.c ite_dv.c ite_gb.c ite_hy.c \
	ite_rb.c ite_sti.c ite_subr.c ite_tc.c ite_tvrx.c kbd.c kbdconf.c \
	machdep.c nhpib.c scsi.c sd.c version.c

CFLAGS+=-I${.CURDIR}/../include \
	-I${.CURDIR}/../../.. -I${.CURDIR}/../../../.. -DCD9660_DUMMYLABEL

NOPROFILE=
NOPIC=

install:

.include <bsd.lib.mk>
