# This configuration is for a gcc that uses mips-tfile. If your gcc
# uses gas, you should configure gdb --with-gnu-as.
#
weird.o: $(srcdir)/weird.def $(srcdir)/ecoff.sed
	sed -f $(srcdir)/ecoff.sed <$(srcdir)/weird.def >weird.s
	$(CC) $(CFLAGS) -c weird.s
