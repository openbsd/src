#!/bin/sh
#
# $OpenBSD: snmpd.sh,v 1.18 2022/01/19 11:02:38 martijn Exp $
#/*
# * Copyright (c) Rob Pierce <rob@openbsd.org>
# *
# * Permission to use, copy, modify, and distribute this software for any
# * purpose with or without fee is hereby granted, provided that the above
# * copyright notice and this permission notice appear in all copies.
# *
# * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
# */

# Basic snmpd regression script.

export OBJDIR

FAILED=0
SLEEP=1
PF[0]="disabled"
PF[1]="enabled"

# This file will be creatred by traphandler.c as user _snmpd
TMPFILE=$(mktemp -q /tmp/_snmpd_traptest.XXXXXX)

trap 'skip' INT

if [ "$(pgrep snmpd)" ]
then
	echo "The snmpd daemon is already running."
	echo SKIPPED
	exit 0
fi

cleanup() {
	rm ${TMPFILE} >/dev/null 2>&1
	rm ${OBJDIR}/nohup.out >/dev/null 2>&1
	rm ${OBJDIR}/snmpd.log >/dev/null 2>&1
	rm ${OBJDIR}/snmpd.conf >/dev/null 2>&1
}

fail() {
	echo FAILED
	cleanup
	exit 1
}

skip() {
	echo SKIPPED
	cleanup
	exit 0
}

# # # # # CONFIG ONE # # # # #

echo "\nConfiguration: default community strings, trap receiver, trap handle\n"

cat > ${OBJDIR}/snmpd.conf <<EOF
# This is config template (1) for snmpd regression testing
# Restrict daemon to listen on localhost only
listen on 127.0.0.1 snmpv1 snmpv2c snmpv3
listen on 127.0.0.1 snmpv2c notify
listen on ::1 snmpv1 snmpv2c snmpv3
listen on ::1 snmpv2c notify

# Specify communities
read-only community public
read-write community private
trap community public

trap handle 1.2.3.4 "/usr/bin/touch ${TMPFILE}"
EOF

(cd ${OBJDIR} && nohup snmpd -dvf ./snmpd.conf > snmpd.log 2>&1) &

sleep ${SLEEP}

[ ! -n "$(pgrep snmpd)" ] && echo "Failed to start snmpd." && fail

# pf (also checks "oid all" which obtains privileged kernel data

pf_enabled="$(pfctl -si | grep ^Status | awk '{ print $2 }' | tr [A-Z] [a-z])"
snmp_command="snmp walk -Oq -v2c -cpublic localhost 1.3"
echo ======= $snmp_command
enabled="$(eval $snmp_command | grep -vi parameters | grep -i pfrunning | awk '{ print $2 }')"
if [ "${PF[$enabled]}" != "${PF[enabled]}" ]
then
	if [ "${PF[$enabled]}" != "${PF[disabled]}" ]
	then
		echo "Retrieval of pf status failed."
		FAILED=1
	fi
fi

# hostname

sys_name=$(hostname)
snmp_command="snmp get -Oqv -v2c -cpublic localhost 1.3.6.1.2.1.1.5.0"
echo ======= $snmp_command
name="$(eval $snmp_command)"
if [ "$name" != "$sys_name" ]
then
	echo "Retrieval of hostname failed."
	FAILED=1
fi

# carp allow

carp="$(sysctl net.inet.carp.allow | awk -F= '{ print $2 }')"
snmp_command="snmp get -On -v2c -cpublic localhost 1.3.6.1.4.1.30155.6.1.1.0"
echo ======= $snmp_command
carp_allow="$(eval $snmp_command)"
carp_allow="${carp_allow##.1.3.6.1.4.1.30155.6.1.1.0 = INTEGER: }"
if [ "$carp" -ne "$carp_allow" ]
then
	echo "Retrieval of carp.allow failed."
	FAILED=1
fi

# carp allow with default ro community string

carp="$(sysctl net.inet.carp.allow | awk -F= '{ print $2 }')"
snmp_command="snmp getnext -Onq -v2c -cpublic localhost 1.3.6.1.4.1.30155.6.1.1"
echo ======= $snmp_command
carp_allow="$(eval $snmp_command)"
carp_allow="${carp_allow##.1.3.6.1.4.1.30155.6.1.1.0 }"
if [ "$carp" -ne "$carp_allow" ]
then
	echo "Retrieval of carp.allow with default ro community string failed."
	FAILED=1
fi

# trap handler with command execution

rm -f ${TMPFILE}
snmp_command="snmp trap -v2c -cpublic 127.0.0.1 '' 1.2.3.4"
echo ======= $snmp_command
eval $snmp_command
sleep ${SLEEP}
if [ ! -f "${TMPFILE}" ]
then
	echo "Trap handler test failed."
	FAILED=1
fi

# system.sysContact set with default rw community string

# Currently no set support in snmpd
#puffy="puffy@openbsd.org"
#snmp_command="snmp set -c private -v 1 localhost system.sysContact.0 s $puffy"
#echo ======= $snmp_command
#eval $snmp_command > /dev/null 2>&1
#snmp_command="snmp get -v2c -cpublic localhost 1.3.6.1.2.1.1.4.0"
#echo ======= $snmp_command
#contact="$(eval $snmp_command)"
#contact="${contact##sysContact.0 = STRING: }"
#if [ "$contact" !=  "$puffy" ]
#then
#	echo "Setting with default rw community string failed."
#	FAILED=1
#fi

kill $(pgrep snmpd) >/dev/null 2>&1
wait

# # # # # CONFIG TWO # # # # #
echo "\nConfiguration: seclevel auth\n"

cat > ${OBJDIR}/snmpd.conf <<EOF
# This is config template (2) for snmpd regression testing
# Restrict daemon to listen on localhost only
listen on 127.0.0.1
listen on ::1

seclevel auth

user "hans" authkey "password123" auth hmac-sha1
EOF

(cd ${OBJDIR} && nohup snmpd -dvf ./snmpd.conf > snmpd.log 2>&1) &

sleep ${SLEEP}

[ ! -n "$(pgrep snmpd)" ] && echo "Failed to start snmpd." && fail

# make sure we can't get an oid with deault community string

snmp_command="snmp get -r2 -v2c -cpublic localhost 1.3.6.1.2.1.1.5.0"
echo ======= $snmp_command
eval $snmp_command > /dev/null 2>&1
if [ $? -eq 0 ]
then
	echo "Non-defaut ro community string test failed."
	FAILED=1
fi

# get with SHA authentication

os="$(uname -s)"
snmp_command="snmp get -v3 -Oq -l authNoPriv -u hans -a SHA -A password123 \
   localhost system.sysDescr.0"
echo ======= $snmp_command
system="$(eval $snmp_command | awk '{ print $2 }')"
if [ "$system" != "$os" ]
then
	echo "Retrieval test with seclevel auth and SHA failed."
	FAILED=1
fi

kill $(pgrep snmpd) >/dev/null 2>&1
wait

# # # # # CONFIG THREE # # # # #
echo "\nConfiguration: seclevel enc\n"

cat > ${OBJDIR}/snmpd.conf <<EOF
# This is config template (3) for snmpd regression testing
# Restrict daemon to listen on localhost only
listen on 127.0.0.1
listen on ::1

seclevel enc

user "hans" authkey "password123" auth hmac-sha1 enc aes enckey "321drowssap"
EOF

(cd ${OBJDIR} && nohup snmpd -dvf ./snmpd.conf > snmpd.log 2>&1) &

sleep ${SLEEP}

[ ! -n "$(pgrep snmpd)" ] && echo "Failed to start snmpd." && fail

# get with SHA authentication and AES encryption

os="$(uname -s)"
snmp_command="snmp get -v3 -Oq -l authPriv -u hans -a SHA -A password123 -x AES \
   -X 321drowssap localhost system.sysDescr.0"
echo ======= $snmp_command
system="$(eval $snmp_command | awk '{ print $2 }')"
if [ "$system" != "$os" ]
then
	echo "seclevel auth with SHA failed"
	FAILED=1
fi

kill $(pgrep snmpd) >/dev/null 2>&1
wait

# # # # # CONFIG FOUR # # # # #
echo "\nConfiguration: non-default community strings, custom oids\n"

replacement="$(printf '\357\277\275')"
oe="$(printf '\303\266')"
boe="$(printf '\303')"
cat > ${OBJDIR}/snmpd.conf <<EOF
# This is config template (4) for snmpd regression testing
# Restrict daemon to listen on localhost only
listen on 127.0.0.1 snmpv1 snmpv2c
listen on ::1 snmpv1 snmpv2c

read-only community non-default-ro

read-write community non-default-rw

oid 1.3.6.1.4.1.30155.42.1 name myName read-only string "humppa"
oid 1.3.6.1.4.1.30155.42.2 name myStatus read-only integer 1
# No need to place a full index, we just need the object
oid 1.3.6.1.6.3.15.1.2.2.1.3.1 name Reyk read-only string "Reyk Fl${oe}ter"
oid 1.3.6.1.6.3.15.1.2.2.1.3.2 name broken read-only string "br${boe}ken"
EOF

(cd ${OBJDIR} && nohup snmpd -dvf ./snmpd.conf > snmpd.log 2>&1) &

sleep ${SLEEP}

[ ! -n "$(pgrep snmpd)" ] && echo "Failed to start snmpd." && fail

# carp allow with non-default ro community string

carp="$(sysctl net.inet.carp.allow | awk -F= '{ print $2 }')"
snmp_command="snmp get -OfQ -v2c -c non-default-ro localhost \
    1.3.6.1.4.1.30155.6.1.1.0"
echo ======= $snmp_command
carp_allow="$(eval $snmp_command)"
carp_allow="${carp_allow##.iso.org.dod.internet.private.enterprises.openBSD.carpMIBObjects.carpSysctl.carpAllow.0 = }"
if [ "$carp" -ne "$carp_allow" ]
then
	echo "Retrieval test with default ro community string failed."
	FAILED=1
fi

# system.sysContact set with non-default rw/ro community strings

# Currently no set support in snmpd
#puffy="puffy@openbsd.org"
#snmp_command="snmp set -c non-default-rw -v 1 localhost system.sysContact.0 \
#   s $puffy"
#echo ======= $snmp_command
#eval $snmp_command > /dev/null 2>&1
#snmp_command="snmp get -Oqv -v2c -cnon-default-ro localhost 1.3.6.1.2.1.1.4.0"
#echo ======= $snmp_command
#contact="$(eval $snmp_command)"
#if [ "$contact" !=  "$puffy" ]
#then
#	echo "Setting with default rw community string failed."
#	FAILED=1
#fi

# custom oids, with a ro that we should not be able to set

snmp_command="snmp get -Oqv -v2c -cnon-default-rw localhost \
    1.3.6.1.4.1.30155.42.1.0"
echo ======= $snmp_command
string="$(eval $snmp_command)"
if [ "$string" !=  "humppa" ]
then
	echo "couldn't get customer oid string"
	FAILED=1
fi

snmp_command="snmp get -Oqv -v2c -c non-default-rw localhost \
    1.3.6.1.4.1.30155.42.2.0"
echo ======= $snmp_command
integer="$(eval $snmp_command)"
if [ $integer -ne  1 ]
then
	echo "Retrieval of customer oid integer failed."
	FAILED=1
fi

# Currently no set support in snmpd
#snmp_command="snmp set -c non-default-rw -v 1 localhost \
#   1.3.6.1.4.1.30155.42.1.0 s \"bula\""
#echo ======= $snmp_command
#eval $snmp_command > /dev/null 2>&1
#if [ $? -eq 0  ]
#then
#	echo "Setting of a ro custom oid test unexpectedly succeeded."
#	FAILED=1
#fi

snmp_command="snmp get -Oqv -v2c -c non-default-rw localhost \
    usmUserSecurityName.1.0"
echo ======= $snmp_command
reyk="$(eval LC_ALL=en_US.UTF-8 $snmp_command)"
if [ "$reyk" != "Reyk Fl${oe}ter" ]
then
	echo "Printing of UTF-8 string in UTF-8 locale failed"
	FAILED=1
fi
reyk="$(eval LC_ALL=C $snmp_command)"
if [ "$reyk" != "Reyk Fl.ter" ]
then
	echo "Printing of UTF-8 string in C locale failed"
	FAILED=1
fi

snmp_command="snmp get -Oqv -v2c -c non-default-rw localhost \
    usmUserSecurityName.2.0"
echo ======= $snmp_command
broken="$(eval LC_ALL=en_US.UTF-8 $snmp_command)"
if [ "$broken" != "br${replacement}ken" ]
then
	echo "Printing of UTF-8 replacement character failed"
	FAILED=1
fi
broken="$(eval LC_ALL=C $snmp_command)"
if [ "$broken" != "br?ken" ]
then
	echo "Printing of question mark in C locale failed"
	FAILED=1
fi

kill $(pgrep snmpd) >/dev/null 2>&1

case $FAILED in
0)	echo
	cleanup
	exit 0
	;;
1)	fail
	;;
esac
