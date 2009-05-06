# $OpenBSD: dot.login,v 1.5 2009/05/06 22:02:05 millert Exp $
#
# csh login file

if ( ! $?TERMCAP ) then
	if ( $?XTERM_VERSION ) then
		tset -IQ '-munknown:?vt220' $TERM
	else
		tset -Q '-munknown:?vt220' $TERM
	endif
endif

stty	newcrt crterase

set	savehist=100
set	ignoreeof

setenv	EXINIT		'set ai sm noeb'
setenv	HOSTALIASES	 $HOME/.hostaliases

if (-x /usr/games/fortune) /usr/games/fortune
