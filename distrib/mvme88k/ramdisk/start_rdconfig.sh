#
#	$OpenBSD: start_rdconfig.sh,v 1.1 1998/12/17 02:16:33 smurph Exp $
echo rdconfig ${1} ${2}
rdconfig ${1} ${2} &
echo  $! >rd.pid 

