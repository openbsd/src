# normal close
BEGIN {
	print "hello" >"T.close.5.tmp"
	print close("T.close.5.tmp")
}
