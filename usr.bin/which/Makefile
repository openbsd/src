#	$OpenBSD: Makefile,v 1.6 2013/04/10 02:57:20 guenther Exp $

whereis_MAN = ${.OBJDIR}/whereis.1
man_depends = ${.OBJDIR}/whereis.d

PROG=	which
MAN=	which.1 ${whereis_MAN}
LINKS=	${BINDIR}/which ${BINDIR}/whereis

${whereis_MAN}: ${.CURDIR}/whereis.1 /usr/include/paths.h
	stdpath=`printf '#include <paths.h>\n_PATH_STDPATH\n' | \
		${CPP} ${CPPFLAGS} -MD -MT '$${.OBJDIR}/whereis.1' \
		-MF ${man_depends} - | \
		sed -n 's/^[ 	]*"\(.*\)".*/\1/p'` ; \
	sed "s!_PATH_STDPATH!$${stdpath}!" < ${.CURDIR}/whereis.1 >$@

CLEANFILES = ${whereis_MAN}
cleandir: cleanlocaldepend
cleanlocaldepend:
	rm -f ${man_depends}

.if exists(${man_depends})
. include "${man_depends}"
.endif

.include <bsd.prog.mk>
