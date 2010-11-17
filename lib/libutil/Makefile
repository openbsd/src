#	$OpenBSD: Makefile,v 1.33 2010/11/17 10:10:31 jsing Exp $
#	$NetBSD: Makefile,v 1.8 1996/05/16 07:03:28 thorpej Exp $

LIB=	util
WANTLINT=

HDRS=	util.h imsg.h
SRCS=	check_expire.c duid.c getmaxpartitions.c getrawpartition.c login.c \
	login_tty.c logout.c logwtmp.c opendev.c passwd.c pty.c readlabel.c \
	login_fbtab.c uucplock.c fparseln.c opendisk.c pidfile.c \
	fmt_scaled.c imsg.c imsg-buffer.c

MAN=	check_expire.3 getmaxpartitions.3 getrawpartition.3 isduid.3 login.3 \
	opendev.3 openpty.3 pw_init.3 pw_lock.3 readlabelfs.3 uucplock.3 \
	fparseln.3 opendisk.3 login_fbtab.3 pidfile.3 fmt_scaled.3 imsg_init.3

MLINKS+=imsg_init.3 imsg_read.3
MLINKS+=imsg_init.3 imsg_get.3
MLINKS+=imsg_init.3 imsg_compose.3
MLINKS+=imsg_init.3 imsg_composev.3
MLINKS+=imsg_init.3 imsg_create.3
MLINKS+=imsg_init.3 imsg_add.3
MLINKS+=imsg_init.3 imsg_close.3
MLINKS+=imsg_init.3 imsg_free.3
MLINKS+=imsg_init.3 imsg_flush.3
MLINKS+=imsg_init.3 imsg_clear.3
MLINKS+=imsg_init.3 ibuf_open.3
MLINKS+=imsg_init.3 ibuf_dynamic.3
MLINKS+=imsg_init.3 ibuf_add.3
MLINKS+=imsg_init.3 ibuf_reserve.3
MLINKS+=imsg_init.3 ibuf_seek.3
MLINKS+=imsg_init.3 ibuf_size.3
MLINKS+=imsg_init.3 ibuf_left.3
MLINKS+=imsg_init.3 ibuf_close.3
MLINKS+=imsg_init.3 ibuf_write.3
MLINKS+=imsg_init.3 ibuf_free.3
MLINKS+=imsg_init.3 msgbuf_init.3
MLINKS+=imsg_init.3 msgbuf_clear.3
MLINKS+=imsg_init.3 msgbuf_write.3
MLINKS+=imsg_init.3 msgbuf_drain.3
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
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} \
		-m 444 $$i ${DESTDIR}/usr/include"; \
	    echo $$j; \
	    eval "$$j"; \
	done

.include <bsd.lib.mk>
