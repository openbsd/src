EXECUTABLES = sh3
sh3: sh3.s
	$(CC) -c $(srcdir)/sh3.s
	$(CC) -o sh3 sh3.o

