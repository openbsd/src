#!/bin/sh -
#	$OpenBSD: groups.sh,v 1.4 2001/06/20 20:50:27 pjanzen Exp $
#	Public domain.

exec /usr/bin/id -Gn $*
