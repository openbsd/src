
BEGIN { @INC = grep {!/blib/} @INC }

require 5.005;
use ExtUtils::MakeMaker;
WriteMakefile
(
          'DISTNAME' => 'PathTools',
	  'NAME' => 'Cwd',
          'VERSION_FROM' => 'Cwd.pm',
          'DEFINE' => join(" ",
                "-DDOUBLE_SLASHES_SPECIAL=@{[$^O eq q(qnx) || $^O eq q(nto) ? 1 : 0]}",
                ((grep { $_ eq 'PERL_CORE=1' } @ARGV) ? '-DNO_PPPORT_H' : ()),
          ),
          'PREREQ_PM' => {
                           'Carp' => '0',
                           'File::Basename' => '0',
                           'Scalar::Util' => '0',
                           'Test' => '0',
                           # done_testing() is used in dist/Cwd/t/Spec.t
                           'Test::More' => 0.88,
                         },
          'INSTALLDIRS' => 'perl',
          'EXE_FILES' => [],
          'PL_FILES' => {}
        )
;
