# $OpenBSD: dot.login,v 1.9 2003/03/20 01:43:31 david Exp $
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
endif
