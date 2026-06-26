function f() {
	n = 1
	exit
}

BEGIN {
	n = 0
	f()
	n = 2
}

END { print n }
