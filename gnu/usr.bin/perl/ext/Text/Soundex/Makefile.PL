use ExtUtils::MakeMaker;
WriteMakefile(
	NAME		=> "Text::Soundex",
	VERSION_FROM	=> 'Soundex.pm',
	'dist'		=> {
			     COMPRESS	=> 'gzip -9f',
	      		     SUFFIX	=> 'gz',
	      		     DIST_DEFAULT => 'all tardist',
			   },
	MAN3PODS	=> {},
);
