use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    'NAME'	=> 'OS2::Process',
    VERSION_FROM=> 'Process.pm',
    MAN3PODS 	=> ' ', 	# Pods will be built by installman.
    'LIBS'	=> [''],   # e.g., '-lm' 
    'DEFINE'	=> '',     # e.g., '-DHAVE_SOMETHING' 
    'INC'	=> '',     # e.g., '-I/usr/include/other' 
    IMPORTS	=> { _16_DosSmSetTitle => 'sesmgr.DOSSMSETTITLE',
		     # _16_Win16SetTitle => 'pmshapi.93',
		   },
);
