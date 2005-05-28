#!/bin/sh
# 
# $OpenBSD: gzsig.sh,v 1.1.1.1 2005/05/28 01:58:12 marius Exp $

set -e

# TODO: add ssh2

KEYS="ssh2-rsa ssh2-dsa x509-dsa x509-rsa"
KEYS_ENC="ssh2-rsa-pass ssh2-dsa-pass x509-dsa-pass x509-rsa-pass"

# Clean up from last time.
gunzip *.gz >/dev/null 2>&1 || true

for key in ${KEYS}; do
    echo "===> $key" >&2
    gzip -f file*
    gzsig sign -v $key *.gz
    gzsig verify -v $key.pub *.gz
    gunzip *.gz
done

for key in ${KEYS_ENC}; do
    echo "===> $key" >&2
    gzip -f file*
    # SSH2 keys require longer passwords
    if [ "$key" != "${key#ssh2}" ]; then
        pass=asdfg
    else
	pass=asdf
    fi
    gzsig sign -v -p $pass $key *.gz
    gzsig verify -v $key.pub *.gz
    gunzip *.gz
done

echo -n "Generating random data..."
dd if=/dev/urandom of=rand bs=1024k count=2 > /dev/null 2>&1
echo "done."

for key in ${KEYS}; do
    echo "===> $key" >&2
    gzip -f rand
    gzsig sign -v $key < rand.gz > rand2.gz
    gzsig verify -v $key.pub < rand2.gz
    rm -f rand2.gz
    gunzip *.gz
done

rm -f rand
