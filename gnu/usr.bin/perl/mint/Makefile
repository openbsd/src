# IMPORTANT:  This Makefile is not intended to build Perl itself but
#             only to replace a broken pwd command!

all:	pwd

pwd:	pwd.c
	$(CC) -O3 -o pwd pwd.c

install: pwd
	(new_pwd=`which pwd` && cp -f $$new_pwd $$new_pwd.broken \
		&& cp -f pwd $$new_pwd) 

clean:
	rm -f pwd.o pwd

