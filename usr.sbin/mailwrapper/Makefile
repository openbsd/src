#	$OpenBSD: Makefile,v 1.4 2009/03/16 14:26:25 jacekm Exp $

PROG=	mailwrapper
MAN=	mailwrapper.8 mailer.conf.5

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

afterinstall:
	ln -fs /usr/sbin/mailwrapper ${DESTDIR}/usr/sbin/sendmail
	ln -fs /usr/sbin/mailwrapper ${DESTDIR}/usr/bin/newaliases
	ln -fs /usr/sbin/mailwrapper ${DESTDIR}/usr/bin/mailq
	ln -fs /usr/sbin/mailwrapper ${DESTDIR}/usr/bin/makemap
	ln -fs /usr/sbin/mailwrapper ${DESTDIR}/usr/bin/hoststat
	ln -fs /usr/sbin/mailwrapper ${DESTDIR}/usr/bin/purgestat

.include <bsd.prog.mk>
