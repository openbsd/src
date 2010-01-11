#ifndef RT_ZEBRA
#define RT_ZEBRA 1

/** rt_zebra context */
typedef struct _rt_zebra {
	int sock;
	int state;
	int write_ready:1,
	    reserved:31;
	struct event ev_sock;
	bytebuffer *buffer;
} rt_zebra;

#ifdef __cplusplus
extern "C" {
#endif

rt_zebra * rt_zebra_get_instance(void);
int   rt_zebra_init (rt_zebra *);
void  rt_zebra_fini (rt_zebra *);
int   rt_zebra_start (rt_zebra *);
void  rt_zebra_stop (rt_zebra *);
int   rt_zebra_is_running (rt_zebra *);
int   rt_zebra_add_ipv4_blackhole_rt(rt_zebra *, uint32_t, uint32_t );
int   rt_zebra_delete_ipv4_blackhole_rt(rt_zebra *, uint32_t, uint32_t );

#ifdef __cplusplus
}
#endif
#endif
