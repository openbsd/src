#	from: @(#)Makefile	8.2 (Berkeley) 12/15/93
#	$OpenBSD: Makefile,v 1.2 1996/03/19 23:15:46 niklas Exp $
#	$NetBSD: Makefile,v 1.6 1996/02/24 01:15:15 jtk Exp $

LIB=	telnet
SRCS=	auth.c encrypt.c genget.c getent.c misc.c
CFLAGS+= -DHAS_CGETENT

# These are the sources that have encryption stuff in them.
CRYPT_SRC= auth.c enc-proto.h enc_des.c encrypt.c
CRYPT_SRC+= encrypt.h kerberos.c kerberos5.c krb4encpwd.c
CRYPT_SRC+= misc.c spx.c Makefile
NOCRYPT_DIR=${.CURDIR}/Nocrypt

.include <bsd.lib.mk>

nocrypt:
	@echo "Encryption code already removed."
