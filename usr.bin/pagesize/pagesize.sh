#!/bin/sh -
#	$OpenBSD: pagesize.sh,v 1.3 2001/08/06 23:35:21 millert Exp $
#	Public domain.

exec /sbin/sysctl -n hw.pagesize
