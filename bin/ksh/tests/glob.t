name: glob-bad-1
description:
	Check that globbing isn't done when glob has syntax error
perl-setup:
	mkdir("[x", 0777) || die "couldn't make directory [x - $!\n";
	&touch("[x/foo");
stdin:
	echo [*
	echo *[x
	echo [x/*
expected-stdout:
	[*
	*[x
	[x/foo
---

name: glob-bad-2
description:
	Check that symbolic links aren't stat()'d
perl-setup:
	mkdir("dir", 0777) || die "couldn't make directory dir - $!\n";
	&touch("dir/abc");
	symlink("non-existent-file", "dir/abc");
stdin:
	echo d*/*
	echo d*/abc
expected-stdout:
	dir/abc
	dir/abc
---

name: glob-range-1
description:
	Test range matching
perl-setup:
	&touch(".bc", "abc", "bbc", "cbc", "-bc");
stdin:
	echo [ab-]*
	echo [-ab]*
	echo [!-ab]*
	echo [!ab]*
	echo []ab]*
expected-stdout:
	-bc abc bbc
	-bc abc bbc
	cbc
	-bc cbc
	abc bbc
---

name: glob-range-2
description:
	Test range matching
	(at&t ksh fails this; POSIX says invalid)
perl-setup:
	&touch("abc");
stdin:
	echo [a--]*
expected-stdout:
	[a--]*
---

name: glob-range-3
description:
	Check that globbing matches the right things...
perl-setup:
	&touch("a\302c");
stdin:
	echo a[Á-Ú]*
expected-stdout:
	aÂc
---

name: glob-range-4
description:
	Results unspecified according to POSIX
perl-setup:
	&touch(".bc");
stdin:
	echo [a.]*
expected-stdout:
	[a.]*
---

name: glob-range-5
description:
	Results unspecified according to POSIX
	(at&t ksh treats this like [a-cc-e]*)
perl-setup:
	&touch("abc", "bbc", "cbc", "dbc", "ebc", "-bc");
stdin:
	echo [a-c-e]*
expected-stdout:
	-bc abc bbc cbc ebc
---

