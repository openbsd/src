#!/bin/sh
# $OpenBSD: eval4.sh,v 1.1 2013/06/09 13:27:27 millert Exp $

set -e
true && true && false
echo "should not print"
