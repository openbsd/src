#	$OpenBSD: Makefile,v 1.13 2001/09/11 19:02:49 millert Exp $

PROG=	sendmail

WANT_LIBWRAP=1
WANT_LIBSM=1
WANT_LIBSMUTIL=1

# For TLS/SSL support
ENVDEF+= -DSTARTTLS
LDADD+= -lssl -lcrypto
DPADD=	${LIBSSL} ${LIBCRYPTO}

# To build with mail filter support (untested)
#WANT_LIBMILTER=1
#ENVDEF+= -DMILTER
#CPPFLAGS+= -pthread
#LDFLAGS+= -pthread

# Since we have random PIDs we need to be careful to avoid filename collisions
ENVDEF+= -DFAST_PID_RECYCLE

SRCS=	main.c alias.c arpadate.c bf.c collect.c conf.c control.c convtime.c \
	daemon.c deliver.c domain.c envelope.c err.c headers.c macro.c map.c \
	mci.c milter.c mime.c parseaddr.c queue.c readcf.c recipient.c \
	savemail.c sasl.c sfsasl.c shmticklib.c sm_resolve.c srvrsmtp.c stab.c \
	stats.c sysexits.c timers.c tls.c trace.c udb.c usersmtp.c util.c \
	version.c
MAN=	aliases.5 mailq.1 newaliases.8 sendmail.8
MLINKS=	sendmail.8 hoststat.1 sendmail.8 purgestat.1
BINDIR=	/usr/libexec/sendmail
BINOWN=	root
BINGRP=	smmsp
BINMODE=2555

beforeinstall:
	# Force user to make the world sane for us
	@if [ -f /etc/sendmail.cf ]; then \
		if [ ! -f /etc/mail/sendmail.cf ]; then \
			echo "Error: /etc/sendmail.cf exists but /etc/mail/sendmail.cf does not.  Please move /etc/sendmail.cf to /etc/mail/sendmail.cf."; \
			false; \
		else \
			echo "Warning: both /etc/mail/sendmail.cf and /etc/sendmail.cf exist.  Sendmail will use /etc/mail/sendmail.cf."; \
		fi ; \
	fi
	@if test -f /etc/rc && grep -q /etc/sendmail.cf /etc/rc; \
	    then echo "Error: /etc/rc must check for /etc/mail/sendmail.cf, not /etc/sendmail.cf"; \
	    false; \
	fi
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g wheel -m 444 \
	    ${.CURDIR}/helpfile ${DESTDIR}/etc/mail/helpfile
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g wheel -m 644 \
	    /dev/null ${DESTDIR}/var/log/sendmail.st

.include "../../Makefile.inc"
.include <bsd.prog.mk>
