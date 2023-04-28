#!/bin/sh

for i in ${1}/*.test; do
	echo $(basename ${i})
	tstdir=${1} sh ${i}
	if [ "$?" -eq "0" ]; then
		echo OK
	else
		echo FAIL
	fi
done
