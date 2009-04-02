# $OpenBSD: Makefile,v 1.7 2009/04/02 10:21:08 jacekm Exp $
#
# Generate the various .db versions from their source files.
# The sendmail .mc files are not built here.  They live in
# /usr/share/sendmail/cf and have their own Makefile.
#

SENDMAIL=	/usr/libexec/sendmail/sendmail
MAKEMAP=	/usr/libexec/sendmail/makemap

.for DB in access aliases genericstable mailertable virtusertable userdb
. if exists(${DB})
DB_FILES+= ${DB}.db
. endif
.endfor

all: ${DB_FILES}

clean:
	rm -f ${DB_FILES}

distribution:
	${INSTALL} -c -o root -g wheel -m 644 Makefile \
	    ${DESTDIR}/etc/mail/Makefile
	${INSTALL} -c -o root -g wheel -m 644 README \
	    ${DESTDIR}/etc/mail/README
	${INSTALL} -c -o root -g wheel -m 644 access \
	    ${DESTDIR}/etc/mail/access
	${MAKEMAP} hash ${DESTDIR}/etc/mail/access < access
	${INSTALL} -c -o root -g wheel -m 644 aliases \
	    ${DESTDIR}/etc/mail/aliases
	${SENDMAIL} -C${DESTDIR}/etc/mail/sendmail.cf \
	    -bi -O AliasFile=${DESTDIR}/etc/mail/aliases \
	    -O DontBlameSendmail=mapinunsafedirpath
	${INSTALL} -c -o root -g wheel -m 644 local-host-names \
	    ${DESTDIR}/etc/mail/local-host-names
	${INSTALL} -c -o root -g wheel -m 644 genericstable \
	    ${DESTDIR}/etc/mail/genericstable
	${MAKEMAP} hash ${DESTDIR}/etc/mail/genericstable < genericstable
	${INSTALL} -c -o root -g wheel -m 644 mailertable \
	    ${DESTDIR}/etc/mail/mailertable
	${MAKEMAP} hash ${DESTDIR}/etc/mail/mailertable < mailertable
	${INSTALL} -c -o root -g wheel -m 644 relay-domains \
	    ${DESTDIR}/etc/mail/relay-domains
	${INSTALL} -c -o root -g wheel -m 644 trusted-users \
	    ${DESTDIR}/etc/mail/trusted-users
	${INSTALL} -c -o root -g wheel -m 644 virtusertable \
	    ${DESTDIR}/etc/mail/virtusertable
	${INSTALL} -c -o root -g wheel -m 644 spamd.conf \
	    ${DESTDIR}/etc/mail/spamd.conf
	${INSTALL} -c -o root -g wheel -m 644 smtpd.conf \
	    ${DESTDIR}/etc/mail/smtpd.conf
	${MAKEMAP} hash ${DESTDIR}/etc/mail/virtusertable < virtusertable
	@for db in access.db aliases.db genericstable.db mailertable.db \
	    virtusertable.db; do \
		chmod 644 ${DESTDIR}/etc/mail/$$db; \
	done

access.db: access
	${MAKEMAP} hash ${.ALLSRC} < ${.ALLSRC}

aliases.db: aliases
	${SENDMAIL} -bi

genericstable.db: genericstable
	${MAKEMAP} hash ${.ALLSRC} < ${.ALLSRC}

mailertable.db: mailertable
	${MAKEMAP} hash ${.ALLSRC} < ${.ALLSRC}

virtusertable.db: virtusertable
	${MAKEMAP} hash ${.ALLSRC} < ${.ALLSRC}

userdb.db: userdb
	${MAKEMAP} btree ${.ALLSRC} < ${.ALLSRC}

.PHONY: distribution
.include <bsd.own.mk>
.include <bsd.sys.mk>
