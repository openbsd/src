name: heredoc-1
description:
	Check ordering/content of redundent here documents.
stdin:
	cat << EOF1 << EOF2 
	hi
	EOF1
	there
	EOF2
expected-stdout:
	there
---

name: heredoc-2
description:
	Check quoted here-doc is protected.
stdin:
	a=foo
	cat << 'EOF'
	hi\
	there$a
	stuff
	EO\
	F
	EOF
expected-stdout:
	hi\
	there$a
	stuff
	EO\
	F
---

name: heredoc-3
description:
	Check that newline isn't needed after heredoc-delimiter marker.
stdin: !
	cat << EOF
	hi
	there
	EOF
expected-stdout:
	hi
	there
---

name: heredoc-4
description:
	Check that an error occurs if the heredoc-delimiter is missing.
stdin: !
	cat << EOF
	hi
	there
expected-exit: e > 0
expected-stderr-pattern: /.*/
---

name: heredoc-5
description:
	Check that backslash quotes a $, ` and \ and kills a \newline
stdin: 
	a=BAD
	b=ok
	cat << EOF
	h\${a}i
	h\\${b}i
	th\`echo not-run\`ere
	th\\`echo is-run`ere
	fol\\ks
	more\\
	last \
	line
	EOF
expected-stdout:
	h${a}i
	h\oki
	th`echo not-run`ere
	th\is-runere
	fol\ks
	more\
	last line
---

name: heredoc-6
description:
	Check that \newline in initial here-delim word doesn't imply
	a quoted here-doc.
stdin: 
	a=i
	cat << EO\
	F
	h$a
	there
	EOF
expected-stdout:
	hi
	there
---

name: heredoc-7
description:
	Check that double quoted $ expressions in here delimiters are
	not expanded and match the delimiter.
	POSIX says only quote removal is applied to the delimiter.
stdin: 
	a=b
	cat << "E$a"
	hi
	h$a
	hb
	E$a
	echo done
expected-stdout:
	hi
	h$a
	hb
	done
---

name: heredoc-8
description:
	Check that double quoted escaped $ expressions in here
	delimiters are not expanded and match the delimiter.
	POSIX says only quote removal is applied to the delimiter
	(\ counts as a quote).
stdin: 
	a=b
	cat << "E\$a"
	hi
	h$a
	h\$a
	hb
	h\b
	E$a
	echo done
expected-stdout:
	hi
	h$a
	h\$a
	hb
	h\b
	done
---

