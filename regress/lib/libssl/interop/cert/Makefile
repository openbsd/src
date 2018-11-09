# $OpenBSD: Makefile,v 1.1 2018/11/09 06:30:41 bluhm Exp $

.if ! exists(/usr/local/bin/eopenssl) || ! exists(/usr/local/bin/eopenssl11)
regress:
	# install openssl-1.0.2p and openssl-1.1.1 from ports
	@echo SKIPPED
.endif

CLEANFILES +=	client.out server.out

.for cca in noca ca fakeca
.for sca in noca ca fakeca
.for ccert in nocert cert
.for scert in nocert cert
.for cv in noverify verify
.for sv in noverify verify certverify

# remember when certificate verification should fail
.if (("${cv}" == verify && "${cca}" == ca && "${scert}" == cert) || \
    "${cv}" == noverify) && \
    (("${sv}" == verify && "${ccert}" == nocert) || \
    ("${sv}" == verify && "${sca}" == ca && "${ccert}" == cert) || \
    ("${sv}" == certverify && "${sca}" == ca && "${ccert}" == cert) || \
    "${sv}" == noverify)
FAIL_${cca}_${sca}_${ccert}_${scert}_${cv}_${sv} =
.else
FAIL_${cca}_${sca}_${ccert}_${scert}_${cv}_${sv} = !
.endif

.for clib in libressl openssl openssl11
.for slib in libressl openssl openssl11

REGRESS_TARGETS +=	\
run-client-${clib}-${cca}-${ccert}-${cv}-server-${slib}-${sca}-${scert}-${sv}

run-client-${clib}-${cca}-${ccert}-${cv}-server-${slib}-${sca}-${scert}-${sv}:\
    127.0.0.1.crt ca.crt fake-ca.crt client.crt server.crt \
    ../${clib}/client ../${slib}/server
	@echo '\n======== $@ ========'
	LD_LIBRARY_PATH=/usr/local/lib/e${slib} \
	    ../${slib}/server >server.out \
	    ${sca:S/^noca//:S/^fakeca/-C fake-ca.crt/:S/^ca/-C ca.crt/} \
	    ${scert:S/^nocert//:S/^cert/-c server.crt -k server.key/} \
	    ${sv:S/^noverify//:S/^verify/-v/:S/^certverify/-vv/} \
	    127.0.0.1 0
	${FAIL_${cca}_${sca}_${ccert}_${scert}_${cv}_${sv}} \
	    LD_LIBRARY_PATH=/usr/local/lib/e${clib} \
	    ../${clib}/client >client.out \
	    ${cca:S/^noca//:S/^fakeca/-C fake-ca.crt/:S/^ca/-C ca.crt/} \
	    ${ccert:S/^nocert//:S/^cert/-c server.crt -k server.key/} \
	    ${cv:S/^noverify//:S/^verify/-v/} \
	    `sed -n 's/listen sock: //p' server.out`
.if empty(${FAIL_${cca}_${sca}_${ccert}_${scert}_${cv}_${sv}})
	grep '^success$$' server.out
	grep '^success$$' client.out
.elif ! ("${sv}" == certverify && "${ccert}" == nocert) || \
    ("${cv}" == verify && "${scert}" != cert)
	grep '^verify: fail' client.out server.out
.endif

.endfor
.endfor
.endfor
.endfor
.endfor
.endfor
.endfor
.endfor

.include <bsd.regress.mk>
