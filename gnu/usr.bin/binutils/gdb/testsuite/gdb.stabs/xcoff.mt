weird.o: $(srcdir)/weird.def $(srcdir)/xcoff.sed
	sed -f $(srcdir)/xcoff.sed <$(srcdir)/weird.def >weird.s
	$(CC) -c weird.s
