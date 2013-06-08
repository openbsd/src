#! /bin/sh
# $OpenBSD: seterror.sh,v 1.3 2013/06/08 21:36:50 millert Exp $

# set -e is supposed to abort the script for errors that are not caught
# otherwise.

set -e

if true; then false && false; fi
if true; then if true; then false && false; fi; fi

for i in 1 2 3
do
	false && true
	true || false
done

! true | false

true
