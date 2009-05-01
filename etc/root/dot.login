# $OpenBSD: dot.login,v 1.12 2009/05/01 18:08:42 millert Exp $
#
# csh login file

if ( -x /usr/bin/tset ) then
	set noglob histchars=""
	onintr finish
	eval `tset -sQ '-munknown:?vt220' $TERM`
	finish:
	unset noglob histchars
	onintr
endif

if ( `logname` == `whoami` ) then
	echo "Read the afterboot(8) man page for administration advice."
endif
