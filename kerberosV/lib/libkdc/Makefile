#$OpenBSD: Makefile,v 1.2 2013/06/19 19:36:44 robert Exp $

LIB=	kdc

INCS=	kdc.h \
	kdc-protos.h

KRB5INCS=	windc_plugin.h
KRB5INCSDIR=	${INCLUDEDIR}/krb5
INCSGROUPS=	INCS KRB5INCS

SRCS=	\
	default_config.c \
	set_dbinfo.c \
	kerberos5.c \
	krb5tgs.c \
	log.c \
	misc.c \
	process.c \
	windc.c

CFLAGS+=	-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/hdb \
		-I${KRB5DIR}/kdc

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/kdc
