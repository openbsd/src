#!/bin/sh
#
# Copyright (c) 2015 Ingo Schwarze <schwarze@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

test_ps()
{
	args=$1
	ps_vars=$2
	ps_args=$3
	expected=$4
	if [ "X$args" = "X${args%=*}" ]; then
		./shortsleep $args &
	else
		env -i $args ./shortsleep &
	fi
	pid=$!

	# Give the forked process some time to set up its process name.
	ps > /dev/null

	result=`env $ps_vars ps -p $pid $ps_args | tail -n +2`
	kill $pid
	if [ "$result" != "$expected" ]; then
		echo "$args & $ps_vars ps $ps_args"
		echo "expected: >$expected<"
		echo "result:   >$result<"
		exit 1;
	fi
}

mypid=`printf %5d $$`

# not in the last column, limited width
test_ps "" "" "-o command,ppid" "./shortsleep     $mypid"
test_ps "" "" "-c -o command,ppid" "shortsleep       $mypid"
test_ps "E=1" "" "-eo command,ppid" "E=1 (shortsleep) $mypid"
test_ps "E=1" "" "-ceo command,ppid" "E=1 shortsleep   $mypid"
test_ps "long_argument" "" "-o command,ppid" "./shortsleep lon $mypid"
test_ps "long_argument" "" "-co command,ppid" "shortsleep       $mypid"
test_ps "E=long" "" "-eo command,ppid" "E=long (shortsle $mypid"
test_ps "E=long" "" "-ceo command,ppid" "E=long shortslee $mypid"
test_ps "E=1 L=very_long_var" "" "-eo command,ppid" "E=1 L=very_long_ $mypid"
test_ps "E=1 L=very_long_var" "" "-ceo command,ppid" "E=1 L=very_long_ $mypid"

# not in the last column, unlimited width
test_ps "" "" "-wwo command,ppid" "./shortsleep     $mypid"
test_ps "" "" "-cwwo command,ppid" "shortsleep       $mypid"
test_ps "E=1" "" "-ewwo command,ppid" "E=1 (shortsleep) $mypid"
test_ps "E=1" "" "-cewwo command,ppid" "E=1 shortsleep   $mypid"
test_ps "long_argument" "" "-wwo command,ppid" "./shortsleep lon $mypid"
test_ps "long_argument" "" "-cwwo command,ppid" "shortsleep       $mypid"
test_ps "E=long" "" "-ewwo command,ppid" "E=long (shortsle $mypid"
test_ps "E=long" "" "-cewwo command,ppid" "E=long shortslee $mypid"
test_ps "E=1 L=very_long_var" "" "-ewwo command,ppid" \
	"E=1 L=very_long_ $mypid"
test_ps "E=1 L=very_long_var" "" "-cewwo command,ppid" \
	"E=1 L=very_long_ $mypid"

# in the last column, limited width
test_ps "" "" "-o command" "./shortsleep"
test_ps "" "" "-co command" "shortsleep"
test_ps "" "COLUMNS=4" "-o command" "./sh"
test_ps "" "COLUMNS=4" "-co command" "shor"
test_ps "" "COLUMNS=10" "-o ppid,command" "$mypid ./sh"
test_ps "" "COLUMNS=10" "-co ppid,command" "$mypid shor"
test_ps "" "COLUMNS=4" "-o ppid,command" "$mypid ./shortsleep"
test_ps "" "COLUMNS=4" "-co ppid,command" "$mypid shortsleep"
test_ps "long_arg" "COLUMNS=4" "-o ppid,command" "$mypid ./shortsleep lon"
test_ps "long_arg" "COLUMNS=4" "-co ppid,command" "$mypid shortsleep"
test_ps "E=1" "" "-eo command" "E=1 (shortsleep)"
test_ps "E=1" "" "-ceo command" "E=1 shortsleep"
test_ps "E=1" "COLUMNS=6" "-eo command" "E=1 (s"
test_ps "E=1" "COLUMNS=5" "-eo command" "E=1 ("
test_ps "E=1" "COLUMNS=4" "-eo command" "E=1 "
test_ps "E=1" "COLUMNS=3" "-eo command" "E=1"
test_ps "E=1" "COLUMNS=2" "-eo command" "E="
test_ps "E=1" "COLUMNS=5" "-ceo command" "E=1 s"
test_ps "E=1" "COLUMNS=4" "-ceo command" "E=1 "
test_ps "E=1" "COLUMNS=3" "-ceo command" "E=1"
test_ps "E=1" "COLUMNS=2" "-ceo command" "E="

# in the last column, unlimited width
test_ps "" "" "-wwo command" "./shortsleep"
test_ps "" "" "-cwwo command" "shortsleep"
test_ps "long_argument" "" "-wwo command" "./shortsleep long_argument"
test_ps "long_argument" "" "-cwwo command" "shortsleep"
test_ps "E=1" "" "-ewwo command" "E=1 (shortsleep)"
test_ps "E=1" "" "-cewwo command" "E=1 shortsleep"
test_ps "E=1 L=very_long_var" "" "-ewwo command" \
	"E=1 L=very_long_var (shortsleep)"
test_ps "E=1 L=very_long_var" "" "-cewwo command" \
	"E=1 L=very_long_var shortsleep"

exit 0
