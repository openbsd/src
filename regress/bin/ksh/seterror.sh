#! /bin/sh
# $OpenBSD: seterror.sh,v 1.1 2003/02/09 18:52:49 espie Exp $

# set -e is supposed to abort the script for errors that are not caught
# otherwise.

set -e

for i in 1 2 3
do
	false && true
done
true
