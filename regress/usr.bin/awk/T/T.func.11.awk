BEGIN {
	n = 10
	for (i = 1; i <= n; i++)
		for (j = 1; j <= n; j++)
			x[i,j] = n * i + j
	for (i = 1; i <= n; i++)
		for (j = 1; j <= n; j++)
			if ((i,j) in x)
				k++
	print (k == n^2)
}
