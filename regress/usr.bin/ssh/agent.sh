tid="simple agent test"

SSH_AUTH_SOCK=/nonexistant ssh-add -l > /dev/null 2>&1
if [ $? -ne 2 ]; then
	fail "ssh-add -l did not fail with exit code 2"
fi

trace "start agent"
eval `ssh-agent -s` > /dev/null
r=$?
if [ $r -ne 0 ]; then
	fail "could not start ssh-agent: exit code $r"
else
	ssh-add -l > /dev/null 2>&1
	if [ $? -ne 1 ]; then
		fail "ssh-add -l did not fail with exit code 1"
	fi
	trace "overwrite authorized keys"
	echo -n > $OBJ/authorized_keys_$USER
	for t in rsa rsa1; do
		# generate user key for agent
		rm -f $OBJ/$t-agent
		ssh-keygen -q -N '' -t $t -f $OBJ/$t-agent ||\
			 fail "ssh-keygen for $t-agent failed"
		# add to authorized keys
		cat $OBJ/$t-agent.pub >> $OBJ/authorized_keys_$USER
		# add privat key to agent
		ssh-add $OBJ/$t-agent > /dev/null 2>&1
		if [ $? -ne 0 ]; then
			fail "ssh-add did succeed exit code 0"
		fi
	done
	ssh-add -l > /dev/null 2>&1
	if [ $? -ne 0 ]; then
		fail "ssh-add -l did succeed exit code 0"
	fi

	trace  "simple connect via agent"
	for p in 1 2; do
		ssh -o "Protocol=$p" -F $OBJ/ssh_config somehost exit 5$p
		if [ $? -ne 5$p ]; then
			fail "ssh connect with protocol $p failed"
		fi
	done

	trace "kill agent"
	ssh-agent -k > /dev/null
fi
