#	$OpenBSD: Makefile,v 1.1 1999/08/02 19:50:07 jakob Exp $

PROG=	mailwrapper
MAN=	mailwrapper.8 mailer.conf.5

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

afterinstall:
	ln -fs /usr/sbin/mailwrapper ${DESTDIR}/usr/sbin/sendmail
	ln -fs /usr/sbin/mailwrapper ${DESTDIR}/usr/bin/newaliases
	ln -fs /usr/sbin/mailwrapper ${DESTDIR}/usr/bin/mailq
	ln -fs /usr/sbin/mailwrapper ${DESTDIR}/usr/bin/hoststat
	ln -fs /usr/sbin/mailwrapper ${DESTDIR}/usr/bin/purgestat

.include <bsd.prog.mk>
