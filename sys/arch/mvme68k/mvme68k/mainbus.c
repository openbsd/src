void mainbus_attach __P((struct device *, struct device *, void *));
int  mainbus_match __P((struct device *, void *, void *));

struct mainbus_softc {
	struct device sc_dev;
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus_", mainbus_match, mainbus_attach,
	DV_DULL, sizeof(struct mainbus_softc), 0
};

int
mainbus_match(parent, cf, args)
	struct device *parent;
	void *cf;
	void *args;
{
	return 1;
}

void
mainbus_attach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	while (config_found(self, NULL, NULL))
		;
}
