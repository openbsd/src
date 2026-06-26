BEGIN {
	x=0
	getline <"/dev/null"
	print x
}

END {
	print x
}
