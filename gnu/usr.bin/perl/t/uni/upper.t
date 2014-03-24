BEGIN {
    chdir 't' if -d 't';
    @INC = qw(../lib uni .);
    require "case.pl";
}

is(uc("\x{3B1}\x{345}\x{301}"), "\x{391}\x{301}\x{399}", 'Verify moves YPOGEGRAMMENI');

casetest( 1,	# extra tests already run
	"Uppercase_Mapping",
	 sub { uc $_[0] },
	 sub { my $a = ""; uc ($_[0] . $a) });
