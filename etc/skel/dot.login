# $OpenBSD: dot.login,v 1.2 2000/05/01 20:38:41 jakob Exp $
#
# csh login file

if ( ! $?TERMCAP ) then
	tset -Q  '-mdialup:?vt100' $TERM
endif

stty	newcrt crterase

set	savehist=100
set	ignoreeof

setenv	EXINIT		'set ai sm noeb'
setenv	HOSTALIASES	 $HOME/.hostaliases

if ( -x /usr/games/fortune) /usr/games/fortune
