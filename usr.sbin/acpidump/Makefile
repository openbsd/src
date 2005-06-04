# $OpenBSD: Makefile,v 1.3 2005/06/04 02:25:53 cloder Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64")
PROG=	acpidump
SRCS=	acpi.c acpi_user.c asl_dump.c aml_dump.c acpidump.c
SRCS+=	aml_name.c aml_parse.c aml_amlmem.c aml_memman.c aml_obj.c
SRCS+=	aml_common.c aml_evalobj.c aml_store.c
.else
NOPROG=		yes
.endif

MAN=	acpidump.8

VPATH=${.CURDIR}/aml
CFLAGS+=-I${.CURDIR}

BINDIR?=/usr/sbin

.include <bsd.prog.mk>
