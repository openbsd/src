#	$OpenBSD: Makefile,v 1.2 2000/04/02 19:48:34 millert Exp $

PROG=	sendmail

WANT_LIBWRAP=1
WANT_LIBSMUTIL=1

SRCS=	main.c alias.c arpadate.c bf_torek.c clock.c collect.c \
	conf.c control.c convtime.c daemon.c deliver.c domain.c \
	envelope.c err.c headers.c macro.c map.c mci.c milter.c \
	mime.c parseaddr.c queue.c readcf.c recipient.c savemail.c \
	sfsasl.c shmticklib.c srvrsmtp.c stab.c stats.c sysexits.c \
	timers.c trace.c udb.c usersmtp.c util.c version.c
MAN=	aliases.5 mailq.1 newaliases.1 sendmail.8
BINDIR=	/usr/libexec/sendmail
BINOWN=	root
BINGRP=	bin
BINMODE=4555

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
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/helpfile ${DESTDIR}/etc/mail/helpfile
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 644 \
	    /dev/null ${DESTDIR}/var/log/sendmail.st

.include "../../Makefile.inc"
.include <bsd.prog.mk>
