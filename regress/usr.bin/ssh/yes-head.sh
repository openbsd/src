tid="yes|head"

for p in 1 2; do
	lines=`ssh -$p -F $OBJ/ssh_config thishost 'yes | head -2000' | (sleep 3 ; wc -l)`
	if [ $? -ne 0 ]; then
		fail "yes|head test failed"
		lines = 0;
	fi
	if [ $lines -ne 2000 ]; then
		fail "yes|head returns $lines lines instead of 2000"
	fi
done
