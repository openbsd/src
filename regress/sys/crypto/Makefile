#	$OpenBSD: Makefile,v 1.11 2015/11/04 12:24:31 mikeb Exp $

SUBDIR+= cmac
SUBDIR+= gmac
SUBDIR+= hmac
SUBDIR+= key_wrap

.if defined(REGRESS_FULL) || make(clean) || make(cleandir) || make(obj)
SUBDIR+= enc
SUBDIR+= aesctr
SUBDIR+= aesxts
SUBDIR+= aes
SUBDIR+= chachapoly
.endif

install:

.include <bsd.subdir.mk>
