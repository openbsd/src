# $OpenBSD: dot.login,v 1.10 2003/08/19 10:13:14 deraadt Exp $
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
	echo "Don't login as root, use su"
	echo "Read the afterboot(8) man page for administration advice."
endif
