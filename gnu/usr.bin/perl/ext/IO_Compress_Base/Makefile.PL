#! perl -w

use strict ;
require 5.004 ;

use private::MakeUtil;
use ExtUtils::MakeMaker 5.16 ;

UpDowngrade(getPerlFiles('MANIFEST')) 
    unless $ENV{PERL_CORE};

WriteMakefile( 
    NAME         => 'IO::Compress::Base',
    VERSION_FROM => 'lib/IO/Compress/Base.pm',
    'dist'       => { COMPRESS     => 'gzip', 
                      TARFLAGS     => '-chvf',
                      SUFFIX       => 'gz',
                      DIST_DEFAULT => 'MyTrebleCheck tardist',
                    },

    (
      $ENV{SKIP_FOR_CORE}
        ? (MAN3PODS    => {})
	    : (PREREQ_PM   => { 'Scalar::Util'  => 0,
		                    $] >= 5.005 && $] < 5.006   
                                ? ('File::BSDGlob' => 0) 
                                : () }
	      )
    ),
       
    (
    $] >= 5.005
        ? (ABSTRACT_FROM => 'lib/IO/Compress/Base.pm',
            AUTHOR       => 'Paul Marquess <pmqs@cpan.org>')
        : ()
    ),

    INSTALLDIRS => ($] >= 5.009 ? 'perl' : 'site'),

    ((ExtUtils::MakeMaker->VERSION() gt '6.30') ?
        ('LICENSE'  => 'perl')         : ()),
    
) ;

# end of file Makefile.PL


