use ExtUtils::MakeMaker;

WriteMakefile(
	      NAME => 'OS2::REXX',
	      VERSION => '0.22',
	      MAN3PODS 	=> ' ', 	# Pods will be built by installman.
	      XSPROTOARG => '-noprototypes',
	      PERL_MALLOC_OK => 1,
);
