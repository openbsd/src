#!/bin/sh
# 
# $OpenBSD: gzsig.sh,v 1.3 2005/05/29 08:54:59 djm Exp $

set -e

# TODO: add ssh2

# Location of keys, etc for regress is passed in by make target usually
if [ -z "$1" ] ; then
	SRCDIR="."
else
	SRCDIR="$1"
fi

KEYS="ssh-rsa ssh2-rsa ssh2-dsa x509-dsa x509-rsa"
KEYS_ENC="ssh-rsa-pass ssh2-rsa-pass ssh2-dsa-pass x509-dsa-pass x509-rsa-pass"

# Clean up from last time.
gunzip *.gz >/dev/null 2>&1 || true

for key in ${KEYS}; do
    echo "===> $key" >&2
    for x in ${SRCDIR}/file*[0-9] ; do gzip -f < $x > `basename ${x}`-t.gz ; done
    gzsig sign -v ${SRCDIR}/$key file*-t.gz
    gzsig verify -v ${SRCDIR}/$key.pub file*-t.gz
    gzcat file*-t.gz >/dev/null
done

for key in ${KEYS_ENC}; do
    echo "===> $key" >&2
    for x in ${SRCDIR}/file*[0-9] ; do gzip -f < $x > `basename ${x}`-t.gz ; done
    # SSH2 keys require longer passwords
    if [ "$key" != "${key#ssh2}" ]; then
        pass=pass.ssh2
    else
	pass=pass.ssh
    fi
    gzsig sign -v -f ${SRCDIR}/$pass ${SRCDIR}/$key file*-t.gz
    gzsig verify -v ${SRCDIR}/$key.pub file*-t.gz
    gzcat file*-t.gz >/dev/null
done

echo -n "Generating random data..."
dd if=/dev/arandom of=masterrand bs=1024k count=2 > /dev/null 2>&1
echo "done."

for key in ${KEYS}; do
    echo "===> $key" >&2
    cp masterrand rand
    gzip -f rand
    gzsig sign -v ${SRCDIR}/$key < rand.gz > rand2.gz
    gzsig verify -v ${SRCDIR}/$key.pub < rand2.gz
    gzcat rand*.gz > /dev/null
    rm -f rand*
done

rm -f masterrand rand file*-t* *.gz
