use ExtUtils::MakeMaker;
WriteMakefile(
    NAME	=> 'POSIX',
    LIBS	=> ["-lm -lposix -lcposix"],
    MAN3PODS 	=> ' ', 	# Pods will be built by installman.
    XSPROTOARG => '-noprototypes', 		# XXX remove later?
    VERSION_FROM => 'POSIX.pm', 
);
