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
use IO::Uncompress::Unzip 'unzip' ;

BEGIN 
{ 
    plan(skip_all => "Needs Perl 5.005 or better - you have Perl $]" )
        if $] < 5.005 ;
    
    # use Test::NoWarnings, if available
    my $extra = 0 ;
    $extra = 1
        if eval { require Test::NoWarnings ;  import Test::NoWarnings; 1 };

    plan tests => 8 + $extra ;
}


my $Inc = join " ", map qq["-I$_"] => @INC;
$Inc = '"-MExtUtils::testlib"'
    if ! $ENV{PERL_CORE} && eval " require ExtUtils::testlib; " ;

my $Perl = ($ENV{'FULLPERL'} or $^X or 'perl') ;
$Perl = qq["$Perl"] if $^O eq 'MSWin32' ;
 
$Perl = "$Perl $Inc -w" ;
#$Perl .= " -Mblib " ;
my $binDir = $ENV{PERL_CORE} ? "../ext/IO-Compress/bin/"
                             : "./bin/";

my $hello1 = <<EOM ;
hello
this is 
a test
message
x ttttt
xuuuuuu
the end
EOM




my $lex = new LexFile my $stderr ;


sub check
{
    my $command = shift ;
    my $expected = shift ;

    my $lex = new LexFile my $stderr ;

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


# streamzip
# ########

{
    title "streamzip" ;

    my ($infile, $outfile);
    my $lex = new LexFile $infile, $outfile ;

    writeFile($infile, $hello1) ;
    check "$Perl ${binDir}/streamzip <$infile >$outfile";

    my $uncompressed ;
    unzip $outfile => \$uncompressed;
    is $uncompressed, $hello1;
}

{
    title "streamzip" ;

    my ($infile, $outfile);
    my $lex = new LexFile $infile, $outfile ;

    writeFile($infile, $hello1) ;
    check "$Perl ${binDir}/streamzip -zipfile=$outfile <$infile";

    my $uncompressed ;
    unzip $outfile => \$uncompressed;
    is $uncompressed, $hello1;
}
