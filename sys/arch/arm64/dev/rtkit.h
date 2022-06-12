/* public domain */

struct rtkit_state;

struct rtkit {
	void *rk_cookie;
	bus_dma_tag_t rk_dmat;
	int (*rk_map)(void *, bus_addr_t, bus_size_t);
};

struct rtkit_state *rtkit_init(int, const char *, struct rtkit *);
int	rtkit_boot(struct rtkit_state *);
int	rtkit_poll(struct rtkit_state *);
int	rtkit_start_endpoint(struct rtkit_state *, uint32_t,
	    void (*)(void *, uint64_t), void *);
int	rtkit_send_endpoint(struct rtkit_state *, uint32_t, uint64_t);

int	aplsart_map(uint32_t, bus_addr_t, bus_size_t);
