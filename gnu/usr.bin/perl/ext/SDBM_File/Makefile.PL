use ExtUtils::MakeMaker;

# The existence of the ./sdbm/Makefile.PL file causes MakeMaker
# to automatically include Makefile code for the targets
#	config, all, clean, realclean and sdbm/Makefile
# which perform the corresponding actions in the subdirectory.

WriteMakefile(
    NAME	=> 'SDBM_File',
    MYEXTLIB => 'sdbm/libsdbm$(LIB_EXT)',
    MAN3PODS 	=> ' ', 	# Pods will be built by installman.
    XSPROTOARG => '-noprototypes', 		# XXX remove later?
    VERSION_FROM => 'SDBM_File.pm',
);


sub MY::postamble {
    '
$(MYEXTLIB): sdbm/Makefile
	cd sdbm; $(MAKE) all
';
}

