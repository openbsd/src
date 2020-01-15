#	$OpenBSD: Makefile,v 1.3 2020/01/15 13:27:05 bluhm Exp $

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
	success=false; \
	count=0; \
	while [[ $$count -le 3 ]]; do \
		sasleft=`ssh ${LEFT_SSH} ipsecctl -sa`; \
		flowleft=`echo "$$sasleft" \
		    | sed -n "/^flow $$flowtype in from ${RIGHT_ADDR} to ${LEFT_ADDR}/p"`; \
		sasright=`ssh ${RIGHT_SSH} ipsecctl -sa`; \
		flowright=`echo "$$sasright" \
		    | sed -n "/^flow $$flowtype in from ${LEFT_ADDR} to ${RIGHT_ADDR}/p"`; \
		if [[ -n "$$flowleft" && -n "$$flowright" ]]; then \
			 success=true; \
			 break; \
		fi; \
		let count=$$count+1; \
	done; \
	if [[ "$$success" = false ]]; then \
		echo "error: SAs not found : $$sasleft $$sasright"; \
		exit 1; \
	fi

TEST_PING = \
	dump=`ssh ${LEFT_SSH} "tcpdump -c2 -i enc0 -w '/tmp/test.pcap' > /dev/null & \
	     ping -c 5 ${RIGHT_ADDR} > /dev/null && tcpdump -r /tmp/test.pcap" && rm -f /tmp/test.pcap`; \
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
	echo "LOCAL_ADDR=\"${LEFT_ADDR}\"" >> $@_left.conf; \
	echo "PEER_ADDR=\"${RIGHT_ADDR}\"" >> $@_left.conf; \
	echo "IPCOMP=\"$$ipcomp\"" >> $@_left.conf; \
	echo "SRCID=\"left\"" >> $@_left.conf; \
	echo "AUTH=\"$$authstr\"" >> $@_left.conf; \
	cat ${.CURDIR}/iked.in >> $@_left.conf; \
	chmod 0600 $@_left.conf; \
	echo "cd /tmp\nput $@_left.conf test.conf" | sftp -q ${LEFT_SSH}; \
	echo "FRAGMENTATION=\"$$fragstr\"" > $@_right.conf; \
	echo "LOCAL_ADDR=\"${RIGHT_ADDR}\"" >> $@_right.conf; \
	echo "PEER_ADDR=\"${LEFT_ADDR}\"" >> $@_right.conf; \
	echo "IPCOMP=\"$$ipcomp\"" >> $@_right.conf; \
	echo "SRCID=\"right\"" >> $@_right.conf; \
	echo "AUTH=\"$$authstr\"" >> $@_right.conf; \
	cat ${.CURDIR}/iked.in >> $@_right.conf; \
	chmod 0600 $@_right.conf; \
	echo "cd /tmp\nput $@_right.conf test.conf" | sftp -q ${RIGHT_SSH}

SETUP_SYSCTL = \
	ssh ${LEFT_SSH} "sysctl net.inet.ipcomp.enable=1"; \
	ssh ${RIGHT_SSH} "sysctl net.inet.ipcomp.enable=1"

SETUP_START = \
	ssh ${LEFT_SSH} "ipsecctl -F; pkill iked; iked -6 -f /tmp/test.conf"; \
	ssh ${RIGHT_SSH} "ipsecctl -F; pkill iked; iked -6 -f /tmp/test.conf"

SETUP_CERT = \
	echo "ALTNAME = $$name" > crt_$$name.cnf; \
	cat ${.CURDIR}/crt.in >> crt_$$name.cnf; \
	openssl req -config crt_$$name.cnf -new -newkey rsa:2048 -nodes \
	    -keyout $$name.key -out $$name.csr; \
	openssl x509 -extfile crt_$$name.cnf -extensions req_cert_extensions \
	     -req -in $$name.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out $$name.crt

ca.key:
	openssl genrsa -out ca.key 2048

ca.crt:
	openssl req -subj "/C=DE/ST=Bavaria/L=Munich/O=iked/CN=ca" -new -x509 -key ca.key -out ca.crt

cleanup:
	-ssh ${LEFT_SSH} 'rm -f /tmp/test.conf; ipsecctl -F; pkill iked'
	-ssh ${RIGHT_SSH} 'rm -f /tmp/test.conf; ipsecctl -F; pkill iked'

REGRESS_TARGETS =	run-cert run-psk run-fragmentation run-ipcomp

setup_certs: ca.key ca.crt
	name=left; ${SETUP_CERT}; \
	echo "cd /etc/iked\nput left.crt certs\nput left.key private/local.key\nput ca.crt ca\n" \
	     | sftp ${LEFT_SSH} -q; \
	name=right; ${SETUP_CERT}; \
	echo "cd /etc/iked\nput right.crt certs\nput right.key private/local.key\nput ca.crt ca\n" \
	     | sftp ${RIGHT_SSH} -q

run-cert:
	@echo '======= $@ ========'
	flowtype=esp; \
	${SETUP_CONFIGS}; \
	${SETUP_START}; \
	${TEST_FLOWS}; \
	${TEST_PING}

run-psk:
	@echo '======= $@ ========'
	auth=psk; \
	flowtype=esp; \
	${SETUP_CONFIGS}; \
	${SETUP_START}; \
	${TEST_FLOWS}; \
	${TEST_PING}

run-fragmentation:
	@echo '======= $@ ========'
	flowtype=esp; \
	fragmentation=true; \
	${SETUP_CONFIGS}; \
	${SETUP_START}; \
	${TEST_FLOWS}; \
	${TEST_PING}

run-ipcomp:
	@echo '======= $@ ========'
	flowtype=ipcomp; \
	${SETUP_CONFIGS}; \
	${SETUP_SYSCTL}; \
	${SETUP_START}; \
	${TEST_FLOWS}; \
	${TEST_PING}

.include <bsd.regress.mk>
