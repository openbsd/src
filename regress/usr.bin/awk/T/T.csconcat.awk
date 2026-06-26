BEGIN {
	$0 = "aaa"
	print "abcdef" " " $0
}
BEGIN {
	print "hello" "world"
	print helloworld
}
BEGIN {
	print " " "hello"
	print "hello" " "
	print "hello" " " "world"
	print "hello" (" " "world")
}
