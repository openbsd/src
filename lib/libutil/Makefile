#	$OpenBSD: Makefile,v 1.24 2001/09/29 10:56:29 deraadt Exp $
#	$NetBSD: Makefile,v 1.8 1996/05/16 07:03:28 thorpej Exp $

LIB=	util
CFLAGS+=-DLIBC_SCCS

HDRS=	util.h scsi.h
SRCS=	check_expire.c getmaxpartitions.c getrawpartition.c login.c \
	login_tty.c logout.c logwtmp.c opendev.c passwd.c pty.c readlabel.c \
	scsi.c login_fbtab.c uucplock.c fparseln.c opendisk.c

MAN=	check_expire.3 getmaxpartitions.3 getrawpartition.3 login.3 opendev.3 \
	openpty.3 pw_init.3 pw_lock.3 readlabelfs.3 scsi.3 pw_getconf.3 \
	uucplock.3 fparseln.3 opendisk.3 login_fbtab.3

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
MLINKS+=scsi.3 scsireq_buff_decode.3
MLINKS+=scsi.3 scsireq_build.3
MLINKS+=scsi.3 scsireq_decode.3
MLINKS+=scsi.3 scsireq_encode.3
MLINKS+=scsi.3 scsireq_enter.3
MLINKS+=scsi.3 scsireq_new.3
MLINKS+=scsi.3 scsireq_reset.3
MLINKS+=scsi.3 SCSIREQ_ERROR.3
MLINKS+=scsi.3 scsi_open.3
MLINKS+=scsi.3 scsi_debug.3
MLINKS+=scsi.3 scsi_debug_output.3
MLINKS+=uucplock.3 uu_lock.3
MLINKS+=uucplock.3 uu_unlock.3
MLINKS+=uucplock.3 uu_lockerr.3
MLINKS+=uucplock.3 uu_lock_txfr.3

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		${DESTDIR}/usr/include"; \
	    echo $$j; \
	    eval "$$j"; \
	done

.include <bsd.lib.mk>
