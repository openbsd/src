/*	$OpenBSD: pxapcicvar.h,v 1.2 2005/01/02 19:52:37 drahn Exp $ */
struct pxapcic_socket {
        struct pxapcic_softc *sc;
        int socket;                     /* socket number */
        struct device *pcmcia;
	/*
        struct pxapcic_tag *pcictag;
	*/

        int flags;

        int power_capability;

        void *pcictag_cookie;   /* opaque data for pcictag functions */
};

/* event */
#define PXAPCIC_EVENT_INSERTION    0
#define PXAPCIC_EVENT_REMOVAL      1

/* laststatus */
#define PXAPCIC_FLAG_CARDD 0
#define PXAPCIC_FLAG_CARDP 1

struct pxapcic_tag {
	int (*read)(struct pxapcic_socket *, int);
	void (*write)(struct pxapcic_socket *, int, int);
	void (*set_power)(struct pxapcic_socket *, int);
	void (*clear_intr)(int);
	void *(*intr_establish)(struct pxapcic_socket *, int,
	    int (*)(void *), void *);
	void (*intr_disestablish)(struct pxapcic_socket *, void *);
};


struct pxapcic_softc {
	struct device sc_dev;
	struct pxapcic_socket sc_socket[2];

        bus_space_tag_t sc_iot;
	bus_space_handle_t sc_scooph;

	struct proc *sc_event_thread;
	void *sc_irq;
	int sc_gpio;
        int sc_shutdown;
};

 
#define SCOOP_REG_MCR  0x00
#define SCOOP_REG_CDR  0x04
#define SCOOP_REG_CSR  0x08
#define SCOOP_REG_CPR  0x0C
#define SCOOP_REG_CCR  0x10
#define SCOOP_REG_IRR  0x14
#define SCOOP_REG_IRM  0x14
#define SCOOP_REG_IMR  0x18
#define SCOOP_REG_ISR  0x1C
#define SCOOP_REG_GPCR 0x20
#define SCOOP_REG_GPWR 0x24
#define SCOOP_REG_GPRR 0x28

#define SCP_CDR_DETECT	0x0002

#define SCP_CSR_READY	0x0002
#define SCP_CSR_MISSING	0x0004
#define SCP_CSR_WPROT	0x0008
#define SCP_CSR_BVD1	0x0010
#define SCP_CSR_BVD2	0x0020
#define SCP_CSR_3V	0x0040
#define SCP_CSR_PWR	0x0080

#define SCP_CPR_OFF	0x0000
#define SCP_CPR_3V	0x0001
#define SCP_CPR_5V	0x0002
#define SCP_CPR_PWR	0x0080


#define SCP_MCR_IOCARD	0x0010
#define SCP_CCR_RESET	0x0080

#define SCP_IMR_READY	0x0002
#define SCP_IMR_DETECT	0x0004
#define SCP_IMR_WRPROT	0x0008
#define SCP_IMR_STSCHG	0x0010
#define SCP_IMR_BATWARN	0x0020
#define SCP_IMR_UNKN0	0x0040
#define SCP_IMR_UNKN1	0x0080
