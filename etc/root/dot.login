set tterm='?'$TERM 
eval `set noglob ; tset -s -Q $tterm ; unset noglob`
unset tterm

if ( `logname` == `whoami` ) then
	echo "Don't login as root, use su"
endif
