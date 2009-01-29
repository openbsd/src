#! /bin/sh
# $OpenBSD: seterror.sh,v 1.2 2009/01/29 23:27:26 jaredy Exp $

# set -e is supposed to abort the script for errors that are not caught
# otherwise.

set -e

if true; then false && false; fi
if true; then if true; then false && false; fi; fi

for i in 1 2 3
do
	true && false
	false || false
done

! true | false

true
