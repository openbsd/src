#!./perl -w

# Tests for the command-line switches:
# -0, -c, -l, -s, -m, -M, -V, -v, -h, -z, -i
# Some switches have their own tests, see MANIFEST.

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

require "./test.pl";

plan(tests => 26);

use Config;

# due to a bug in VMS's piping which makes it impossible for runperl()
# to emulate echo -n (ie. stdin always winds up with a newline), these 
# tests almost totally fail.
$TODO = "runperl() unable to emulate echo -n due to pipe bug" if $^O eq 'VMS';

my $r;
my @tmpfiles = ();
END { unlink @tmpfiles }

# Tests for -0

$r = runperl(
    switches	=> [ '-0', ],
    stdin	=> 'foo\0bar\0baz\0',
    prog	=> 'print qq(<$_>) while <>',
);
is( $r, "<foo\0><bar\0><baz\0>", "-0" );

$r = runperl(
    switches	=> [ '-l', '-0', '-p' ],
    stdin	=> 'foo\0bar\0baz\0',
    prog	=> '1',
);
is( $r, "foo\nbar\nbaz\n", "-0 after a -l" );

$r = runperl(
    switches	=> [ '-0', '-l', '-p' ],
    stdin	=> 'foo\0bar\0baz\0',
    prog	=> '1',
);
is( $r, "foo\0bar\0baz\0", "-0 before a -l" );

$r = runperl(
    switches	=> [ sprintf("-0%o", ord 'x') ],
    stdin	=> 'fooxbarxbazx',
    prog	=> 'print qq(<$_>) while <>',
);
is( $r, "<foox><barx><bazx>", "-0 with octal number" );

$r = runperl(
    switches	=> [ '-00', '-p' ],
    stdin	=> 'abc\ndef\n\nghi\njkl\nmno\n\npq\n',
    prog	=> 's/\n/-/g;$_.=q(/)',
);
is( $r, 'abc-def--/ghi-jkl-mno--/pq-/', '-00 (paragraph mode)' );

$r = runperl(
    switches	=> [ '-0777', '-p' ],
    stdin	=> 'abc\ndef\n\nghi\njkl\nmno\n\npq\n',
    prog	=> 's/\n/-/g;$_.=q(/)',
);
is( $r, 'abc-def--ghi-jkl-mno--pq-/', '-0777 (slurp mode)' );

$r = runperl(
    switches	=> [ '-066' ],
    prog	=> 'BEGIN { print qq{($/)} } print qq{[$/]}',
);
is( $r, "(\066)[\066]", '$/ set at compile-time' );

# Tests for -c

my $filename = 'swctest.tmp';
SKIP: {
    local $TODO = '';   # this one works on VMS

    open my $f, ">$filename" or skip( "Can't write temp file $filename: $!" );
    print $f <<'SWTEST';
BEGIN { print "block 1\n"; }
CHECK { print "block 2\n"; }
INIT  { print "block 3\n"; }
	print "block 4\n";
END   { print "block 5\n"; }
SWTEST
    close $f or die "Could not close: $!";
    $r = runperl(
	switches	=> [ '-c' ],
	progfile	=> $filename,
	stderr		=> 1,
    );
    # Because of the stderr redirection, we can't tell reliably the order
    # in which the output is given
    ok(
	$r =~ /$filename syntax OK/
	&& $r =~ /\bblock 1\b/
	&& $r =~ /\bblock 2\b/
	&& $r !~ /\bblock 3\b/
	&& $r !~ /\bblock 4\b/
	&& $r !~ /\bblock 5\b/,
	'-c'
    );
    push @tmpfiles, $filename;
}

# Tests for -l

$r = runperl(
    switches	=> [ sprintf("-l%o", ord 'x') ],
    prog	=> 'print for qw/foo bar/'
);
is( $r, 'fooxbarx', '-l with octal number' );

# Tests for -s

$r = runperl(
    switches	=> [ '-s' ],
    prog	=> 'for (qw/abc def ghi/) {print defined $$_ ? $$_ : q(-)}',
    args	=> [ '--', '-abc=2', '-def', ],
);
is( $r, '21-', '-s switch parsing' );

# Bug ID 20011106.084
$filename = 'swstest.tmp';
SKIP: {
    open my $f, ">$filename" or skip( "Can't write temp file $filename: $!" );
    print $f <<'SWTEST';
#!perl -sn
BEGIN { print $x; exit }
SWTEST
    close $f or die "Could not close: $!";
    $r = runperl(
	progfile    => $filename,
	args	    => [ '-x=foo' ],
    );
    is( $r, 'foo', '-s on the shebang line' );
    push @tmpfiles, $filename;
}

# Tests for -m and -M

$filename = 'swtest.pm';
SKIP: {
    open my $f, ">$filename" or skip( "Can't write temp file $filename: $!",4 );
    print $f <<'SWTESTPM';
package swtest;
sub import { print map "<$_>", @_ }
1;
SWTESTPM
    close $f or die "Could not close: $!";
    $r = runperl(
	switches    => [ '-Mswtest' ],
	prog	    => '1',
    );
    is( $r, '<swtest>', '-M' );
    $r = runperl(
	switches    => [ '-Mswtest=foo' ],
	prog	    => '1',
    );
    is( $r, '<swtest><foo>', '-M with import parameter' );
    $r = runperl(
	switches    => [ '-mswtest' ],
	prog	    => '1',
    );

    {
        local $TODO = '';  # this one works on VMS
        is( $r, '', '-m' );
    }
    $r = runperl(
	switches    => [ '-mswtest=foo,bar' ],
	prog	    => '1',
    );
    is( $r, '<swtest><foo><bar>', '-m with import parameters' );
    push @tmpfiles, $filename;
}

# Tests for -V

{
    local $TODO = '';   # these ones should work on VMS

    # basic perl -V should generate significant output.
    # we don't test actual format too much since it could change
    like( runperl( switches => ['-V'] ), qr/(\n.*){20}/,
          '-V generates 20+ lines' );

    like( runperl( switches => ['-V'] ),
	  qr/\ASummary of my perl5 .*configuration:/,
          '-V looks okay' );

    # lookup a known config var
    chomp( $r=runperl( switches => ['-V:osname'] ) );
    is( $r, "osname='$^O';", 'perl -V:osname');

    # lookup a nonexistent var
    chomp( $r=runperl( switches => ['-V:this_var_makes_switches_test_fail'] ) );
    is( $r, "this_var_makes_switches_test_fail='UNKNOWN';",
        'perl -V:unknown var');

    # regexp lookup
    # platforms that don't like this quoting can either skip this test
    # or fix test.pl _quote_args
    $r = runperl( switches => ['"-V:i\D+size"'] );
    # should be unlike( $r, qr/^$|not found|UNKNOWN/ );
    like( $r, qr/^(?!.*(not found|UNKNOWN))./, 'perl -V:re got a result' );

    # make sure each line we got matches the re
    ok( !( grep !/^i\D+size=/, split /^/, $r ), '-V:re correct' );
}

# Tests for -v

{
    local $TODO = '';   # these ones should work on VMS

    my $v = sprintf "%vd", $^V;
    like( runperl( switches => ['-v'] ),
	  qr/This is perl, v$v built for $Config{archname}.+Copyright.+Larry Wall.+Artistic License.+GNU General Public License/s,
          '-v looks okay' );

}

# Tests for -h

{
    local $TODO = '';   # these ones should work on VMS

    like( runperl( switches => ['-h'] ),
	  qr/Usage: .+(?i:perl(?:$Config{_exe})?).+switches.+programfile.+arguments/,
          '-h looks okay' );

}

# Tests for -z (which does not exist)

{
    local $TODO = '';   # these ones should work on VMS

    like( runperl( switches => ['-z'], stderr => 1 ),
	  qr/\QUnrecognized switch: -z  (-h will show valid options)./,
          '-z correctly unknown' );

}

# Tests for -i

{
    local $TODO = '';   # these ones should work on VMS

    sub do_i_unlink { 1 while unlink("file", "file.bak") }

    open(FILE, ">file") or die "$0: Failed to create 'file': $!";
    print FILE <<__EOF__;
foo yada dada
bada foo bing
king kong foo
__EOF__
    close FILE;

    END { do_i_unlink() }

    runperl( switches => ['-pi.bak'], prog => 's/foo/bar/', args => ['file'] );

    open(FILE, "file") or die "$0: Failed to open 'file': $!";
    chomp(my @file = <FILE>);
    close FILE;

    open(BAK, "file.bak") or die "$0: Failed to open 'file': $!";
    chomp(my @bak = <BAK>);
    close BAK;

    is(join(":", @file),
       "bar yada dada:bada bar bing:king kong bar",
       "-i new file");
    is(join(":", @bak),
       "foo yada dada:bada foo bing:king kong foo",
       "-i backup file");
}
