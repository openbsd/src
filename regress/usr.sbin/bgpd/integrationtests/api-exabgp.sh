#!/bin/sh

# ignore Control C, exabgp will send a TERM
trap '' SIGINT

(sleep 10 && echo shutdown) &

while read line; do
	[ -z "$line" ] && continue
	echo "$line" >&2
done
