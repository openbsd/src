tset -Q \?$TERM

if ( `logname` == `whoami` ) then
	echo "Don't login as root, use su"
endif
