#	from: @(#)Makefile	8.2 (Berkeley) 12/15/93
#	$OpenBSD: Makefile,v 1.3 1998/03/12 04:48:42 art Exp $
#	$NetBSD: Makefile,v 1.6 1996/02/24 01:15:15 jtk Exp $

LIB=	telnet
SRCS=	auth.c encrypt.c genget.c getent.c misc.c

CFLAGS+= -DHAS_CGETENT

.include <bsd.own.mk> # for KERBEROS

.if (${KERBEROS} == "yes")
CFLAGS+= -DKRB4 -DAUTHENTICATION -DENCRYPTION -DDES_ENCRYPTION
SRCS+= kerberos.c enc_des.c
.endif

# XXX this has not been tested due to lack of kerberos5 to test with.
.if (${KERBEROS5} == "yes")
CFLAGS+= -DKRB5 -DAUTHENTICATION -DENCRYPTION -DDES_ENCRYPTION
SRCS+= kerberos5.c enc_des.c
.endif

.include <bsd.lib.mk>
