#	$OpenBSD: Makefile,v 1.9 2016/05/26 21:24:16 millert Exp $

PROG=	which
MAN=	which.1 whereis.1
LINKS=	${BINDIR}/which ${BINDIR}/whereis

check_path_in_man:
	@echo "Checking path expansion in whereis.1"; \
	stdpath=`printf '#include <paths.h>\n_PATH_STDPATH\n' | \
	${CC} -E -P ${CPPFLAGS} - | \
	sed -n 's/^[    ]*"\(.*\)".*/.D1 \1/p'` ; \
	fgrep -xq "$$stdpath" ${.CURDIR}/whereis.1 && { touch $@; exit 0; }; \
	echo "Update the expansion of _PATH_STDPATH in ${.CURDIR}/whereis.1"; \
	exit 1

all: check_path_in_man
CLEANFILES = check_path_in_man

.include <bsd.prog.mk>
