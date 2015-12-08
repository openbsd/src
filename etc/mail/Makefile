# $OpenBSD: Makefile,v 1.12 2015/12/08 09:03:50 sunil Exp $
#
# Generate the various .db versions from their source files.

DB_FILES= aliases.db
MAKEMAP= /usr/sbin/makemap

all: ${DB_FILES}

clean:
	rm -f ${DB_FILES}

distribution:
	${INSTALL} -c -o root -g wheel -m 644 aliases \
	    ${DESTDIR}/etc/mail/aliases
	${MAKEMAP} -t aliases -o ${DESTDIR}/etc/mail/aliases.db \
	    ${DESTDIR}/etc/mail/aliases
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
