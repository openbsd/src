use ExtUtils::MakeMaker;
use Config;

# The existence of the ./sdbm/Makefile.PL file causes MakeMaker
# to automatically include Makefile code for the targets
#	config, all, clean, realclean and sdbm/Makefile
# which perform the corresponding actions in the subdirectory.

# Work around: Darwin ppc64 bug ignores archive contents when building bundles

$define = ($^O eq 'MSWin32') ? '-DMSDOS' : '';
if ($^O eq 'MSWin32') { $myextlib = 'sdbm\\libsdbm$(LIB_EXT)'; }
elsif ($^O eq 'darwin' && $Config{'ldflags'} =~/-arch ppc64/) {
                        $myextlib = '-all_load sdbm/libsdbm$(LIB_EXT)'; }
else                  { $myextlib = 'sdbm/libsdbm$(LIB_EXT)';  }

WriteMakefile(
              NAME	=> 'SDBM_File',
              MYEXTLIB => $myextlib,
              XSPROTOARG => '-noprototypes', 		# XXX remove later?
              VERSION_FROM => 'SDBM_File.pm',
              DEFINE => $define,
             );

# We don't want the default subdir rule, as it creates a race condition with the
# rule we add below.
sub MY::subdir_x {
    return '';
}

sub MY::postamble {
  if ($^O =~ /MSWin32/ && !defined($ENV{SYSTEMROOT})) {
	if ($Config{'make'} =~ /dmake/i) {
	    # dmake-specific
	    return <<'EOT';
$(MYEXTLIB): sdbm/Makefile
@[
	cd sdbm
	$(MAKE) all
	cd ..
]
EOT
	} elsif ($Config{'make'} =~ /nmake/i) {
	    #
	    return <<'EOT';
$(MYEXTLIB): sdbm/Makefile
	cd sdbm
	$(MAKE) all
	cd ..
EOT
	} 
} elsif ($^O ne 'VMS') {
    '
$(MYEXTLIB): sdbm/Makefile
	cd sdbm && $(MAKE) all
';
  }
  else {
    '   
$(MYEXTLIB) : [.sdbm]descrip.mms
	set def [.sdbm]
    $(MMS) all
    set def [-]
';  
  }
}
