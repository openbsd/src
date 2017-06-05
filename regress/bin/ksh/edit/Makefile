REGRESS_TARGETS=	emacs vi

PROG=		edit
SRCS=		edit.c
CFLAGS+=	-Wall
LDADD+=		-lutil

emacs: edit
	@sh ${.CURDIR}/emacs.sh ${.CURDIR}

vi: edit
	@sh ${.CURDIR}/vi.sh ${.CURDIR}

.include <bsd.regress.mk>
