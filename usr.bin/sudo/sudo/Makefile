#	$OpenBSD: Makefile,v 1.4 2007/08/01 22:37:28 millert Exp $

.PATH:		${.CURDIR}/.. ${.CURDIR}/../auth

PROG=	sudo
BINOWN=	root
BINMODE=4555
LINKS=  ${BINDIR}/sudo ${BINDIR}/sudoedit
CFLAGS+=-DSUDO_DEVEL

SRCS=	check.c env.c getspwuid.c interfaces.c logging.c memrchr.c parse.c \
	set_perms.c sudo.c tgetpass.c zero_bytes.c sudo_auth.c bsdauth.c \
	sudo_edit.c

BINDIR=	/usr/bin
POD2MAN=/usr/bin/pod2man

MAN=	sudo.8 sudoers.5
MLINKS= sudo.8 sudoedit.8
VERSION!=uname -rs
MANDATE!=date '+%B %e, %Y'

CLEANFILES+= ${MAN}

sudo.8: sudo.pod
	sed -f ${.CURDIR}/../varsub ${.ALLSRC} | ${POD2MAN} --quotes=none \
	    --name="SUDO" --section=8 --release="${VERSION}" \
	    --date="${MANDATE}" --center="OpenBSD Reference Manual" | \
	    sed '1s/^/.if n .ll 78n /' > $@

sudoers.5: sudoers.pod
	sed -f ${.CURDIR}/../varsub ${.ALLSRC} | ${POD2MAN} --quotes=none \
	    --name="SUDOERS" --section=5 --release="${VERSION}" \
	    --date="${MANDATE}" --center="OpenBSD Reference Manual" | \
	    sed '1s/^/.if n .ll 78n /' > $@

afterdepend: ${MAN}

.include <bsd.prog.mk>
