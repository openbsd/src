#	$OpenBSD: Makefile,v 1.28 2008/08/20 22:19:49 dlg Exp $

PROG=	sendmail

.include <bsd.own.mk>

WANT_LIBWRAP=1
WANT_LIBSM=1
WANT_LIBSMUTIL=1

# For TLS/SSL support
ENVDEF+= -DSTARTTLS
LDADD+= -lssl -lcrypto
DPADD=	${LIBSSL} ${LIBCRYPTO}

# To build with SASL support define WANT_SMTPAUTH in /etc/mk.conf (unsupported)
.if defined(WANT_SMTPAUTH)
ENVDEF+= -DSASL
CFLAGS+= -I/usr/local/include/sasl
LDADD+= -lsasl2
LDFLAGS+=-L/usr/local/lib
.endif

# Compile with milter support.
ENVDEF+= -DMILTER

# Compile with socket map support.
ENVDEF+= -DSOCKETMAP

# Since we have random PIDs we need to be careful to avoid filename collisions
ENVDEF+= -DFAST_PID_RECYCLE

# We want sendmail to call setlogin() when running commands as a non-root user
ENVDEF+= -D_FFR_USE_SETLOGIN

SRCS=	main.c alias.c arpadate.c bf.c collect.c conf.c control.c convtime.c \
	daemon.c deliver.c domain.c envelope.c err.c headers.c macro.c map.c \
	mci.c milter.c mime.c parseaddr.c queue.c ratectrl.c readcf.c \
	recipient.c sasl.c savemail.c sfsasl.c shmticklib.c sm_resolve.c \
	srvrsmtp.c stab.c stats.c sysexits.c timers.c tls.c trace.c udb.c \
	usersmtp.c util.c version.c
MAN=	aliases.5 mailq.8 newaliases.8 sendmail.8
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

afterinstall:
	cmp -s ${.CURDIR}/TUNING ${DESTDIR}${SHAREDIR}/sendmail/TUNING || \
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} \
	    -m 444  ${.CURDIR}/TUNING ${DESTDIR}${SHAREDIR}/sendmail/TUNING ; \


.include "../../Makefile.inc"
.include <bsd.prog.mk>
