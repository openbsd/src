#!perl
# vim:syntax=perl:

BEGIN {
    $|= 1;

    # when building perl, skip this test if Win32API::File isn't being built
    if ( $ENV{PERL_CORE} ) {
	require Config;
	if ( $Config::Config{extensions} !~ m:(?<!\S)Win32API/File(?!\S): ) {
	    print "1..0 # Skip Win32API::File extension not built\n";
	    exit();
	}
    }

    print "1..10\n";
}
END   { print "not ok 1\n" unless $main::loaded; }

use strict;
use warnings;
use Win32API::File qw(:ALL);
use IO::File;

$main::loaded = 1;

print "ok 1\n";

unlink "foo.txt";

my $fh = new Win32API::File "+> foo.txt"
	or die fileLastError();

my $tell = tell $fh;
print "# tell \$fh == '$tell'\n";
print "not " unless
	tell $fh == 0;
print "ok 2\n";

my $text = "some text\n";

print "not " unless
	print $fh $text;
print "ok 3\n";

$tell = tell $fh;
print "# after printing 'some text\\n', tell is: '$tell'\n";
print "not " unless
	$tell == length($text) + 1;
print "ok 4\n";

print "not " unless
	seek($fh, 0, 0) == 0;
print "ok 5\n";

print "not " unless
	not eof $fh;
print "ok 6\n";

my $readline = <$fh>;

my $pretty_readline = $readline;
$pretty_readline =~ s/\r/\\r/g;  $pretty_readline =~ s/\n/\\n/g;  
print "# read line is '$pretty_readline'\n";

print "not " unless
	$readline eq "some text\r\n";
print "ok 7\n";

print "not " unless
	eof $fh;
print "ok 8\n";

print "not " unless
	close $fh;
print "ok 9\n";

# Test out binmode (should be only LF with print, no CR).

$fh = new Win32API::File "+> foo.txt"
	or die fileLastError();
binmode $fh;
print $fh "hello there\n";
seek $fh, 0, 0;

print "not " unless
	<$fh> eq "hello there\n";
print "ok 10\n";

close $fh;

unlink "foo.txt";
