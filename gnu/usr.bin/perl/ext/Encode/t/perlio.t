BEGIN {
    if ($ENV{'PERL_CORE'}){
        chdir 't';
        unshift @INC, '../lib';
    }
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bEncode\b/) {
      print "1..0 # Skip: Encode was not built\n";
      exit 0;
    }
    if (ord("A") == 193) {
	print "1..0 # Skip: EBCDIC\n";
	exit 0;
    }
    unless (PerlIO::Layer->find('perlio')){
        print "1..0 # Skip: PerlIO required\n";
        exit 0;
    }
    $| = 1;
}

use strict;
use File::Basename;
use File::Spec;
use File::Compare qw(compare_text);
use File::Copy;
use FileHandle;

#use Test::More qw(no_plan);
use Test::More tests => 28;

our $DEBUG = 0;

use Encode (":all");
{
    no warnings;
    @ARGV and $DEBUG = shift;
    #require Encode::JP::JIS7;
    #require Encode::KR::2022_KR;
    #$Encode::JP::JIS7::DEBUG = $DEBUG;
}



my $seq = 0;
my $dir = dirname(__FILE__);

my %e = 
    (
     jisx0208 => [ qw/euc-jp shiftjis 7bit-jis iso-2022-jp iso-2022-jp-1/],
     #ksc5601  => [ qw/euc-kr iso-2022-kr/],
     ksc5601  => [ qw/euc-kr/],
     #gb2312   => [ qw/euc-cn hz/],
     gb2312   => [ qw/euc-cn/],
    );

$/ = "\x0a"; # may fix VMS problem for test #28 and #29

for my $src(sort keys %e) {
    my $ufile = File::Spec->catfile($dir,"$src.utf");
    open my $fh, "<:utf8", $ufile or die "$ufile : $!";
    my @uline = <$fh>;
    my $utext = join('' => @uline);
    close $fh;

    for my $e (@{$e{$src}}){
	my $sfile = File::Spec->catfile($dir,"$$.sio");
	my $pfile = File::Spec->catfile($dir,"$$.pio");
    
	# first create a file without perlio
	dump2file($sfile, &encode($e, $utext, 0));
    
	# then create a file via perlio without autoflush

    TODO:{
	    #local $TODO = "$e: !perlio_ok" unless (perlio_ok($e) or $DEBUG);
	    todo_skip "$e: !perlio_ok", 4 unless (perlio_ok($e) or $DEBUG);
	    no warnings 'uninitialized';
	    open $fh, ">:encoding($e)", $pfile or die "$sfile : $!";
	    $fh->autoflush(0);
	    print $fh $utext;
	    close $fh;
	    $seq++;
	    is(compare_text($sfile, $pfile), 0 => ">:encoding($e)");
	    if ($DEBUG){
		copy $sfile, "$sfile.$seq";
		copy $pfile, "$pfile.$seq";
	    }
	    
	    # this time print line by line.
	    # works even for ISO-2022 but not ISO-2022-KR
	    open $fh, ">:encoding($e)", $pfile or die "$sfile : $!";
	    $fh->autoflush(1);
	    for my $l (@uline) {
		print $fh $l;
	    }
	    close $fh;
	    $seq++;
	    is(compare_text($sfile, $pfile), 0 => ">:encoding($e) by lines");
	    if ($DEBUG){
		copy $sfile, "$sfile.$seq";
		copy $pfile, "$pfile.$seq";
	    }
	    my $dtext;
	    open $fh, "<:encoding($e)", $pfile or die "$pfile : $!";
	    $fh->autoflush(0);
	    $dtext = join('' => <$fh>);
	    close $fh;
	    $seq++;
	    ok($utext eq $dtext, "<:encoding($e)");
	    if ($DEBUG){
		dump2file("$sfile.$seq", $utext);
		dump2file("$pfile.$seq", $dtext);
	    }
	    if (perlio_ok($e) or $DEBUG){
		$dtext = '';
		open $fh, "<:encoding($e)", $pfile or die "$pfile : $!";
		while(defined(my $l = <$fh>)) {
		    $dtext .= $l;
		}
		close $fh;
	    }
	    $seq++;
	    ok($utext eq $dtext,  "<:encoding($e) by lines");
	    if ($DEBUG){
		dump2file("$sfile.$seq", $utext);
		dump2file("$pfile.$seq", $dtext);
	    }
	}
	$DEBUG or unlink ($sfile, $pfile);
    }
}
    

sub dump2file{
    no warnings;
    open my $fh, ">", $_[0] or die "$_[0]: $!";
    binmode $fh;
    print $fh $_[1];
    close $fh;
}
