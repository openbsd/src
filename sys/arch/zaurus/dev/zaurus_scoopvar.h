struct scoop_softc {
	struct device sc_dev;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
};

extern	void scoop_backlight_on(int);

extern	int scoop_gpio_pin_read(struct scoop_softc *sc, int);
extern	void scoop_gpio_pin_write(struct scoop_softc *sc, int, int);
