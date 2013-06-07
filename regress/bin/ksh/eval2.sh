#!/bin/sh
# $OpenBSD: eval2.sh,v 1.1 2013/06/07 08:48:18 espie Exp $

set -e
true && false
echo "should not print"
