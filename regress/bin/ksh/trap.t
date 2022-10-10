#	$OpenBSD: trap.t,v 1.1 2022/10/10 14:57:48 kn Exp $

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
