#	$OpenBSD: trap.t,v 1.3 2022/10/14 23:51:16 kn Exp $

#
# Check that I/O redirection failure triggers the ERR trap.
# stderr patterns are minimal to match all of bash, ksh and ksh93.
# Try writing the root directory to guarantee EISDIR.
#

name: failed-redirect-triggers-ERR-restricted
description:
	Check that restricted mode prevents valid redirections that may write.
arguments: !-r!
stdin:
	trap 'echo ERR' ERR
	true >/dev/null
expected-stdout:
	ERR
expected-stderr-pattern:
	/restricted/
expected-exit: e != 0
---


name: failed-redirect-triggers-ERR-command
description:
	Redirect standard output for a single command.
stdin:
	trap 'echo ERR' ERR
	true >/
expected-stdout:
	ERR
expected-stderr-pattern:
	/Is a directory/
expected-exit: e != 0
---


name: failed-redirect-triggers-ERR-permanent
description:
	Permanently redirect standard output of the shell without execution.
stdin:
	trap 'echo ERR' ERR
	exec >/
expected-stdout:
	ERR
expected-stderr-pattern:
	/Is a directory/
expected-exit: e != 0
---

#
# Check that the errexit option
# a) does not interfere with running traps and
# b) propagates a non-zero exit status from traps.
# Check that traps are run in the same order in which they were triggered.
#

name: EXIT-always-runs
description:
	Check that EXIT runs under errexit even if ERR failed.
arguments: !-e!
stdin:
	trap 'echo ERR ; false' ERR
	trap 'echo EXIT' EXIT
	false
expected-stdout:
	ERR
	EXIT
expected-exit: e != 0
---


name: signal-handling-is-no-error
description:
	Check that gracefully handling a signal is not treated as error.
arguments: !-e!
stdin:
	trap 'echo ERR' ERR
	trap 'echo EXIT' EXIT
	trap 'echo USR1' USR1
	kill -USR1 $$
expected-stdout:
	USR1
	EXIT
expected-exit: e == 0
---


name: errexit-aborts-EXIT
# XXX remove once bin/ksh/main.c r1.52 is backed out
expected-fail: yes
description:
	Check that errexit makes EXIT exit early.
arguments: !-e!
stdin:
	trap 'echo ERR' ERR
	trap 'false ; echo EXIT' EXIT
expected-stdout:
	ERR
expected-exit: e != 0
---


name: EXIT-triggers-ERR
# XXX remove once bin/ksh/main.c r1.52 is backed out *AND* and a new fix is in
expected-fail: yes
description:
	Check that ERR runs under errexit if EXIT failed.
arguments: !-e!
stdin:
	trap 'echo ERR' ERR
	trap 'echo EXIT ; false' EXIT
	true
expected-stdout:
	EXIT
	ERR
expected-exit: e != 0
 ---
