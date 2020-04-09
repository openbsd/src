#	$OpenBSD: Makefile,v 1.11 2020/04/09 19:59:38 tobhe Exp $

# Copyright (c) 2020 Tobias Heider <tobhe@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

REGRESS_SETUP_ONCE =	setup_certs
REGRESS_CLEANUP =	cleanup
CLEANFILES =		*.conf *.cnf *.csr *.key *.crt *.srl

LEFT_SSH ?=
RIGHT_SSH ?=
LEFT_ADDR ?=
RIGHT_ADDR ?=

.if empty(LEFT_SSH) || empty(RIGHT_SSH) || empty(LEFT_ADDR) || empty(RIGHT_ADDR)
regress:
	@echo this test needs two remote machines to operate
	@echo LEFT_SSH RIGHT_SSH RIGHT_ADDR LEFT_ADDR are not defined
	@echo SKIPPED
.endif

TEST_FLOWS = \
	[ -z $$tmode ] && tmode=tunnel; \
	success=false; \
	count=0; \
	while [[ $$count -le 3 ]]; do \
		ipsecctlleft=`ssh ${LEFT_SSH} ipsecctl -sa`; \
		ipsecctlright=`ssh ${RIGHT_SSH} ipsecctl -sa`; \
		flowleft=`echo "$$ipsecctlleft" \
		    | sed -n "/^flow $$flowtype in from ${RIGHT_ADDR} to ${LEFT_ADDR}/p"`; \
		flowright=`echo "$$ipsecctlright" \
		    | sed -n "/^flow $$flowtype in from ${LEFT_ADDR} to ${RIGHT_ADDR}/p"`; \
		saleft_rtol=`echo "$$ipsecctlleft" \
		    | sed -n "/^$$flowtype $$tmode from ${RIGHT_ADDR} to ${LEFT_ADDR}/p"`; \
		saleft_ltor=`echo "$$ipsecctlleft" \
		    | sed -n "/^$$flowtype $$tmode from ${LEFT_ADDR} to ${RIGHT_ADDR}/p"`; \
		saright_rtol=`echo "$$ipsecctlright" \
		    | sed -n "/^$$flowtype $$tmode from ${RIGHT_ADDR} to ${LEFT_ADDR}/p"`; \
		saright_ltor=`echo "$$ipsecctlright" \
		    | sed -n "/^$$flowtype $$tmode from ${LEFT_ADDR} to ${RIGHT_ADDR}/p"`; \
		if [[ -n "$$saleft_ltor" && -n "$$saleft_rtol" && \
		     -n "$$saright_ltor" && -n "$$saright_rtol" && \
		     -n "$$flowleft" && -n "$$flowright" ]]; then \
			 success=true; \
			 break; \
		fi; \
		let count=$$count+1; \
	done; \
	if [[ "$$success" = false ]]; then \
		echo "error: SAs not found:\n$$ipsecctlleft\n$$ipsecctlright"; \
		exit 1; \
	fi

TEST_PING = \
	dump=`ssh ${LEFT_SSH} "tcpdump -n -c2 -i enc0 -w '/tmp/test.pcap' > /dev/null & \
	     ping -c 5 ${RIGHT_ADDR} > /dev/null && tcpdump -n -r /tmp/test.pcap" && rm -f /tmp/test.pcap`; \
	rtol=`echo "$$dump" \
	    | sed -n "/(authentic,confidential): SPI 0x[0-9a-f]\{8\}: ${LEFT_ADDR} > ${RIGHT_ADDR}/p"`; \
	ltor=`echo "$$dump" \
	    | sed -n "/(authentic,confidential): SPI 0x[0-9a-f]\{8\}: ${RIGHT_ADDR} > ${LEFT_ADDR}/p"`; \
	if [[ -z "$$rtol" || -z "$$ltor" ]]; then \
		echo "error: no esp traffic."; \
		exit 1; \
	fi; \
	echo "$$dump"

SETUP_CONFIGS = \
	authstr=""; \
	if [[ "$$auth" = "psk" ]]; then \
		psk=`openssl rand -hex 20`; \
		authstr="psk $$psk"; \
	fi; \
	ipcomp=""; \
	if [[ "$$flowtype" = "ipcomp" ]]; then \
		ipcomp="ipcomp"; \
	fi; \
	fragstr=""; \
	if [ "$$fragmentation" = true ]; then \
		fragstr="set fragmentation"; \
	fi; \
	echo "FRAGMENTATION=\"$$fragstr\"" > $@_left.conf; \
	echo "TMODE=\"$$tmode\"" >> $@_left.conf; \
	echo "LOCAL_ADDR=\"${LEFT_ADDR}\"" >> $@_left.conf; \
	echo "PEER_ADDR=\"${RIGHT_ADDR}\"" >> $@_left.conf; \
	echo "IPCOMP=\"$$ipcomp\"" >> $@_left.conf; \
	echo "SRCID=\"$$leftid\"" >> $@_left.conf; \
	echo "AUTH=\"$$authstr\"" >> $@_left.conf; \
	cat ${.CURDIR}/iked.in >> $@_left.conf; \
	chmod 0600 $@_left.conf; \
	echo "cd /tmp\nput $@_left.conf test.conf" | sftp -q ${LEFT_SSH}; \
	echo "FRAGMENTATION=\"$$fragstr\"" > $@_right.conf; \
	echo "TMODE=\"$$tmode\"" >> $@_right.conf; \
	echo "LOCAL_ADDR=\"${RIGHT_ADDR}\"" >> $@_right.conf; \
	echo "PEER_ADDR=\"${LEFT_ADDR}\"" >> $@_right.conf; \
	echo "IPCOMP=\"$$ipcomp\"" >> $@_right.conf; \
	echo "SRCID=\"$$rightid\"" >> $@_right.conf; \
	echo "AUTH=\"$$authstr\"" >> $@_right.conf; \
	cat ${.CURDIR}/iked.in >> $@_right.conf; \
	chmod 0600 $@_right.conf; \
	echo "cd /tmp\nput $@_right.conf test.conf" | sftp -q ${RIGHT_SSH}

SETUP_SYSCTL = \
	ssh ${LEFT_SSH} "sysctl $$sysctl"; \
	ssh ${RIGHT_SSH} "sysctl $$sysctl"

SETUP_START = \
	ssh ${LEFT_SSH} "ipsecctl -F; pkill iked; iked $$iked_flags -f /tmp/test.conf"; \
	ssh ${RIGHT_SSH} "ipsecctl -F; pkill iked; iked $$iked_flags -f /tmp/test.conf"

SETUP_CERT = \
	echo "ALTNAME = $$name-from-$$caname" > $$name-from-$$caname.cnf; \
	cat ${.CURDIR}/crt.in >> $$name-from-$$caname.cnf; \
	openssl req -config $$name-from-$$caname.cnf -new -key $$name.key -nodes \
	    -out $$name-from-$$caname.csr; \
	openssl x509 -extfile $$name-from-$$caname.cnf -extensions req_cert_extensions \
	     -req -in $$name-from-$$caname.csr -CA $$caname.crt -CAkey $$caname.key \
	     -CAcreateserial -out $$name-from-$$caname.crt

SETUP_CA = \
	openssl genrsa -out $$caname.key 2048; \
	openssl req -subj "/C=DE/ST=Bavaria/L=Munich/O=iked/CN=$$caname" \
	     -new -x509 -key $$caname.key -out $$caname.crt

cleanup:
	-ssh ${LEFT_SSH} 'rm -f /tmp/test.conf; ipsecctl -F; pkill iked; \
	    rm -f /etc/iked/ca/*; rm -f /etc/iked/certs/*; rm -f /etc/iked/private/*; \
	    sysctl "net.inet.esp.udpencap_port=4500"'
	-ssh ${RIGHT_SSH} 'rm -f /tmp/test.conf; ipsecctl -F; pkill iked; \
	    rm -f /etc/iked/ca/*; rm -f /etc/iked/certs/*; rm -f /etc/iked/private/*; \
	    sysctl "net.inet.esp.udpencap_port=4500"'

setup_certs: ca-both.crt left-from-ca-both.crt left.key right-from-ca-both.crt \
    right.key ca-left.crt right-from-ca-left.crt ca-right.crt left-from-ca-right.crt
	echo "cd /etc/iked\n \
	    put left-from-ca-both.crt certs\n \
	    put left-from-ca-right.crt certs\n \
	    put left.key private/local.key\n \
	    put ca-left.crt ca\n \
	    put ca-both.crt ca\n" | sftp ${LEFT_SSH} -q; \
	echo "cd /etc/iked\n \
	    put right-from-ca-both.crt certs\n \
	    put right-from-ca-left.crt certs\n \
	    put right.key private/local.key\n \
	    put ca-right.crt ca\n \
	    put ca-both.crt ca\n" | sftp ${RIGHT_SSH} -q; \
	ssh ${LEFT_SSH} "openssl rsa -in /etc/iked/private/local.key -pubout > /etc/iked/local.pub"; \
	ssh ${RIGHT_SSH} "openssl rsa -in /etc/iked/private/local.key -pubout > /etc/iked/local.pub"

.PHONY: setup_certs

test_flows:
	${TEST_FLOWS}

left.key right.key:
	openssl genrsa -out $@ 2048

ca-both.crt ca-both.key:
	caname=ca-both; ${SETUP_CA}

left-from-ca-both.crt: ca-both.crt ca-both.key left.key
	caname=ca-both; name=left; ${SETUP_CERT}

right-from-ca-both.crt: ca-both.crt ca-both.key right.key
	caname=ca-both; name=right; ${SETUP_CERT}

ca-left.crt ca-left.key:
	caname=ca-left; ${SETUP_CA}

right-from-ca-left.crt right.key: ca-left.crt ca-left.key
	caname=ca-left; name=right; ${SETUP_CERT}

ca-right.crt ca-right.key:
	caname=ca-right; ${SETUP_CA}

left-from-ca-right.crt left.key: ca-right.crt ca-right.key
	caname=ca-right; name=left; ${SETUP_CERT}

REGRESS_TARGETS = run-cert-single-ca
run-cert-single-ca:
	@echo '======= $@ ========'
	flowtype=esp;
	leftid=left-from-ca-both; \
	rightid=right-from-ca-both; \
	    ${SETUP_CONFIGS}
	${SETUP_START}
	flowtype=esp; ${TEST_FLOWS}
	${TEST_PING}

REGRESS_TARGETS += run-cert-multi-ca
run-cert-multi-ca:
	@echo '======= $@ ========'
	flowtype=esp; \
	leftid=left-from-ca-right; \
	rightid=right-from-ca-left; \
	    ${SETUP_CONFIGS}
	${SETUP_START}
	flowtype=esp; ${TEST_FLOWS}
	${TEST_PING}

REGRESS_TARGETS += run-cert-second-altname
run-cert-second-altname:
	@echo '======= $@ ========'
	flowtype=esp;
	leftid=left-from-ca-both-alternative; \
	rightid=right-from-ca-both@openbsd.org; \
	    ${SETUP_CONFIGS}
	${SETUP_START}
	flowtype=esp; ${TEST_FLOWS}
	${TEST_PING}

REGRESS_TARGETS += run-psk
run-psk:
	@echo '======= $@ ========'
	auth=psk; \
	leftid=left; \
	rightid=right; \
	flowtype=esp; \
	    ${SETUP_CONFIGS}
	${SETUP_START}
	flowtype=esp; ${TEST_FLOWS}
	${TEST_PING}

REGRESS_TARGETS += run-fragmentation
run-fragmentation:
	@echo '======= $@ ========'
	flowtype=esp; \
	fragmentation=true; \
	leftid=left-from-ca-both; \
	rightid=right-from-ca-both; \
	    ${SETUP_CONFIGS}
	${SETUP_START}
	flowtype=esp; ${TEST_FLOWS}
	${TEST_PING}

REGRESS_TARGETS += run-transport
run-transport:
	@echo '======= $@ ========'
	flowtype=esp; \
	tmode=transport; \
	leftid=left-from-ca-both; \
	rightid=right-from-ca-both; \
	    ${SETUP_CONFIGS}
	${SETUP_START}
	tmode=transport; flowtype=esp; \
	  ${TEST_FLOWS}
	${TEST_PING}

REGRESS_TARGETS += run-ipcomp
run-ipcomp:
	@echo '======= $@ ========'
	flowtype=ipcomp; \
	leftid=left-from-ca-both; \
	rightid=right-from-ca-both; \
	    ${SETUP_CONFIGS}
	sysctl="net.inet.ipcomp.enable=1"; \
	    ${SETUP_SYSCTL}
	${SETUP_START}
	flowtype=ipcomp; ${TEST_FLOWS}
	${TEST_PING}

REGRESS_TARGETS += run-udpencap-port
run-udpencap-port:
	@echo '======= $@ ========'
	flowtype=esp; \
	leftid=left-from-ca-both; \
	rightid=right-from-ca-both; \
	    ${SETUP_CONFIGS}; \
	sysctl="net.inet.esp.udpencap_port=9999"; \
	    ${SETUP_SYSCTL};
	iked_flags=-p9999; \
	    ${SETUP_START};
	flowtype=esp; ${TEST_FLOWS}; \
	    ${TEST_PING}
	sysctl="net.inet.esp.udpencap_port=4500"; \
	    ${SETUP_SYSCTL};

.include <bsd.regress.mk>
