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
