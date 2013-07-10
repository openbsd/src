# $OpenBSD: Makefile,v 1.7 2013/07/10 11:14:40 ajacoutot Exp $

PROG=	kcm
MPAGES=	kcm.8
MSRC=	${KRB5DIR}/kcm

SRCS=	acl.c \
	acquire.c \
	cache.c \
	client.c \
	config.c \
	connect.c \
	events.c \
	glue.c \
	log.c \
	main.c \
	protocol.c \
	sessions.c \
	renew.c

CPPFLAGS=	-I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/kcm -I${KRB5DIR}/lib/ipc
DPADD=	${LIBHDB} ${LIBKRB5} ${LIBROKEN} ${LIBASN1} \
	${LIBHEIMIPCS} ${LIBCRYPTO} ${LIBVERS}
LDADD=	-lhdb -lkrb5 -lroken -lasn1 ${LIBHEIMIPCS} \
	-lcrypto ${LIBVERS} -lwind -lcom_err -lwind \
	-lheimbase -lutil

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kcm
