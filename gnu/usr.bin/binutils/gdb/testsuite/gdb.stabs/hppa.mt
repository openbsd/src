weird.o: $(srcdir)/weird.def $(srcdir)/hppa.sed
	sed -f $(srcdir)/hppa.sed <$(srcdir)/weird.def >weird.s
	if $(CC) -c weird.s 2>errs; then \
	  true; \
	else \
	  if grep 'Directive name not recognized - STABS' errs \
	      >/dev/null; then \
	    echo HP assembler in use--skipping stabs tests; true; \
	  else \
	    cat errs; false; \
	  fi; \
	fi
