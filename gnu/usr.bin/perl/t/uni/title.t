BEGIN {
    chdir 't' if -d 't';
    @INC = qw(../lib uni .);
    require "case.pl";
}

casetest("Title", \%utf8::ToSpecTitle, sub { ucfirst $_[0] },
	 sub { my $a = ""; ucfirst ($_[0] . $a) });
