# $OpenBSD: Makefile,v 1.4 2017/07/10 18:56:23 bluhm Exp $

PROGS=		prog1 prog2 prog3 prog4
SRCS_prog1=	main.c
SRCS_prog2=	main.c
SRCS_prog3=	main.c
SRCS_prog4=	main.c
LDFLAGS+=	-Wl,-E
LDFLAGS+=	-Wl,--disable-new-dtags
LDFLAGS+=	-Wl,-rpath,$(IF1_OBJDIR) -L$(IF1_OBJDIR)
LDFLAGS+=	-Wl,-rpath,$(IF2_OBJDIR) -L$(IF2_OBJDIR)
LDFLAGS+=	-Wl,-rpath,$(IF3_OBJDIR) -L$(IF3_OBJDIR)
LDFLAGS+=	-Wl,-rpath,$(NORMAL_OBJDIR) -L$(NORMAL_OBJDIR)
LDADD_prog1+=	-lnormal -lif1 -lif2
LDADD_prog2+=	-lif1 -lnormal -lif2
LDADD_prog3+=	-lif1 -lif2 -lnormal
LDADD_prog4+=	-lif1 -lif2 -lif3

REGRESS_TARGETS=do-prog1 do-prog2 do-prog3 do-prog4

do-prog1: prog1
	./prog1 | cmp -s ${.CURDIR}/expected1 -
do-prog2: prog2
	./prog2 | cmp -s ${.CURDIR}/expected2 -
do-prog3: prog3
	./prog3 | cmp -s ${.CURDIR}/expected3 -
do-prog4: prog4
	./prog4 | cmp -s ${.CURDIR}/expected4 -

.include <bsd.regress.mk>
