# $OpenBSD: Makefile,v 1.1 2016/09/01 14:59:52 landry Exp $

LOG=${.CURDIR}/log
OUT=${.CURDIR}/out
WRK!=mktemp -d
DIT=dc=example,dc=com
DIT2=dc=bar,dc=quux
NLV=0.65

# Needs p5-ldap and openldap (client)

PERL_REQUIRE !=	perl -Mstrict -Mwarnings -e ' \
    eval { require Net::LDAP } or print $@; \
'
.if ! empty (PERL_REQUIRE)
regress:
	@echo "${PERL_REQUIRE}"
	@echo install p5-ldap
	@exit 1
.endif

.BEGIN:
	@[ -z "${SUDO}" ] || ${SUDO} true
	@${SUDO} install -m 600 -o root ${.CURDIR}/ldapd.conf ${WRK}
	@${SUDO} ldapd -n -r ${WRK} -f ${WRK}/ldapd.conf
	@${SUDO} ldapd -dvv -r ${WRK} -f ${WRK}/ldapd.conf > ${LOG} 2>&1 &
	@sleep 1
	@grep -a startup ${LOG} |sed -e 's/.*\[// ; s/\].*//' > ldapd.pid
.END:
	@${SUDO} kill $$(cat ldapd.pid)
	@-${SUDO} rm -Rf ${WRK}

clean:
	@-${SUDO} rm -Rf ${WRK}

all: connect import perl cpan

connect:
	-ldapsearch -x -H ldapi://%2ftmp%2fldapi | diff - ${OUT}/empty.log
	-ldapsearch -x -p 6639 -h localhost 2>&1 | diff - ${OUT}/empty.log
	-ldapsearch -x -p 6636 -h localhost 2>&1 | diff - ${OUT}/empty.log

import:
	ldapadd -f ${.CURDIR}/dit-example.ldif -Dcn=admin,${DIT} -x -w secret -h localhost -p 6639 | diff - ${OUT}/adding.log
	ldapsearch -x -b ${DIT} -h localhost -p 6639 | diff - ${OUT}/example.log
	ldapsearch -x -D cn=admin,${DIT} -w secret -b ${DIT} -h localhost -p 6639 | diff - ${OUT}/example.log

purge: import
	ldapdelete -Dcn=admin,${DIT} -x -w secret -h localhost -p 6639 -r ${DIT}
	-ldapsearch -x -p 6639 -h localhost 2>&1 | diff - ${OUT}/empty.log

perl:
	@-perl run-tests.pl

# run tests from Net::LDAP
# requires all of databases/p5-ldap RUN_DEPENDS
# tests above 41 fail because ldapd doesnt handle uppercase namespaces
cpan:
	tar -C ${WRK} -xzf /usr/ports/distfiles/perl-ldap-${NLV}.tar.gz
	@${SUDO} install -m 600 -o root ${.CURDIR}/nldapd.conf ${WRK}/perl-ldap-${NLV}
	cp common.pl ${WRK}/perl-ldap-${NLV}/t
	cd ${WRK}/perl-ldap-${NLV} && perl Makefile.PL --skipdeps
	sed -i -e 's/OpenLDAPperson/inetOrgPerson/' ${WRK}/perl-ldap-${NLV}/data/*.ldif
	@-${SUDO} ${MAKE} -C ${WRK}/perl-ldap-${NLV} test

.include <bsd.regress.mk>
