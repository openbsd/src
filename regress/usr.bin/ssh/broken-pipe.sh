tid="broken pipe test"

for i in 1 2 3 4; do
        ssh -2 -F $OBJ/ssh_config nexthost echo $i | true
	r=$?
	if [ $r -ne 0 ]; then
		fail "broken pipe returns $r"
	fi
done
