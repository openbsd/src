# $OpenBSD: dot.login,v 1.14 2009/12/20 15:35:35 deraadt Exp $
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
