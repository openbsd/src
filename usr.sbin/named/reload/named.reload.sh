#!/bin/sh -
#
#	$OpenBSD: named.reload.sh,v 1.3 1997/03/12 14:51:59 downsj Exp $
#	from named.reload	5.2 (Berkeley) 6/27/89
#	$From: named.reload.sh,v 8.1 1994/12/15 06:24:14 vixie Exp $
#

exec %DESTSBIN%/ndc reload
