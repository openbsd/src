#	$OpenBSD: Makefile,v 1.3 2015/12/04 23:43:04 bluhm Exp $

# The following ports must be installed:
#
# python-2.7          interpreted object-oriented programming language
# py-libdnet          python interface to libdnet
# scapy               powerful interactive packet manipulation in python

# Check wether all required python packages are installed.  If some
# are missing print a warning and skip the tests, but do not fail.
PYTHON_IMPORT != python2.7 -c 'from scapy.all import *' 2>&1 || true
.if ! empty(PYTHON_IMPORT)
regress:
	@echo '${PYTHON_IMPORT}'
	@echo install python and the scapy module for additional tests
.endif

# This test needs a manual setup of two machines
# Set up machines: LOCAL REMOTE
# LOCAL is the machine where this makefile is running.
# REMOTE is running OpenBSD with ARP to test the Address Resolution Protocol.
# FAKE is an non existing machine, its IP is used in the tests.
# OTHER is an IP on REMOTE, but configured on another interface.
# OTHERFAKE is an non existing IP on another interface.
# REMOTE_SSH is the hostname to log in on the REMOTE machine.

# Configure Addresses on the machines.
# Adapt interface and addresse variables to your local setup.
#
LOCAL_IF ?=
LOCAL_MAC ?=
REMOTE_MAC ?=
FAKE_MAC ?= 12:34:56:78:9a:bc
REMOTE_SSH ?=

LOCAL_ADDR ?= 10.188.70.17
REMOTE_ADDR ?= 10.188.70.70
FAKE_ADDR ?= 10.188.70.188
OTHER_ADDR ?= 10.188.211.70
OTHERFAKE_ADDR ?= 10.188.211.188

.if empty (LOCAL_IF) || empty (LOCAL_MAC) || empty (REMOTE_MAC) || \
    empty (FAKE_MAC) || empty (REMOTE_SSH) || empty (LOCAL_ADDR) || \
    empty (REMOTE_ADDR) || empty (FAKE_ADDR) || empty (OTHER_ADDR) || \
    empty (OTHERFAKE_ADDR)
regress:
	@echo this tests needs a remote machine to operate on
	@echo LOCAL_IF LOCAL_MAC REMOTE_MAC FAKE_MAC REMOTE_SSH LOCAL_ADDR
	@echo REMOTE_ADDR FAKE_ADDR OTHER_ADDR OTHERFAKE_ADDR are empty
	@echo fill out these variables for additional tests
.endif

depend: addr.py

# Create python include file containing the addresses.
addr.py: Makefile
	rm -f $@ $@.tmp
	echo 'LOCAL_IF = "${LOCAL_IF}"' >>$@.tmp
.for var in LOCAL REMOTE FAKE
	echo '${var}_MAC = "${${var}_MAC}"' >>$@.tmp
.endfor
.for var in LOCAL REMOTE FAKE OTHER OTHERFAKE
	echo '${var}_ADDR = "${${var}_ADDR}"' >>$@.tmp
.endfor
	mv $@.tmp $@

# Set variables so that make runs with and without obj directory.
# Only do that if necessary to keep visible output short.
.if ${.CURDIR} == ${.OBJDIR}
PYTHON =	python2.7 ./
.else
PYTHON =	PYTHONPATH=${.OBJDIR} python2.7 ${.CURDIR}/
.endif

# Clear ARP cache and ping all addresses.  This ensures that
# the IP addresses are configured and all routing table are set up
# to allow bidirectional packet flow.
TARGETS +=	ping
run-regress-ping:
	@echo '\n======== $@ ========'
	${SUDO} arp -da
	ssh -t ${REMOTE_SSH} ${SUDO} arp -da
.for ip in LOCAL_ADDR REMOTE_ADDR
	@echo Check ping ${ip}
	ping -n -c 1 ${${ip}}
.endfor

# Send an ARP request from the local machine, asking for the remote
# machine's MAC.  Target MAC is broadcast, Target IP is remote address.
# Check that all fields of the answer are filled out correctly.
# Check that the remote machine has the local IP and MAC in its ARP table.
TARGETS +=	arp-request
run-regress-arp-request: addr.py
	@echo '\n======== $@ ========'
	@echo Send ARP Request for remote address and insert local address
	ssh -t ${REMOTE_SSH} ${SUDO} arp -d ${LOCAL_ADDR}
	${SUDO} ${PYTHON}arp_request.py
	ssh ${REMOTE_SSH} ${SUDO} arp -an >arp.log
	grep '^${LOCAL_ADDR} .* ${LOCAL_MAC} ' arp.log

# Send an ARP request from the local machine, but use a multicast MAC
# as sender.  Although there is a special check in in_arpinput(),
# this must be answered.  The ARP entry on the remote machine for the
# local address is changed to the multicast MAC.
# Check that all fields of the answer are filled out correctly.
# Check that the remote machine overwrites the local address.
TARGETS +=	arp-multicast
run-regress-arp-multicast: addr.py
	@echo '\n======== $@ ========'
	@echo Send ARP Request and overwrite entry with multicast ethernet
	ssh -t ${REMOTE_SSH} logger -t "arp-regress[$$$$]" $@
	ssh -t ${REMOTE_SSH} ${SUDO} arp -s ${LOCAL_ADDR} ${LOCAL_MAC} temp
	scp ${REMOTE_SSH}:/var/log/messages old.log
	${SUDO} ${PYTHON}arp_multicast.py
	scp ${REMOTE_SSH}:/var/log/messages new.log
	ssh ${REMOTE_SSH} ${SUDO} arp -an >arp.log
	ssh -t ${REMOTE_SSH} ${SUDO} arp -d ${LOCAL_ADDR}
	diff old.log new.log | grep '^> ' >diff.log
	grep 'bsd: arp info overwritten for ${LOCAL_ADDR} by 33:33:33:33:33:33' diff.log
	grep '^${LOCAL_ADDR} .* ${LOCAL_MAC} ' arp.log

# Send an ARP probe from the local machine with the remote IP as
# target.  Sender MAC is local and IP is 0.  The remote machine must
# defend its IP address with an ARP reply.
# Check that all fields of the answer are filled out correctly.
TARGETS +=	arp-probe
run-regress-arp-probe: addr.py
	@echo '\n======== $@ ========'
	@echo Send ARP Probe for existing address and expect correct reply
	${SUDO} ${PYTHON}arp_probe.py

# Send ARP request with broadcast MAC as sender.
# Check that no answer is received.
# Check that the remote machine rejects the broadcast sender.
TARGETS +=	arp-broadcast
run-regress-arp-broadcast: addr.py
	@echo '\n======== $@ ========'
	@echo Send ARP Request with broadcast as sender hardware address
	ssh -t ${REMOTE_SSH} logger -t "arp-regress[$$$$]" $@
	scp ${REMOTE_SSH}:/var/log/messages old.log
	${SUDO} ${PYTHON}arp_broadcast.py
	scp ${REMOTE_SSH}:/var/log/messages new.log
	diff old.log new.log | grep '^> ' >diff.log
	grep 'bsd: arp: ether address is broadcast for IP address ${LOCAL_ADDR}' diff.log

# The local machine announces that it has taken the remote machine's
# IP.  The sender is the local machines MAC and the remote IP.  The
# remote machine must defend its IP address with an ARP reply.
# Check that all fields of the answer are filled out correctly.
# Check that the remote machine reports an duplicate address.
# Check that the remote machine keeps its local ARP entry.
TARGETS +=	arp-announcement
run-regress-arp-announcement: addr.py
	@echo '\n======== $@ ========'
	@echo Send ARP Announcement for existing address
	ssh -t ${REMOTE_SSH} logger -t "arp-regress[$$$$]" $@
	scp ${REMOTE_SSH}:/var/log/messages old.log
	${SUDO} ${PYTHON}arp_announcement.py
	scp ${REMOTE_SSH}:/var/log/messages new.log
	ssh ${REMOTE_SSH} ${SUDO} arp -an >arp.log
	diff old.log new.log | grep '^> ' >diff.log
	grep 'bsd: duplicate IP address ${REMOTE_ADDR} sent from ethernet address ${LOCAL_MAC}' diff.log
	grep '^${REMOTE_ADDR} .* ${REMOTE_MAC} .* permanent * l$$' arp.log

# The local machine sends an gratuitous ARP reply for the remote IP
# with its local MAC.
# Check that no answer is received.
# Check that the remote machine reports an duplicate address.
# Check that the remote machine keeps its local ARP entry.
TARGETS +=	arp-gratuitous
run-regress-arp-gratuitous: addr.py
	@echo '\n======== $@ ========'
	@echo Send Gratuitous ARP for existing address
	ssh -t ${REMOTE_SSH} logger -t "arp-regress[$$$$]" $@
	scp ${REMOTE_SSH}:/var/log/messages old.log
	${SUDO} ${PYTHON}arp_gratuitous.py
	scp ${REMOTE_SSH}:/var/log/messages new.log
	ssh ${REMOTE_SSH} ${SUDO} arp -an >arp.log
	diff old.log new.log | grep '^> ' >diff.log
	grep 'bsd: duplicate IP address ${REMOTE_ADDR} sent from ethernet address ${LOCAL_MAC}' diff.log
	grep '^${REMOTE_ADDR} .* ${REMOTE_MAC} .* permanent * l$$' arp.log

# Add a permanent entry on the remote machine for a fake MAC and IP.
# Send a request form the local machine, indicating with the local
# MAC and the fake IP as sender that it claims the fake address.
# Check that no answer is received.
# Check that the attempt to overwrite the permanent entry is logged.
# Check that the remote machine keeps its permanent ARP entry.
TARGETS +=	arp-permanent
run-regress-arp-permanent: addr.py
	@echo '\n======== $@ ========'
	@echo Send ARP Request to change permanent fake address
	ssh -t ${REMOTE_SSH} logger -t "arp-regress[$$$$]" $@
	ssh -t ${REMOTE_SSH} ${SUDO} arp -s ${FAKE_ADDR} ${FAKE_MAC} permanent
	scp ${REMOTE_SSH}:/var/log/messages old.log
	${SUDO} ${PYTHON}arp_fake.py
	scp ${REMOTE_SSH}:/var/log/messages new.log
	ssh ${REMOTE_SSH} ${SUDO} arp -an >arp.log
	ssh -t ${REMOTE_SSH} ${SUDO} arp -d ${FAKE_ADDR}
	diff old.log new.log | grep '^> ' >diff.log
	grep 'bsd: arp: attempt to overwrite permanent entry for ${FAKE_ADDR} by ${LOCAL_MAC}' diff.log
	grep '^${FAKE_ADDR} .* ${FAKE_MAC} .* permanent * $$' arp.log

# The remote machine has a second address on another interface.
# The local machine claims this address in its sender IP.
# Check that no answer is received.
# Check that the attempt to overwrite the permanent entry is logged.
# Check that the remote machine keeps its local ARP entry.
TARGETS +=	arp-address
run-regress-arp-address: addr.py
	@echo '\n======== $@ ========'
	@echo Send ARP Request to change address on other interface
	ssh -t ${REMOTE_SSH} logger -t "arp-regress[$$$$]" $@
	scp ${REMOTE_SSH}:/var/log/messages old.log
	${SUDO} ${PYTHON}arp_other.py
	scp ${REMOTE_SSH}:/var/log/messages new.log
	ssh ${REMOTE_SSH} ${SUDO} arp -an >arp.log
	diff old.log new.log | grep '^> ' >diff.log
	grep 'bsd: arp: attempt to overwrite permanent entry for ${OTHER_ADDR} by ${LOCAL_MAC}' diff.log
	grep '^${OTHER_ADDR} .* permanent * l$$' arp.log

# The remote machine has a second address on another interface.  Add
# a temporary ARP entry for a fake address in this network on the
# remote machine.  The local machine tries to overwrite this address
# with its own MAC.
# Check that no answer is received.
# Check that the attempt to overwrite the permanent entry is logged.
# Check that the remote machine keeps its ARP entry.
TARGETS +=	arp-temporary
run-regress-arp-temporary: addr.py
	@echo '\n======== $@ ========'
	@echo Send ARP Request to change temporary entry on other interface
	ssh -t ${REMOTE_SSH} logger -t "arp-regress[$$$$]" $@
	ssh -t ${REMOTE_SSH} ${SUDO} arp -s ${OTHERFAKE_ADDR} ${FAKE_MAC} temp
	scp ${REMOTE_SSH}:/var/log/messages old.log
	${SUDO} ${PYTHON}arp_otherfake.py
	scp ${REMOTE_SSH}:/var/log/messages new.log
	ssh ${REMOTE_SSH} ${SUDO} arp -an >arp.log
	ssh -t ${REMOTE_SSH} ${SUDO} arp -d ${OTHERFAKE_ADDR}
	diff old.log new.log | grep '^> ' >diff.log
	grep 'bsd: arp: attempt to overwrite entry for ${OTHERFAKE_ADDR} on .* by ${LOCAL_MAC} on .*' diff.log
	grep '^${OTHERFAKE_ADDR} .* ${FAKE_MAC} ' arp.log

# The remote machine has a second address on another interface.  Create
# an incomplete ARP entry for a fake address in this network on the
# remote machine with an unsuccessful ping.  The local machine tries
# to overwrite this address with its own MAC.
# Check that no answer is received.
# Check that the attempt to add an entry is logged.
# Check that the remote machine keeps its incomplete ARP entry.
TARGETS +=	arp-incomlete
run-regress-arp-incomlete: addr.py
	@echo '\n======== $@ ========'
	@echo Send ARP Request filling an incomplete entry on other interface
	ssh -t ${REMOTE_SSH} logger -t "arp-regress[$$$$]" $@
	ssh -t ${REMOTE_SSH} ${SUDO} ping -n -w 1 -c 1 ${OTHERFAKE_ADDR} || true
	scp ${REMOTE_SSH}:/var/log/messages old.log
	${SUDO} ${PYTHON}arp_otherfake.py
	scp ${REMOTE_SSH}:/var/log/messages new.log
	ssh ${REMOTE_SSH} ${SUDO} arp -an >arp.log
	ssh -t ${REMOTE_SSH} ${SUDO} arp -d ${OTHERFAKE_ADDR}
	diff old.log new.log | grep '^> ' >diff.log
	grep 'bsd: arp: attempt to add entry for ${OTHERFAKE_ADDR} on .* by ${LOCAL_MAC} on .*' diff.log
	grep '^${OTHERFAKE_ADDR} .* (incomplete) ' arp.log

# Publish a proxy ARP entry on the remote machine for a fake address.
# The local machine requests this IP as a the target.
# Check that all fields of the answer are filled out correctly.
# Check that the remote machine has a public ARP entry.
TARGETS +=	arp-proxy
run-regress-arp-proxy: addr.py
	@echo '\n======== $@ ========'
	@echo Send ARP Request for fake address that is proxied
	ssh -t ${REMOTE_SSH} ${SUDO} arp -s ${FAKE_ADDR} ${FAKE_MAC} pub
	${SUDO} ${PYTHON}arp_proxy.py
	ssh ${REMOTE_SSH} ${SUDO} arp -an >arp.log
	ssh -t ${REMOTE_SSH} ${SUDO} arp -d ${FAKE_ADDR}
	grep '^${FAKE_ADDR} .* ${FAKE_MAC} .* static * p$$' arp.log

# Enter a static ARP entry on the remote machine for a fake address,
# but do not publish it.  The local machine requests this IP as a the
# target.
# Check that no answer is received.
# Check that the remote machine has a static ARP entry.
TARGETS +=	arp-nonproxy
run-regress-arp-nonproxy: addr.py
	@echo '\n======== $@ ========'
	@echo Send ARP Request for fake address that is not published
	ssh -t ${REMOTE_SSH} ${SUDO} arp -s ${FAKE_ADDR} ${FAKE_MAC}
	${SUDO} ${PYTHON}arp_nonproxy.py
	ssh ${REMOTE_SSH} ${SUDO} arp -an >arp.log
	ssh -t ${REMOTE_SSH} ${SUDO} arp -d ${FAKE_ADDR}
	grep '^${FAKE_ADDR} .* ${FAKE_MAC} .* static * $$' arp.log

# Publish a proxy ARP entry on the remote machine for a fake address
# on another interface.  The local machine requests this IP.  As the
# proxy entry is for another interface, it must not be answered.
# Check that no answer is received.
# Check that the remote machine has a public ARP entry.
TARGETS +=	arp-otherproxy
run-regress-arp-otherproxy: addr.py
	@echo '\n======== $@ ========'
	@echo Send ARP Request for address proxied on another interface
	ssh -t ${REMOTE_SSH} ${SUDO} arp -s ${OTHERFAKE_ADDR} ${FAKE_MAC} pub
	${SUDO} ${PYTHON}arp_otherproxy.py
	ssh ${REMOTE_SSH} ${SUDO} arp -an >arp.log
	ssh -t ${REMOTE_SSH} ${SUDO} arp -d ${OTHERFAKE_ADDR}
	grep '^${OTHERFAKE_ADDR} .* ${FAKE_MAC} .* static * p$$' arp.log

REGRESS_TARGETS =	${TARGETS:S/^/run-regress-/}

CLEANFILES +=		addr.py *.pyc *.log

.include <bsd.regress.mk>
