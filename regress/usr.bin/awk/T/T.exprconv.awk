BEGIN {
	x = (1 > 0); print x
	x = (1 < 0); print x
	x = (1 == 1); print x
	print ("a" >= "b")
	print ("b" >= "a")
	print (0 == 0.0)
	# x = ((1 == 1e0) && (1 == 10e-1) && (1 == .1e2)); print x
}
