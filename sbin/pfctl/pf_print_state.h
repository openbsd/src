int	unmask (struct pf_addr *, u_int8_t);
void	print_addr (struct pf_addr_wrap *, struct pf_addr *, u_int8_t);
void	print_host (struct pf_state_host *, u_int8_t, int);
void	print_seq (struct pf_state_peer *);
void	print_state(struct pf_state *s, int verbose);
struct hostent *getpfhostname(const char *addr_str);
