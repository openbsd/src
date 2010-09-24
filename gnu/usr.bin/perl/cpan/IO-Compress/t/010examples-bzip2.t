BEGIN {
    if ($ENV{PERL_CORE}) {
	chdir 't' if -d 't';
	@INC = ("../lib", "lib/compress");
    }
}

use lib qw(t t/compress);

use strict;
use warnings;
use bytes;

use Test::More ;
use CompTestUtils;
use IO::Compress::Bzip2 'bzip2' ;

BEGIN 
{ 
    plan(skip_all => "Examples needs Perl 5.005 or better - you have Perl $]" )
        if $] < 5.005 ;
    
    # use Test::NoWarnings, if available
    my $extra = 0 ;
    $extra = 1
        if eval { require Test::NoWarnings ;  import Test::NoWarnings; 1 };

    plan tests => 19 + $extra ;
}


my $Inc = join " ", map qq["-I$_"] => @INC;
$Inc = '"-MExtUtils::testlib"'
    if ! $ENV{PERL_CORE} && eval " require ExtUtils::testlib; " ;

my $Perl = ($ENV{'FULLPERL'} or $^X or 'perl') ;
$Perl = qq["$Perl"] if $^O eq 'MSWin32' ;
 
$Perl = "$Perl $Inc -w" ;
#$Perl .= " -Mblib " ;
my $examples = $ENV{PERL_CORE} ? "../ext/IO-Compress/examples/io/bzip2"
                               : "./examples/io/bzip2";

my $hello1 = <<EOM ;
hello
this is 
a test
message
x ttttt
xuuuuuu
the end
EOM

my @hello1 = grep(s/$/\n/, split(/\n/, $hello1)) ;

my $hello2 = <<EOM;

Howdy
this is the
second
file
x ppppp
xuuuuuu
really the end
EOM

my @hello2 = grep(s/$/\n/, split(/\n/, $hello2)) ;

my $file1 = "hello1.gz" ;
my $file2 = "hello2.gz" ;
my $stderr = "err.out" ;

for ($file1, $file2, $stderr) { 1 while unlink $_ } ;


bzip2 \$hello1 => $file1 ;
bzip2 \$hello2 => $file2 ;

sub check
{
    my $command = shift ;
    my $expected = shift ;

    my $stderr = 'err.out';
    1 while unlink $stderr;

    my $cmd = "$command 2>$stderr";
    my $stdout = `$cmd` ;

    my $aok = 1 ;

    $aok &= is $?, 0, "  exit status is 0" ;

    $aok &= is readFile($stderr), '', "  no stderr" ;

    $aok &= is $stdout, $expected, "  expected content is ok"
        if defined $expected ;

    if (! $aok) {
        diag "Command line: $cmd";
        my ($file, $line) = (caller)[1,2];
        diag "Test called from $file, line $line";
    }

    1 while unlink $stderr;
}

# bzcat
# #####

title "bzcat - command line" ;
check "$Perl ${examples}/bzcat $file1 $file2",  $hello1 . $hello2;

title "bzcat - stdin" ;
check "$Perl ${examples}/bzcat <$file1 ", $hello1;


# bzgrep
# ######

title "bzgrep";
check "$Perl  ${examples}/bzgrep the $file1 $file2",
        join('', grep(/the/, @hello1, @hello2));

for ($file1, $file2, $stderr) { 1 while unlink $_ } ;



# bzstream
# ########

{
    title "bzstream" ;
    writeFile($file1, $hello1) ;
    check "$Perl ${examples}/bzstream <$file1 >$file2";

    title "bzcat" ;
    check "$Perl ${examples}/bzcat $file2", $hello1 ;
}

END
{
    for ($file1, $file2, $stderr) { 1 while unlink $_ } ;
}

