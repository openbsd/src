#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    $ENV{PERL5LIB} = '../lib';
    require Config; import Config;
    require './test.pl';
}

use File::Path;
use File::Spec::Functions;

$| = 1;

my $Is_MacOS   = $^O eq 'MacOS';
my $tmpfile = "tmp0000";
1 while -e ++$tmpfile;
END {  if ($tmpfile) { 1 while unlink $tmpfile} }

my @prgs = () ;
my @w_files = () ;

if (@ARGV)
  { print "ARGV = [@ARGV]\n" ;
    if ($^O eq 'MacOS') {
      @w_files = map { s#^#:lib:warnings:#; $_ } @ARGV
    } else {
      @w_files = map { s#^#./lib/warnings/#; $_ } @ARGV
    }
  }
else
  { @w_files = sort glob(catfile(curdir(), "lib", "warnings", "*")) }

my $files = 0;
foreach my $file (@w_files) {

    next if $file =~ /(~|\.orig|,v)$/;
    next if $file =~ /perlio$/ && !(find PerlIO::Layer 'perlio');
    next if -d $file;

    open F, "<$file" or die "Cannot open $file: $!\n" ;
    my $line = 0;
    while (<F>) {
        $line++;
	last if /^__END__/ ;
    }

    {
        local $/ = undef;
        $files++;
        @prgs = (@prgs, $file, split "\n########\n", <F>) ;
    }
    close F ;
}

undef $/;

plan tests => (scalar(@prgs)-$files);



for (@prgs){
    unless (/\n/)
     {
      print "# From $_\n";
      next;
     }
    my $switch = "";
    my @temps = () ;
    my @temp_path = () ;
    if (s/^\s*-\w+//){
        $switch = $&;
    }
    my($prog,$expected) = split(/\nEXPECT\n/, $_);
    my ($todo, $todo_reason);
    $todo = $prog =~ s/^#\s*TODO(.*)\n//m and $todo_reason = $1;
    if ( $prog =~ /--FILE--/) {
        my(@files) = split(/\n--FILE--\s*([^\s\n]*)\s*\n/, $prog) ;
	shift @files ;
	die "Internal error test $test didn't split into pairs, got " .
		scalar(@files) . "[" . join("%%%%", @files) ."]\n"
	    if @files % 2 ;
	while (@files > 2) {
	    my $filename = shift @files ;
	    my $code = shift @files ;
    	    push @temps, $filename ;
    	    if ($filename =~ m#(.*)/#) {
                mkpath($1);
                push(@temp_path, $1);
    	    }
	    open F, ">$filename" or die "Cannot open $filename: $!\n" ;
	    print F $code ;
	    close F or die "Cannot close $filename: $!\n";
	}
	shift @files ;
	$prog = shift @files ;
    }

    # fix up some paths
    if ($^O eq 'MacOS') {
	$prog =~ s|require "./abc(d)?";|require ":abc$1";|g;
	$prog =~ s|"\."|":"|g;
    }

    open TEST, ">$tmpfile" or die "Cannot open >$tmpfile: $!";
    print TEST q{
        BEGIN {
            open(STDERR, ">&STDOUT")
              or die "Can't dup STDOUT->STDERR: $!;";
        }
    };
    print TEST "\n#line 1\n";  # So the line numbers don't get messed up.
    print TEST $prog,"\n";
    close TEST or die "Cannot close $tmpfile: $!";
    my $results = runperl( switches => [$switch], stderr => 1, progfile => $tmpfile );
    my $status = $?;
    $results =~ s/\n+$//;
    # allow expected output to be written as if $prog is on STDIN
    $results =~ s/tmp\d+/-/g;
    if ($^O eq 'VMS') {
        # some tests will trigger VMS messages that won't be expected
        $results =~ s/\n?%[A-Z]+-[SIWEF]-[A-Z]+,.*//;

        # pipes double these sometimes
        $results =~ s/\n\n/\n/g;
    }
# bison says 'parse error' instead of 'syntax error',
# various yaccs may or may not capitalize 'syntax'.
    $results =~ s/^(syntax|parse) error/syntax error/mig;
    # allow all tests to run when there are leaks
    $results =~ s/Scalars leaked: \d+\n//g;

    # fix up some paths
    if ($^O eq 'MacOS') {
	$results =~ s|:abc\.pm\b|abc.pm|g;
	$results =~ s|:abc(d)?\b|./abc$1|g;
    }

    $expected =~ s/\n+$//;
    my $prefix = ($results =~ s#^PREFIX(\n|$)##) ;
    # any special options? (OPTIONS foo bar zap)
    my $option_regex = 0;
    my $option_random = 0;
    if ($expected =~ s/^OPTIONS? (.+)\n//) {
	foreach my $option (split(' ', $1)) {
	    if ($option eq 'regex') { # allow regular expressions
		$option_regex = 1;
	    }
	    elsif ($option eq 'random') { # all lines match, but in any order
		$option_random = 1;
	    }
	    else {
		die "$0: Unknown OPTION '$option'\n";
	    }
	}
    }
    die "$0: can't have OPTION regex and random\n"
        if $option_regex + option_random > 1;
    my $ok = 1;
    if ( $results =~ s/^SKIPPED\n//) {
	print "$results\n" ;
    }
    elsif ($option_random)
    {
        $ok = randomMatch($results, $expected);
    }
    elsif (($prefix  && (( $option_regex && $results !~ /^$expected/) ||
			 (!$option_regex && $results !~ /^\Q$expected/))) or
	   (!$prefix && (( $option_regex && $results !~ /^$expected/) ||
			 (!$option_regex && $results ne $expected)))) {
        my $err_line = "PROG: $switch\n$prog\n" .
                       "EXPECTED:\n$expected\n" .
                       "GOT:\n$results\n";
        if ($todo) {
            $err_line =~ s/^/# /mg;
            print $err_line;  # Harness can't filter it out from STDERR.
        }
        else {
            print STDERR $err_line;
        }
        $ok = 0;
    }

    $TODO = $todo ? $todo_reason : 0;
    ok($ok);

    foreach (@temps)
	{ unlink $_ if $_ }
    foreach (@temp_path)
	{ rmtree $_ if -d $_ }
}

sub randomMatch
{
    my $got = shift ;
    my $expected = shift;

    my @got = sort split "\n", $got ;
    my @expected = sort split "\n", $expected ;

   return "@got" eq "@expected";

}
