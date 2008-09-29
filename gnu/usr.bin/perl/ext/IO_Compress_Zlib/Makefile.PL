#! perl -w

use strict ;
require 5.004 ;

$::VERSION = '2.008' ;

use private::MakeUtil;
use ExtUtils::MakeMaker 5.16 ;

UpDowngrade(getPerlFiles('MANIFEST')) 
    unless $ENV{PERL_CORE};

my @MainPREREQ = qw(Compress::Raw::Zlib
                    IO::Compress::Base
                    IO::Uncompress::Base
                   );

# Bzip2 is an option prereq.                   
push @MainPREREQ, qw(IO::Compress::Bzip2 IO::Uncompress::Bunzip2)
    if eval { require IO::Compress::Bzip2; };

my %MainPREREQ = map { $_ => $::VERSION } @MainPREREQ; 


WriteMakefile( 
    NAME         => 'IO::Compress::Zlib',
    VERSION_FROM => 'lib/IO/Compress/Gzip.pm',
    'dist'       => { COMPRESS     => 'gzip', 
                      TARFLAGS     => '-chvf',
                      SUFFIX       => 'gz',
                      DIST_DEFAULT => 'MyTrebleCheck tardist',
                    },

    (
      $ENV{SKIP_FOR_CORE}
        ? (MAN3PODS    => {})
	    : (PREREQ_PM   => { %MainPREREQ, 
		                    $] >= 5.005 && $] < 5.006   
                                ? ('File::BSDGlob' => 0) 
                                : () }
	      )
    ),
       
    (
    $] >= 5.005
        ? (ABSTRACT => 'Perl interface to zlib',
            AUTHOR  => 'Paul Marquess <pmqs@cpan.org>')
        : ()
    ),

    INSTALLDIRS => ($] >= 5.009 ? 'perl' : 'site'),

    ((ExtUtils::MakeMaker->VERSION() gt '6.30') ?
        ('LICENSE'  => 'perl')         : ()),    

) ;

# end of file Makefile.PL

