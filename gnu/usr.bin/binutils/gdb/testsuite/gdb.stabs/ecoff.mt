# This configuration is for a gcc that uses mips-tfile. If your gcc
# uses gas, you should configure gdb --with-gnu-as.
#
weird.o: $(srcdir)/weird.def $(srcdir)/ecoff.sed
	sed <$(srcdir)/weird.def -e 's/.long/.word/' | \
	sed -f $(srcdir)/ecoff.sed >weird.s
	$(CC) -c weird.s
