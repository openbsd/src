use ExtUtils::MakeMaker;

WriteMakefile(
	      NAME => 'OS2::REXX',
	      VERSION => '0.21',
	      MAN3PODS 	=> ' ', 	# Pods will be built by installman.
	      XSPROTOARG => '-noprototypes',
);
