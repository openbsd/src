#	$OpenBSD: Makefile,v 1.2 2000/04/04 02:41:19 millert Exp $

PROG=	mailwrapper
MAN=	mailwrapper.8 mailer.conf.5

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

afterinstall:
	ln -fs /usr/sbin/mailwrapper ${DESTDIR}/usr/sbin/sendmail
	ln -fs /usr/sbin/mailwrapper ${DESTDIR}/usr/sbin/smtpd
	ln -fs /usr/sbin/mailwrapper ${DESTDIR}/usr/bin/newaliases
	ln -fs /usr/sbin/mailwrapper ${DESTDIR}/usr/bin/mailq
	ln -fs /usr/sbin/mailwrapper ${DESTDIR}/usr/bin/hoststat
	ln -fs /usr/sbin/mailwrapper ${DESTDIR}/usr/bin/purgestat

.include <bsd.prog.mk>
