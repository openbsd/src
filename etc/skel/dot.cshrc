# $OpenBSD: dot.cshrc,v 1.5 2005/02/16 06:56:57 matthieu Exp $
#
# csh initialization

alias df	df -k
alias du	du -k
alias f		finger
alias h		'history -r | more'
alias j		jobs -l
alias la	ls -a
alias lf	ls -FA
alias ll	ls -lgsA
alias tset	'set noglob histchars=""; eval `\tset -s \!*`; unset noglob histchars'
alias z		suspend

set path = ( ~/bin /bin /sbin /usr/{bin,sbin,X11R6/bin,local/bin,local/sbin,games} .)

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
