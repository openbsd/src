# $OpenBSD: Makefile,v 1.10 2014/09/17 16:39:45 ajacoutot Exp $
#
# Generate the various .db versions from their source files.

DB_FILES= aliases.db
MAKEMAP= /usr/libexec/smtpd/makemap

all: ${DB_FILES}

clean:
	rm -f ${DB_FILES}

distribution:
	${INSTALL} -c -o root -g wheel -m 644 aliases \
	    ${DESTDIR}/etc/mail/aliases
	${MAKEMAP} -t aliases -o ${DESTDIR}/etc/mail/aliases.db \
	    aliases
	${INSTALL} -c -o root -g wheel -m 644 smtpd.conf \
	    ${DESTDIR}/etc/mail/smtpd.conf
	${INSTALL} -c -o root -g wheel -m 644 spamd.conf \
	    ${DESTDIR}/etc/mail/spamd.conf
	chmod 644 ${DESTDIR}/etc/mail/aliases.db

aliases.db: aliases
	newaliases

.PHONY: distribution
.include <bsd.own.mk>
.include <bsd.sys.mk>
