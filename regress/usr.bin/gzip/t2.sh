#!/bin/sh
# $OpenBSD: t2.sh,v 1.1 2008/08/20 09:29:51 mpf Exp $

# test basic gzip functionality

gzip -c /etc/rc > t1.gz
if ! gzip -vt t1.gz; then
	echo "=> ERROR: could not gzip"
	exit 1
else
	echo "=> OK"
fi

if ! gunzip -c t1.gz | cmp -s - /etc/rc; then
	echo "=> ERROR: uncompressed file does not match"
	gunzip -c t1.gz | diff - /etc/rc
	exit 1
else
	echo "=> OK"
fi

gzip -c /etc/rc /etc/motd > t1.gz
if ! gzip -vt t1.gz; then
	echo "=> ERROR: could not gzip multiple files"
	exit 1
else
	echo "=> OK"
fi

cat /etc/rc /etc/motd > rcmotd.test
if ! gunzip -c t1.gz | cmp -s - rcmotd.test; then
	echo "=> ERROR: gunzipped files do not match"
	gunzip -c t1.gz | diff - rcmotd.test
	exit 1
else
	echo "=> OK"
fi

exit 0
