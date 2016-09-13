REGRESS_TARGETS=acme

acme: start_httpd
	/usr/sbin/acme-client \
	    -C ${.OBJDIR}/www/acme \
	    -c ${.OBJDIR}/etc/ssl/acme \
	    -f ${.OBJDIR}/etc/acme/privkey.pem \
	    -k ${.OBJDIR}/etc/ssl/acme/private/privkey.pem \
	    -nNv -s letsencrypt-staging `hostname`
	/usr/sbin/acme-client \
	    -C ${.OBJDIR}/www/acme \
	    -c ${.OBJDIR}/etc/ssl/acme \
	    -f ${.OBJDIR}/etc/acme/privkey.pem \
	    -k ${.OBJDIR}/etc/ssl/acme/private/privkey.pem \
	    -rv -s letsencrypt-staging `hostname`
	pkill -xf "/usr/sbin/httpd -f ${.OBJDIR}/etc/httpd.conf"

start_httpd:
	sh ${.CURDIR}/create-httpd-env.sh ${.OBJDIR}
	/bin/echo `/bin/hostname`
	/usr/sbin/httpd -f ${.OBJDIR}/etc/httpd.conf
	sleep .1 # give httpd some time to spin up
	pgrep -xf "/usr/sbin/httpd -f ${.OBJDIR}/etc/httpd.conf"

.PHONY: ${REGRESS_TARGETS} start_httpd

.include <bsd.regress.mk>
