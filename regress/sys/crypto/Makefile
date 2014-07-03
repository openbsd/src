#	$OpenBSD: Makefile,v 1.10 2014/07/03 21:12:24 miod Exp $

SUBDIR+= cmac
SUBDIR+= gmac
SUBDIR+= hmac
SUBDIR+= key_wrap

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= enc
SUBDIR+= aesctr
SUBDIR+= aesxts
SUBDIR+= aes
.endif

install:

.include <bsd.subdir.mk>
