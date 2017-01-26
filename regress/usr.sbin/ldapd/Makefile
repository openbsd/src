# $OpenBSD: Makefile,v 1.6 2017/01/26 02:06:37 jmatthew Exp $

OUT=${.CURDIR}/out
DIT=dc=example,dc=com
DIT2=dc=bar,dc=quux
NLV=0.65
CLEANFILES=ldapd.pid log ldapd1.conf ${DIT}_indx.db ${DIT}_data.db ${DIT2}_indx.db ${DIT2}_data.db

# Needs p5-ldap and openldap (client)

PERL_REQUIRE !=	perl -Mstrict -Mwarnings -e ' \
    eval { require Net::LDAP } or print $@; \
'
.if ! empty (PERL_REQUIRE)
regress:
	@echo "${PERL_REQUIRE}"
	@echo install package p5-ldap
	@echo SKIPPED
.endif

REGRESS_TARGETS = connect import perl purge #cpan

bootstrap:
	@[ -z "${SUDO}" ] || ${SUDO} true
	@${SUDO} install -m 600 -o root ${.CURDIR}/ldapd.conf ${.OBJDIR}/ldapd1.conf
	@${SUDO} ldapd -n -r ${.OBJDIR} -f ${.OBJDIR}/ldapd1.conf
	@${SUDO} ldapd -dvv -r ${.OBJDIR} -f ${.OBJDIR}/ldapd1.conf > ${.OBJDIR}/log 2>&1 &
	@sleep 1
	@grep -a startup ${.OBJDIR}/log |sed -e 's/.*\[// ; s/\].*//' > ${.OBJDIR}/ldapd.pid

.END:
	@[ -f ${.OBJDIR}/ldapd.pid ] && ${SUDO} kill $$(cat ${.OBJDIR}/ldapd.pid) && rm ${.OBJDIR}/ldapd.pid

connect: bootstrap
	-ldapsearch -x -H ldapi://%2ftmp%2fldapi | diff - ${OUT}/empty.log
	-ldapsearch -x -p 6639 -h localhost 2>&1 | diff - ${OUT}/empty.log
	-ldapsearch -x -p 6636 -h localhost 2>&1 | diff - ${OUT}/empty.log

import: bootstrap
	ldapadd -f ${.CURDIR}/dit-example.ldif -Dcn=admin,${DIT} -x -w secret -h localhost -p 6639 | diff - ${OUT}/adding.log
	ldapsearch -x -b ${DIT} -h localhost -p 6639 | diff - ${OUT}/example.log
	ldapsearch -x -D cn=admin,${DIT} -w secret -b ${DIT} -h localhost -p 6639 | diff - ${OUT}/example.log

purge: bootstrap
	ldapdelete -Dcn=admin,${DIT} -x -w secret -h localhost -p 6639 -r ${DIT}
	-ldapsearch -x -p 6639 -h localhost 2>&1 | diff - ${OUT}/empty.log

perl: bootstrap
	@-perl ${.CURDIR}/run-tests.pl

# run tests from Net::LDAP
# requires all of databases/p5-ldap RUN_DEPENDS
# tests above 41 fail because ldapd doesnt handle uppercase namespaces
cpan: bootstrap
	tar -C ${.OBJDIR} -xzf /usr/ports/distfiles/perl-ldap-${NLV}.tar.gz
	@${SUDO} install -m 600 -o root ${.CURDIR}/nldapd.conf ${.OBJDIR}/perl-ldap-${NLV}
	cp common.pl ${.OBJDIR}/perl-ldap-${NLV}/t
	cd ${.OBJDIR}/perl-ldap-${NLV} && perl Makefile.PL --skipdeps
	sed -i -e 's/OpenLDAPperson/inetOrgPerson/' ${.OBJDIR}/perl-ldap-${NLV}/data/*.ldif
	@-${SUDO} ${MAKE} -C ${.OBJDIR}/perl-ldap-${NLV} test

.include <bsd.regress.mk>
