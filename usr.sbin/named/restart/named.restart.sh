#!/bin/sh -
#
#	$OpenBSD: named.restart.sh,v 1.2 1997/03/12 10:42:53 downsj Exp $
#	from named.restart	5.4 (Berkeley) 6/27/89
#	$Id: named.restart.sh,v 1.2 1997/03/12 10:42:53 downsj Exp $
#

exec %DESTSBIN%/%INDOT%ndc restart
