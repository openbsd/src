weird.o: $(srcdir)/weird.def $(srcdir)/aout.sed
	sed -f $(srcdir)/aout.sed <$(srcdir)/weird.def >weird.s
	$(CC) -c weird.s
