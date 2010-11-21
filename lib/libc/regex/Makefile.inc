#	$OpenBSD: Makefile.inc,v 1.7 2010/11/21 00:02:30 tedu Exp $
#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93

# regex sources
.PATH: ${LIBCSRCDIR}/regex

SRCS+=	regcomp.c regerror.c regexec.c regfree.c

MAN+=	regex.3 re_format.7

MLINKS+=regex.3 regcomp.3 regex.3 regexec.3 regex.3 regerror.3 regex.3 regfree.3
