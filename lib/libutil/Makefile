#	$OpenBSD: Makefile,v 1.40 2017/12/14 09:27:44 kettenis Exp $
#	$NetBSD: Makefile,v 1.8 1996/05/16 07:03:28 thorpej Exp $

LIB=	util

VERSION_SCRIPT=	${.CURDIR}/Symbols.map

HDRS=	util.h imsg.h
SRCS=	bcrypt_pbkdf.c check_expire.c duid.c getmaxpartitions.c \
	getrawpartition.c login.c \
	login_tty.c logout.c logwtmp.c opendev.c passwd.c pty.c readlabel.c \
	login_fbtab.c uucplock.c fparseln.c opendisk.c pidfile.c \
	fmt_scaled.c imsg.c imsg-buffer.c pkcs5_pbkdf2.c

MAN=	bcrypt_pbkdf.3 check_expire.3 getmaxpartitions.3 getrawpartition.3 \
	isduid.3 login.3 \
	opendev.3 openpty.3 pw_init.3 pw_lock.3 readlabelfs.3 uucplock.3 \
	fparseln.3 opendisk.3 login_fbtab.3 pidfile.3 fmt_scaled.3 imsg_init.3 \
	pkcs5_pbkdf2.3

SRCS+=	ohash.c
HDRS += ohash.h

MAN +=	ohash_init.3 ohash_interval.3

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} \
		-m 444 $$i ${DESTDIR}/usr/include"; \
	    echo $$j; \
	    eval "$$j"; \
	done

.include <bsd.lib.mk>
