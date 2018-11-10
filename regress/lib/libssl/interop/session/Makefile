# $OpenBSD: Makefile,v 1.1 2018/11/10 08:33:45 bluhm Exp $

run-client-openssl11-reuse-server-openssl11-reuse:
	@echo '\n======== $@ ========'
	# TLS 1.3 needs some extra setup for session reuse
	@echo DISABLED

CLEANFILES +=		*.out

.for clib in libressl openssl openssl11
.for slib in libressl openssl openssl11

REGRESS_TARGETS +=	run-client-${clib}-reuse-server-${slib}-reuse

run-client-${clib}-reuse-server-${slib}-reuse: 127.0.0.1.crt
	@echo '\n======== $@ ========'
	LD_LIBRARY_PATH=/usr/local/lib/e${slib} \
	    ../${slib}/server >server-${slib}-reuse.out \
	    -ss \
	    127.0.0.1 0
	LD_LIBRARY_PATH=/usr/local/lib/e${clib} \
	    ../${clib}/client >client-${clib}-reuse.out \
	    -ss \
	    `sed -n 's/listen sock: //p' server-${slib}-reuse.out`
	grep '^success$$' server-${slib}-reuse.out
	grep '^success$$' client-${clib}-reuse.out
	grep '^session 2: new$$' server-${slib}-reuse.out
	grep '^session 2: new$$' client-${clib}-reuse.out
	grep '^session 1: reuse$$' server-${slib}-reuse.out
	grep '^session 1: reuse$$' client-${clib}-reuse.out
	grep '^session 0: reuse$$' server-${slib}-reuse.out
	grep '^session 0: reuse$$' client-${clib}-reuse.out

.endfor
.endfor

.include <bsd.regress.mk>
