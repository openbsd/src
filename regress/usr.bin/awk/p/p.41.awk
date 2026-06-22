NR >= 10	{ exit }
END		{ if (NR < 10)
			print "input file has only " NR " lines" }
