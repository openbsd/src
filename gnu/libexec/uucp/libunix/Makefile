# This subdirectory contains Unix specific support functions.
# $Id: Makefile,v 1.1.1.1 1995/10/18 08:38:27 deraadt Exp $

LIB=		unix
SRCS=		access.c addbas.c app3.c app4.c basnam.c bytfre.c cwd.c \
		chmod.c cohtty.c cusub.c detach.c efopen.c epopen.c exists.c \
		filnam.c fsusg.c indir.c init.c isdir.c isfork.c iswait.c \
		jobid.c lcksys.c link.c locfil.c lock.c loctim.c mail.c \
		mkdirs.c mode.c move.c opensr.c pause.c picksb.c portnm.c \
		proctm.c recep.c run.c seq.c serial.c signal.c sindir.c size.c \
		sleep.c spawn.c splcmd.c splnam.c spool.c srmdir.c statsb.c \
		status.c time.c tmpfil.c trunc.c uacces.c ufopen.c ultspl.c \
		unknwn.c uuto.c walk.c wldcrd.c work.c xqtfil.c xqtsub.c \
		ftw.c corrup.c pipe.c priv.c uid.c tli.c tcp.c sync.c failed.c
CFLAGS+=	-I$(.CURDIR)/../common_sources \
		-DOWNER=\"$(owner)\" -DSBINDIR=\"$(sbindir)\"

NOMAN=		noman
NOPROFILE=	noprofile
NOPIC=		nopic

install:

.include <bsd.lib.mk>
