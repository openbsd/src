BEGIN {
	_=0
	getline <"/dev/null"
	print _
}

END {
	print _
}
