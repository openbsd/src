#	$OpenBSD: start_rdconfig.sh,v 1.2 2000/03/01 22:10:05 todd Exp $

echo rdconfig ${1} ${2}
rdconfig ${1} ${2} &
echo  $! >rd.pid 

