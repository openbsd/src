# $OpenBSD: dot.login,v 1.11 2005/03/30 19:50:07 deraadt Exp $
#
# csh login file

set tterm='?'$TERM
set noglob
onintr finish
eval `tset -s -Q $tterm`
finish:
unset noglob
unset tterm
onintr

if ( `logname` == `whoami` ) then
	echo "Read the afterboot(8) man page for administration advice."
endif
