#	$OpenBSD: Makefile,v 1.10 1996/12/08 15:22:34 downsj Exp $
#	$NetBSD: Makefile,v 1.8 1996/05/16 07:03:28 thorpej Exp $

LIB=	util
CFLAGS+=-DLIBC_SCCS

HDRS=	util.h scsi.h
SRCS=	getmaxpartitions.c getrawpartition.c login.c login_tty.c logout.c \
	logwtmp.c opendev.c passwd.c pty.c readlabel.c scsi.c login_fbtab.c

# XXX need login_fbtab.3
MAN=	getmaxpartitions.3 getrawpartition.3 login.3 opendev.3 openpty.3 \
	pw_init.3 pw_lock.3 scsi.3

MLINKS+=login.3 logout.3
MLINKS+=login.3 logwtmp.3
MLINKS+=openpty.3 login_tty.3
MLINKS+=openpty.3 forkpty.3
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

includes:
	@cd ${.CURDIR}; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/$$i || \
		${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		${DESTDIR}/usr/include"; \
	    echo $$j; \
	    eval "$$j"; \
	done

.include <bsd.lib.mk>
