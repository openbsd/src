name: strsub-basic
description:
	Valid string substitutions
stdin:
	echo empty ${v/old/new}
	v=old
	echo empty ${v/old/}
	echo ${v/new/}
	v='old new'
	echo ${v/old/new}
	v='new old'
	echo ${v/old/new}
	echo "${v/old/new}"
expected-stdout:
	empty
	empty
	old
	new new
	new new
	new new
---

name: strsub-multiline
description:
	Value spanning multiple lines
stdin:
	v=`cat <<!
	bsd
	bsd.rd
	bsd.sp
	!`
	echo ${v/bsd.rd/}
expected-stdout:
	bsd bsd.sp
---

name: strsub-global
description:
	Replace all occurrences
stdin:
	v='old new old'
	echo ${v//old/new}
	v='w h i t e s p a c e'
	echo ${v//[[:blank:]]/}
	v='/usr/src'
	echo ${v//\////}
expected-stdout:
	new new new
	whitespace
	//usr//src
---

name: strsub-nested
description:
	Nested substitutions
stdin:
	v=old
	echo ${u:-${v/old/new}}
	v='old new'
	echo ${v/old/${v/old/new}}
expected-stdout:
	new
	new new new
---

name: strsub-longest
description:
	Favor the first longest match
stdin:
	v='old/old'
	echo ${v/old?(\/)/new }
	echo ${v/o*/new}
	echo ${v//old?(\/)/new}
	echo ${v//?(\/)old//}
expected-stdout:
	new old
	new
	newnew
	//
---

name: strsub-replacement-1
description:
	Variables are expanded
stdin:
	v=old
	r=new
	echo ${v/old/$r}
expected-stdout:
	new
---

name: strsub-replacement-2
description:
	The replacement is not treated as magic
stdin:
	v=old
	echo ${v/old/new*}
expected-stdout:
	new*
---

name: strsub-missing-pattern
description:
	A pattern is not required
stdin:
	v=old
	echo ${v/}
expected-stdout:
	old
---

name: strsub-nounset
description:
	Respect nounset
stdin:
	set -u
	echo ${v/old/new}
expected-stderr-pattern:
	/v: parameter not set/
expected-exit: 1
---

name: strsub-posix
description:
	Respect POSIX
env-setup: !POSIXLY_CORRECT=!
stdin:
	echo ${v/old/new}
expected-stderr-pattern:
	/bad substitution/
expected-exit: 1
---
