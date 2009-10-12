#! perl -w

use strict ;
require 5.004 ;

$::VERSION = '2.020' ;

use private::MakeUtil;
use ExtUtils::MakeMaker 5.16 ;

UpDowngrade(getPerlFiles('MANIFEST'))
    unless $ENV{PERL_CORE};

WriteMakefile( 
    NAME         => 'IO::Compress',
    VERSION_FROM => 'lib/IO/Compress/Base.pm',
    'dist'       => { COMPRESS     => 'gzip', 
                      TARFLAGS     => '-chvf',
                      SUFFIX       => 'gz',
                      DIST_DEFAULT => 'MyTrebleCheck tardist',
                    },

    (
      $ENV{SKIP_FOR_CORE}
        ? ()
	    : (PREREQ_PM   => { 'Compress::Raw::Bzip2' => $::VERSION,
		                    'Compress::Raw::Zlib'   => $::VERSION,
		                    $] >= 5.005 && $] < 5.006   
                                ? ('File::BSDGlob' => 0) 
                                : () }
	      )
    ),
       
    (
    $] >= 5.005
        ? (ABSTRACT => 'IO Interface to compressed data files/buffers',
            AUTHOR  => 'Paul Marquess <pmqs@cpan.org>')
        : ()
    ),

    INSTALLDIRS => ($] >= 5.009 ? 'perl' : 'site'),
    
    (
    $] >= 5.009 && ! $ENV{PERL_CORE}
        ? (INST_LIB    => 'blib/arch')
        : ()
    ),

    ((ExtUtils::MakeMaker->VERSION() gt '6.30') ?
        ('LICENSE'  => 'perl')         : ()),    

) ;

# end of file Makefile.PL

