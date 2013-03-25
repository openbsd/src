#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require "./test.pl";
}

print "1..5\n";

my $j = 1;
for $i ( 1,2,5,4,3 ) {
    $file = mkfiles($i);
    open(FH, "> $file") || die "can't create $file: $!";
    print FH "not ok " . $j++ . "\n";
    close(FH) || die "Can't close $file: $!";
}


{
    local *ARGV;
    local $^I = '.bak';
    local $_;
    @ARGV = mkfiles(1..3);
    $n = 0;
    while (<>) {
	print STDOUT "# initial \@ARGV: [@ARGV]\n";
	if ($n++ == 2) {
	    other();
	}
	show();
    }
}

$^I = undef;
@ARGV = mkfiles(1..3);
$n = 0;
while (<>) {
    print STDOUT "#final \@ARGV: [@ARGV]\n";
    if ($n++ == 2) {
	other();
    }
    show();
}

sub show {
    #warn "$ARGV: $_";
    s/^not //;
    print;
}

sub other {
    no warnings 'once';
    print STDOUT "# Calling other\n";
    local *ARGV;
    local *ARGVOUT;
    local $_;
    @ARGV = mkfiles(5, 4);
    while (<>) {
	print STDOUT "# inner \@ARGV: [@ARGV]\n";
	show();
    }
}

my @files;
sub mkfiles {
    foreach (@_) {
	$files[$_] ||= tempfile();
    }
    my @results = @files[@_];
    return wantarray ? @results : @results[-1];
}

END { unlink_all map { ($_, "$_.bak") } mkfiles(1..5) }
