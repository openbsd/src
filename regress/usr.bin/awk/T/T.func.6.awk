function ack(m,n) {
	k = k+1
	if (m == 0) return n+1
	if (n == 0) return ack(m-1, 1)
	return ack(m-1, ack(m, n-1))
}
{ k = 0; print ack($1,$2), "(" k " calls)" }
