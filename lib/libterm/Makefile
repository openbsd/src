#	$OpenBSD: Makefile,v 1.3 1997/12/16 04:11:57 millert Exp $
#	$NetBSD: Makefile,v 1.7 1995/02/27 10:18:34 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/4/93

LIB=	termcap
CFLAGS+=-DCM_N -DCM_GT -DCM_B -DCM_D
SRCS=	termcap.c tgoto.c tputs.c

MAN=	termcap.3
MLINKS=	termcap.3 tgetent.3 termcap.3 tgetflag.3 termcap.3 tgetnum.3 \
	termcap.3 tgetstr.3 termcap.3 tgoto.3 termcap.3 tputs.3

.include <bsd.lib.mk>
