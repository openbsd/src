name: eglob-bad-1
description:
	Check that globbing isn't done when glob has syntax error
perl-setup:
	&touch("abcx", "abcz", "bbc");
stdin:
	echo !([*)*
	echo +(a|b[)*
expected-stdout:
	!([*)*
	+(a|b[)*
---

name: eglob-bad-2
description:
	Check that globbing isn't done when glob has syntax error
	(at&t ksh fails this test)
perl-setup:
	&touch("abcx", "abcz", "bbc");
stdin:
	echo [a*(]*)z
expected-stdout:
	[a*(]*)z
---

name: eglob-infinite-plus
description:
	Check that shell doesn't go into infinite loop expanding +(...)
	expressions.
perl-setup:
	&touch("abc");
time-limit: 3
stdin:
	echo +()c
	echo +()x
	echo +(*)c
	echo +(*)x
expected-stdout:
	+()c
	+()x
	abc
	+(*)x
---

name: eglob-subst-1
description:
	Check that eglobbing isn't done on substitution results
perl-setup:
	&touch("abc");
stdin:
	x='@(*)'
	echo $x
expected-stdout:
	@(*)
---

name: eglob-nomatch-1
description:
	Check that the pattern doesn't match
stdin:
	echo no-file+(a|b)stuff
	echo no-file+(a*(c)|b)stuff
expected-stdout:
	no-file+(a|b)stuff
	no-file+(a*(c)|b)stuff
---

name: eglob-match-1
description:
	Check that the pattern matches correctly
perl-setup:
	&touch("abd", "acd");
stdin:
	echo a+(b|c)d
	echo a!(@(b|B))d
	echo a[b*(foo|bar)]d
expected-stdout:
	abd acd
	acd
	abd
---

name: eglob-case-1
description:
	Simple negation tests
stdin:
	case foo in !(foo|bar)) echo yes;; *) echo no;; esac
	case bar in !(foo|bar)) echo yes;; *) echo no;; esac
expected-stdout:
	no
	no
---

name: eglob-case-2
description:
	Simple kleene tests
stdin:
	case foo in *(a|b[)) echo yes;; *) echo no;; esac
	case foo in *(a|b[)|f*) echo yes;; *) echo no;; esac
	case '*(a|b[)' in *(a|b[)) echo yes;; *) echo no;; esac
expected-stdout:
	no
	yes
	yes
---

name: eglob-trim-1
description:
	Eglobing in trim expressions...
	(at&t ksh fails this - docs say # matches shortest string, ## matches
	longest...)
stdin:
	x=abcdef
	echo 1: ${x#a|abc}
	echo 2: ${x##a|abc}
	echo 3: ${x%def|f}
	echo 4: ${x%%f|def}
expected-stdout:
	1: bcdef
	2: def
	3: abcde
	4: abc
---

name: eglob-trim-2
description:
	Check eglobing works in trims...
stdin:
	x=abcdef
	echo ${x#*(a|b)cd}
	echo "${x#*(a|b)cd}"
	echo ${x#"*(a|b)cd"}
expected-stdout:
	ef
	ef
	abcdef
---

