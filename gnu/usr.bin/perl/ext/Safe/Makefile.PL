use ExtUtils::MakeMaker;
WriteMakefile(
    NAME	=> 'Safe',
    MAN3PODS 	=> ' ', 	# Pods will be built by installman.
    XSPROTOARG => '-noprototypes', 		# XXX remove later?
    VERSION_FROM => 'Safe.pm',
);
