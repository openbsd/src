tid="local and remote forwarding"

base=33
last=$PORT
fwd=""
for j in 0 1 2; do
	for i in 0 1 2; do
		a=$base$j$i
		b=`expr $a + 50`
		c=$last
		# fwd chain: $a -> $b -> $c
		fwd="$fwd -L$a:127.0.0.1:$b -R$b:127.0.0.1:$c"
		last=$a
	done
done
for p in 1 2; do
	q=`expr 3 - $p`
	trace "start forwarding, fork to background"
	ssh -$p -F $OBJ/ssh_config -f $fwd somehost sleep 10

	trace "transfer over forwarded channels and check result"
	ssh -$q -F $OBJ/ssh_config -p$last -o 'ConnectionAttempts=4' \
		somehost cat /bin/ls > $OBJ/ls.copy
	test -f $OBJ/ls.copy			|| fail "failed copy /bin/ls"
	cmp /bin/ls $OBJ/ls.copy		|| fail "corrupted copy of /bin/ls"

	sleep 10
done
