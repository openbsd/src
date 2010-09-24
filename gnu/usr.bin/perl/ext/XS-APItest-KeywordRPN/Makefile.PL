{ use 5.006; }
use warnings;
use strict;

use ExtUtils::MakeMaker;

WriteMakefile(
    NAME => "XS::APItest::KeywordRPN",
    VERSION_FROM => "KeywordRPN.pm",
    PREREQ_PM => {},
    ABSTRACT_FROM => "KeywordRPN.pm",
    AUTHOR => "Andrew Main (Zefram) <zefram\@fysh.org>",
);

sub MY::install { "install ::\n" }

1;
