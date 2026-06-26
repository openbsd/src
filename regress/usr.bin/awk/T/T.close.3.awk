# non-accessible file
BEGIN {
	getline <"/etc/passwd"
	print close("/etc/passwd")
}
