# $OpenBSD: Makefile,v 1.4 2004/08/14 17:06:03 millert Exp $
#
# Generate the various .db versions from their source files.
# The sendmail .mc files are not built here.  They live in
# /usr/share/sendmail/cf and have their own Makefile.
#

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
	makemap hash ${DESTDIR}/etc/mail/access < access
	${INSTALL} -c -o root -g wheel -m 644 aliases \
	    ${DESTDIR}/etc/mail/aliases
	/usr/libexec/sendmail/sendmail -C${DESTDIR}/etc/mail/sendmail.cf \
	    -bi -O AliasFile=${DESTDIR}/etc/mail/aliases \
	    -O DontBlameSendmail=mapinunsafedirpath
	${INSTALL} -c -o root -g wheel -m 644 local-host-names \
	    ${DESTDIR}/etc/mail/local-host-names
	${INSTALL} -c -o root -g wheel -m 644 genericstable \
	    ${DESTDIR}/etc/mail/genericstable
	makemap hash ${DESTDIR}/etc/mail/genericstable < genericstable
	${INSTALL} -c -o root -g wheel -m 644 mailertable \
	    ${DESTDIR}/etc/mail/mailertable
	makemap hash ${DESTDIR}/etc/mail/mailertable < mailertable
	${INSTALL} -c -o root -g wheel -m 644 relay-domains \
	    ${DESTDIR}/etc/mail/relay-domains
	${INSTALL} -c -o root -g wheel -m 644 trusted-users \
	    ${DESTDIR}/etc/mail/trusted-users
	${INSTALL} -c -o root -g wheel -m 644 virtusertable \
	    ${DESTDIR}/etc/mail/virtusertable
	makemap hash ${DESTDIR}/etc/mail/virtusertable < virtusertable
	@for db in access.db aliases.db genericstable.db mailertable.db \
	    virtusertable.db; do \
		chmod 644 ${DESTDIR}/etc/mail/$$db; \
	done

access.db: access
	makemap hash ${.ALLSRC} < ${.ALLSRC}

aliases.db: aliases
	sendmail -bi

genericstable.db: genericstable
	makemap hash ${.ALLSRC} < ${.ALLSRC}

mailertable.db: mailertable
	makemap hash ${.ALLSRC} < ${.ALLSRC}

virtusertable.db: virtusertable
	makemap hash ${.ALLSRC} < ${.ALLSRC}

userdb.db: userdb
	makemap btree ${.ALLSRC} < ${.ALLSRC}

.PHONY: distribution
.include <bsd.own.mk>
.include <bsd.sys.mk>
