use ExtUtils::MakeMaker;
WriteMakefile(
    NAME	=> 'FileHandle',
    MAN3PODS   => ' ',			# Pods will be built by installman.
    XSPROTOARG => '-noprototypes', 	# XXX remove later?
    VERSION_FROM => 'FileHandle.pm',
);
