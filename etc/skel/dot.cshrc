# $OpenBSD: dot.cshrc,v 1.9 2020/01/23 20:56:11 okan Exp $
#
# csh initialization

alias df	df -k
alias du	du -k
alias f		finger
alias h		'history -r | more'
alias j		jobs -l
alias la	ls -a
alias lf	ls -FA
alias ll	ls -lsA
alias tset	'set noglob histchars=""; eval `\tset -s \!*`; unset noglob histchars'
alias z		suspend

set path = (~/bin $path /usr/games)

if ($?prompt) then
	# An interactive shell -- set some stuff up
	set filec
	set history = 1000
	set ignoreeof
	set mail = (/var/mail/$USER)
	set mch = `hostname -s`
	alias prompt 'set prompt = "$mch:q"":$cwd:t {\!} "'
	alias cd 'cd \!*; prompt'
	alias chdir 'cd \!*; prompt'
	alias popd 'popd \!*; prompt'
	alias pushd 'pushd \!*; prompt'
	cd .
	umask 22
endif
