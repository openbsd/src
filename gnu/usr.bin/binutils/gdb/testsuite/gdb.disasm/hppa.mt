EXECUTABLES = hppa
hppa: hppa.s
	if $(CC) -c $(srcdir)/hppa.s 2>errs; then \
	  $(CC) $(CFLAGS) $(LDFLAGS) -o hppa hppa.o $(LIBS); \
	  true; \
	else \
	  if grep 'Opcode not defined - DIAG' errs \
	      >/dev/null; then \
	    echo HP assembler in use--skipping disasm tests; true; \
	  else \
	    cat errs; false; \
	  fi; \
	fi

