BEGIN {
    chdir 't' if -d 't';
    @INC = qw(../lib uni .);
    require "case.pl";
}

casetest("Lower", \%utf8::ToSpecLower, sub { lc $_[0] });

