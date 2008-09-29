use ExtUtils::MakeMaker;

use strict;

WriteMakefile(
    'NAME'		=> 'Math::BigInt::FastCalc',
    'VERSION_FROM'	=> 'FastCalc.pm',
    'PREREQ_PM'		=> {
	'Math::BigInt' => 1.88,
    },
    INSTALLDIRS		=> 'perl',
    PREREQ_FATAL	=> 1,
    MAN3PODS		=> {},
 );
