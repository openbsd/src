tid="broken pipe test"

for p in 1 2; do
	trace "protocol $p"
	for i in 1 2 3 4; do
		ssh -$p -F $OBJ/ssh_config_config nexthost echo $i 2> /dev/null | true
		r=$?
		if [ $r -ne 0 ]; then
			fail "broken pipe returns $r for protocol $p"
		fi
	done
done
