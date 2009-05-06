# $OpenBSD: dot.login,v 1.13 2009/05/06 22:02:05 millert Exp $
#
# csh login file

if ( -x /usr/bin/tset ) then
	set noglob histchars=""
	onintr finish
	if ( $?XTERM_VERSION ) then
		eval `tset -IsQ '-munknown:?vt220' $TERM`
	else
		eval `tset -sQ '-munknown:?vt220' $TERM`
	endif
	finish:
	unset noglob histchars
	onintr
endif

if ( `logname` == `whoami` ) then
	echo "Read the afterboot(8) man page for administration advice."
endif
