{
	n = split($0, x)
	delete x[1]
	n1 = 0; for (i in x) n1++
	delete x; 
	n2 = 0; for (i in x) n2++
	print n, n1, n2
}
