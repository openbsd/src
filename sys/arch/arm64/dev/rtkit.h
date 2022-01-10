/* public domain */

struct rtkit_state;

struct rtkit_state *rtkit_init(int, const char *);
int	rtkit_boot(struct rtkit_state *);
int	rtkit_poll(struct rtkit_state *);
int	rtkit_start_endpoint(struct rtkit_state *, uint32_t,
	    void (*)(void *, uint64_t), void *);
int	rtkit_send_endpoint(struct rtkit_state *, uint32_t, uint64_t);
