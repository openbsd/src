#	$OpenBSD: Makefile.inc,v 1.2 1996/08/19 08:31:01 tholo Exp $

# regex sources
.PATH: ${.CURDIR}/regex

CFLAGS+=-DPOSIX_MISTAKE

SRCS+=	regcomp.c regerror.c regexec.c regfree.c

MAN+=	regex.3 re_format.7

MLINKS+=regex.3 regcomp.3 regex.3 regexec.3 regex.3 regerror.3
MLINKS+=regexec.3 regfree.3
