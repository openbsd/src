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
	EOF
expected-stdout:
	hi\
	there$a
	stuff
---

name: heredoc-3
description:
	Check quoted here-documents don't have \newline processing done
	on them.
stdin:
	cat << 'EOF'
	hi\
	there
	EO\
	F
	EOF
	true
expected-stdout:
	hi\
	there
	EO\
	F
---

name: heredoc-4
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

name: heredoc-5
description:
	Check that an error occurs if the heredoc-delimiter is missing.
stdin: !
	cat << EOF
	hi
	there
expected-exit: e > 0
expected-stderr-pattern: /.*/
---

