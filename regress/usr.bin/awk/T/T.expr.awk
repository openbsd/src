BEGIN {
	FS = "\t"
}
NF == 0 || $1 ~ /^#/ {
	next
}
$1 ~ /try/ {	# new test
	nt++
	sub(/try /, "")
	prog = $0
	printf("%3d  %s\n", nt, prog)
	prog = sprintf("%s -F\"\\t\" '%s'", awk, prog)
	# print "prog is", prog
	nt2 = 0
	while (getline > 0) {
		if (NF == 0)	# blank line terminates a sequence
			break
		input = $1
		for (i = 2; i < NF; i++)	# input data
			input = input "\t" $i
		test = sprintf("echo -E '%s' | %s >T.expr.1.out; ",
			input, prog)
		if ($NF == "\"\"")
			output = ">T.expr.2.out;"
		else
			output = sprintf("echo -E '%s' >T.expr.2.out; ", $NF)
		gsub(/\\t/, "\t", output)
		gsub(/\\n/, "\n", output)
		run = "cmp -s T.expr.1.out T.expr.2.out"
		nt2++
		# print  "input is", input
		# print  "test is", test
		# print  "output is", output
		# print  "run is", run
		if (system(test output run) != 0) {
			printf("test %d.%d failed\n", nt, nt2)
			ret = 1
		}
	}
	tt += nt2
}
END {
	print tt, "tests"
	exit ret
}
