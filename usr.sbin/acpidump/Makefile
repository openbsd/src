# $OpenBSD: Makefile,v 1.1 2005/06/02 20:09:39 tholo Exp $

PROG=	acpidump
SRCS=	acpi.c acpi_user.c asl_dump.c aml_dump.c acpidump.c
SRCS+=	aml_name.c aml_parse.c aml_amlmem.c aml_memman.c aml_obj.c
SRCS+=	aml_common.c aml_evalobj.c aml_store.c
MAN=	acpidump.8

VPATH=	${.CURDIR}/aml
CFLAGS=	-I${.CURDIR}

BINDIR?=/usr/sbin

.include <bsd.prog.mk>
