#$OpenBSD: Makefile,v 1.3 2013/06/21 10:59:40 ajacoutot Exp $

LIB=	kdc
LDFLAGS=	-Wl,--no-undefined

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
