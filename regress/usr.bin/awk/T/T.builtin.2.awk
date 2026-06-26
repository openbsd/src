BEGIN {
	pi = 2 * atan2(1, 0)
	printf("%.5f %.3f %.3f %.5f %.3f\n",
		pi, sin(pi), cos(pi/2), exp(log(pi)), log(exp(10)))
}
