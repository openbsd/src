# $OpenBSD: dot.cshrc,v 1.12 2004/05/10 16:04:07 peter Exp $
#
# csh initialization

umask 022
alias mail Mail
set history=1000
set path=(/sbin /usr/sbin /bin /usr/bin /usr/X11R6/bin /usr/local/sbin /usr/local/bin)
set filec

# directory stuff: cdpath/cd/back
set cdpath=(/sys /sys/arch /usr/src/{bin,sbin,usr.{bin,sbin},pgrm,lib,libexec,share,contrib,local,devel,games,old,gnu,gnu/{lib,usr.bin,usr.sbin,libexec}})

setenv BLOCKSIZE 1k

alias	cd	'set old="$cwd"; chdir \!*'
alias	h	history
alias	j	jobs -l
alias	ll	ls -l
alias	l	ls -alF
alias	back	'set back="$old"; set old="$cwd"; cd "$back"; unset back; dirs'

alias	z	suspend
alias	x	exit
alias	pd	pushd
alias	pd2	pushd +2
alias	pd3	pushd +3
alias	pd4	pushd +4

if ($?prompt) then
	set prompt="`hostname -s`# "
endif
