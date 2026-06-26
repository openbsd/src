BEGIN {
	s = srand(1)	# set a real random start
	for (i = 1; i <= 10; i++)
		print rand() >"T.builtin.3.1.out"
	srand(s)	# reset it
	for (i = 1; i <= 10; i++)
		print rand() >"T.builtin.3.2.out"
}
