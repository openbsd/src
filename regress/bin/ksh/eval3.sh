#!/bin/sh
# $OpenBSD: eval3.sh,v 1.1 2013/06/07 08:48:18 espie Exp $

set -e
false || false
echo "should not print"
