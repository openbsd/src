REGRESS_TARGETS=	emacs vi

PROG=		edit
SRCS=		edit.c
CFLAGS+=	-Wall
LDADD+=		-lutil

# Switch to build user if run as root.  Prompt output $ or # affects test.
SU!!=	if [ "$$USER" = root ]; then echo su build; else echo sh; fi

emacs: edit
	${SU} ${.CURDIR}/emacs.sh ${.CURDIR}

vi: edit
	${SU} ${.CURDIR}/vi.sh ${.CURDIR}

.include <bsd.regress.mk>
