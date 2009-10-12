BEGIN {
	require 5.006;
}

use ExtUtils::MakeMaker;

WriteMakefile(
	NAME		=> 'Devel::DProf',
	DISTNAME	=> 'DProf',
	VERSION_FROM	=> 'DProf.pm',
	clean		=> { 'FILES' => 'tmon.out t/tmon.out t/err'},
	XSPROTOARG	=> '-noprototypes',
	dist		=> {
			     COMPRESS => 'gzip -9f',
			     SUFFIX => 'gz',
			     DIST_DEFAULT => 'all tardist',
			   },
);
