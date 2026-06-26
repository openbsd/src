# normal close
BEGIN {
	print "hello" | "cat >T.close.6.tmp"
	print close("cat >T.close.6.tmp")
}
