SUBDIR+=src
WWWROOT=/var/www
CONFFILES= \
	conf/srm.conf-dist conf/access.conf-dist conf/httpd.conf-dist \
	conf/mime.types conf/access.conf conf/httpd.conf conf/srm.conf 
HTDOCS= \
	htdocs/apache_pb.gif htdocs/index.html htdocs/openbsdpower.gif
CGIFILES= \
	cgi-bin/printenv cgi-bin/test-cgi	
MANUALFILES= \
	manual/images/home.gif manual/images/index.gif			\
	manual/images/sub.gif manual/misc/API.html			\
	manual/misc/known_bugs.html manual/misc/FAQ.html		\
	manual/misc/known_client_problems.html				\
	manual/misc/client_block_api.html manual/misc/nopgp.html	\
	manual/misc/compat_notes.html manual/misc/perf-bsd44.html	\
	manual/misc/descriptors.html manual/misc/perf-dec.html		\
	manual/misc/fin_wait_2.html manual/misc/perf.html		\
	manual/misc/footer.html manual/misc/security_tips.html		\
	manual/misc/header.html manual/misc/vif-info.html		\
	manual/misc/howto.html manual/misc/windoz_keepalive.html	\
	manual/LICENSE manual/host.html manual/multilogs.html		\
	manual/TODO  manual/new_features_1_0.html		\
	manual/bind.html manual/index.html				\
	manual/new_features_1_1.html manual/cgi_path.html		\
	manual/install.html manual/new_features_1_2.html		\
	manual/content-negotiation.html manual/install_1_1.html		\
	manual/process-model.html manual/custom-error.html		\
	manual/invoking.html manual/stopping.html			\
	manual/dns-caveats.html manual/keepalive.html			\
	manual/suexec.html manual/env.html manual/location.html		\
	manual/unixware.html manual/footer.html				\
	manual/man-template.html manual/vhosts-in-depth.html		\
	manual/handler.html manual/virtual-host.html	\
	manual/header.html manual/mod/core.html		\
	manual/mod/mod_auth_msql.html manual/mod/mod_include.html	\
	manual/mod/directives.html manual/mod/mod_browser.html		\
	manual/mod/mod_info.html manual/mod/footer.html			\
	manual/mod/mod_cern_meta.html manual/mod/mod_log_agent.html	\
	manual/mod/header.html manual/mod/mod_cgi.html			\
	manual/mod/mod_log_common.html manual/mod/index.html		\
	manual/mod/mod_cookies.html manual/mod/mod_log_config.html	\
	manual/mod/mod_access.html manual/mod/mod_digest.html		\
	manual/mod/mod_log_referer.html manual/mod/mod_actions.html	\
	manual/mod/mod_dir.html manual/mod/mod_mime.html		\
	manual/mod/mod_alias.html manual/mod/mod_dld.html		\
	manual/mod/mod_negotiation.html manual/mod/mod_asis.html	\
	manual/mod/mod_env.html manual/mod/mod_proxy.html		\
	manual/mod/mod_auth.html manual/mod/mod_example.html		\
	manual/mod/mod_rewrite.html manual/mod/mod_auth_anon.html	\
	manual/mod/mod_expires.html manual/mod/mod_status.html		\
	manual/mod/mod_auth_db.html manual/mod/mod_headers.html		\
	manual/mod/mod_userdir.html manual/mod/mod_auth_dbm.html	\
	manual/mod/mod_imap.html manual/mod/mod_usertrack.html
ICONFILES= \
	icons/README icons/c.gif icons/hand.right.gif icons/pie2.gif	\
	icons/sphere1.gif icons/a.gif icons/comp.blue.gif		\
	icons/hand.up.gif icons/pie3.gif icons/sphere2.gif		\
	icons/alert.black.gif icons/comp.gray.gif icons/icon.sheet.gif	\
	icons/pie4.gif icons/tar.gif icons/alert.red.gif		\
	icons/compressed.gif icons/image1.gif icons/pie5.gif		\
	icons/tex.gif icons/apache_pb.gif icons/continued.gif		\
	icons/image2.gif icons/pie6.gif icons/text.gif icons/back.gif	\
	icons/dir.gif icons/image3.gif icons/pie7.gif			\
	icons/transfer.gif icons/ball.gray.gif icons/down.gif		\
	icons/index.gif icons/pie8.gif icons/unknown.gif		\
	icons/ball.red.gif icons/dvi.gif icons/layout.gif		\
	icons/portal.gif icons/up.gif icons/binary.gif icons/f.gif	\
	icons/left.gif icons/ps.gif icons/uu.gif icons/binhex.gif	\
	icons/folder.gif icons/link.gif icons/quill.gif			\
	icons/uuencoded.gif icons/blank.gif icons/folder.open.gif	\
	icons/movie.gif icons/right.gif icons/world1.gif		\
	icons/bomb.gif icons/folder.sec.gif icons/p.gif			\
	icons/screw1.gif icons/world2.gif icons/box1.gif		\
	icons/forward.gif icons/patch.gif icons/screw2.gif		\
	icons/box2.gif icons/generic.gif icons/pdf.gif			\
	icons/script.gif icons/broken.gif icons/generic.red.gif		\
	icons/pie0.gif icons/sound1.gif icons/burst.gif			\
	icons/generic.sec.gif icons/pie1.gif icons/sound2.gif

distribution:
	@-for i in ${CONFFILES}; do \
		j=`dirname $$i`; \
		echo "Installing ${DESTDIR}${WWWROOT}/$$i"; \
		${INSTALL} ${INSTALL_COPY} -g ${BINGRP} -m 444 \
		${.CURDIR}/$$i ${DESTDIR}${WWWROOT}/$$j/; \
	done
	@-for i in ${HTDOCS}; do \
		j=`dirname $$i`; \
		echo "Installing ${DESTDIR}${WWWROOT}/$$i"; \
		${INSTALL} ${INSTALL_COPY} -g ${BINGRP} -m 444 \
		${.CURDIR}/$$i ${DESTDIR}${WWWROOT}/$$j/; \
	done
	@-for i in ${MANUALFILES}; do \
		j=`dirname $$i`; \
		echo "Installing ${DESTDIR}${WWWROOT}/$$i"; \
		${INSTALL} ${INSTALL_COPY} -g ${BINGRP} -m 444 \
		${.CURDIR}/htdocs/$$i ${DESTDIR}${WWWROOT}/htdocs/$$j/; \
	done
	@-for i in ${CGIFILES}; do \
		j=`dirname $$i`; \
		echo "Installing ${DESTDIR}${WWWROOT}/$$i"; \
		${INSTALL} ${INSTALL_COPY} -g ${BINGRP} -m 000 \
		${.CURDIR}/$$i ${DESTDIR}${WWWROOT}/$$j/; \
	done
	@-for i in ${ICONFILES}; do \
		j=`dirname $$i`; \
		echo "Installing ${DESTDIR}${WWWROOT}/$$i"; \
		${INSTALL} ${INSTALL_COPY} -g ${BINGRP} -m 444 \
		${.CURDIR}/$$i ${DESTDIR}${WWWROOT}/$$j/; \
	done

.include<bsd.subdir.mk>
