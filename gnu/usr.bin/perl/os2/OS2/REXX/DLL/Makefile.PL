use ExtUtils::MakeMaker;

WriteMakefile(
	      NAME => 'OS2::DLL',
	      VERSION => '0.01',
	      MAN3PODS 	=> ' ', 	# Pods will be built by installman.
	      XSPROTOARG => '-noprototypes',
	      PERL_MALLOC_OK => 1,
);
