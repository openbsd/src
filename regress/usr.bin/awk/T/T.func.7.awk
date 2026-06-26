END { print "end" }
{ print fib($1) }
function fib(n) {
	if (n <= 1) return 1
	else return add(fib(n-1), fib(n-2))
}
function add(m,n) { return m+n }
BEGIN { print "begin" }
