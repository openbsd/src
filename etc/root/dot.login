set noglob
eval `tset -s -Q ?$TERM`
unset noglob

if ( `logname` == `whoami` ) then
	echo "Don't login as root, use su"
endif
