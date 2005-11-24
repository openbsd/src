#	$OpenBSD: Makefile,v 1.29 2005/11/24 20:49:23 deraadt Exp $
#	$NetBSD: Makefile,v 1.8 1996/05/16 07:03:28 thorpej Exp $

LIB=	util
WANTLINT=
CFLAGS+=-DLIBC_SCCS

HDRS=	util.h
SRCS=	check_expire.c getmaxpartitions.c getrawpartition.c login.c \
	login_tty.c logout.c logwtmp.c opendev.c passwd.c pty.c readlabel.c \
	login_fbtab.c uucplock.c fparseln.c opendisk.c pidfile.c \
	fmt_scaled.c

MAN=	check_expire.3 getmaxpartitions.3 getrawpartition.3 login.3 opendev.3 \
	openpty.3 pw_init.3 pw_lock.3 readlabelfs.3 uucplock.3 \
	fparseln.3 opendisk.3 login_fbtab.3 pidfile.3 fmt_scaled.3

MLINKS+=login.3 logout.3
MLINKS+=login.3 logwtmp.3
MLINKS+=check_expire.3 login_check_expire.3
MLINKS+=openpty.3 login_tty.3
MLINKS+=openpty.3 forkpty.3
MLINKS+=pw_init.3 pw_setdir.3
MLINKS+=pw_init.3 pw_file.3
MLINKS+=pw_init.3 pw_edit.3
MLINKS+=pw_init.3 pw_prompt.3
MLINKS+=pw_init.3 pw_copy.3
MLINKS+=pw_init.3 pw_scan.3
MLINKS+=pw_init.3 pw_error.3
MLINKS+=pw_lock.3 pw_mkdb.3
MLINKS+=pw_lock.3 pw_abort.3
MLINKS+=uucplock.3 uu_lock.3
MLINKS+=uucplock.3 uu_unlock.3
MLINKS+=uucplock.3 uu_lockerr.3
MLINKS+=uucplock.3 uu_lock_txfr.3
MLINKS+=fmt_scaled.3 scan_scaled.3

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		${DESTDIR}/usr/include"; \
	    echo $$j; \
	    eval "$$j"; \
	done

.include <bsd.lib.mk>
