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
# should work w/o PerlIO now!
#    unless (PerlIO::Layer->find('perlio')){
#	print "1..0 # Skip: PerlIO required\n";
#	exit 0;
#   }
    $| = 1;
}
use strict;
use Test::More tests => 73;
#use Test::More qw(no_plan);
use Encode;
use File::Basename;
use File::Spec;
use File::Compare qw(compare_text);
our $DEBUG = shift || 0;

my %Charset =
    (
     'big5-eten'  => [qw(big5-eten cp950 MacChineseTrad)],
     'big5-hkscs' => [qw(big5-hkscs)],
     gb2312       => [qw(euc-cn gb2312-raw cp936 MacChineseSimp)],
     jisx0201     => [qw(euc-jp shiftjis 7bit-jis jis0201-raw
			 cp932 MacJapanese)],
     jisx0212     => [qw(euc-jp 7bit-jis iso-2022-jp-1 jis0208-raw)],
     jisx0208     => [qw(euc-jp shiftjis 7bit-jis cp932 MacJapanese
		     iso-2022-jp iso-2022-jp-1 jis0212-raw)],
     ksc5601      => [qw(euc-kr iso-2022-kr ksc5601-raw cp949 MacKorean)],
    );

my $dir = dirname(__FILE__);
my $seq = 1;

for my $charset (sort keys %Charset){
    my ($src, $uni, $dst, $txt);

    my $transcoder = find_encoding($Charset{$charset}[0]) or die;

    my $src_enc = File::Spec->catfile($dir,"$charset.enc");
    my $src_utf = File::Spec->catfile($dir,"$charset.utf");
    my $dst_enc = File::Spec->catfile($dir,"$$.enc");
    my $dst_utf = File::Spec->catfile($dir,"$$.utf");


    open $src, "<$src_enc" or die "$src_enc : $!";
    # binmode($src); # not needed! 

    $txt = join('',<$src>);
    close($src);
    
    eval{ $uni = $transcoder->decode($txt, 1) }; 
    $@ and print $@;
    ok(defined($uni),  "decode $charset"); $seq++;
    is(length($txt),0, "decode $charset completely"); $seq++;
    
    open $dst, ">$dst_utf" or die "$dst_utf : $!";
    if (PerlIO::Layer->find('perlio')){
	binmode($dst, ":utf8");
	print $dst $uni;
    }else{ # ugh!
	binmode($dst);
	my $raw = $uni; Encode::_utf8_off($raw);
	print $dst $raw;
    }

    close($dst); 
    is(compare_text($dst_utf, $src_utf), 0, "$dst_utf eq $src_utf")
	or ($DEBUG and rename $dst_utf, "$dst_utf.$seq");
    $seq++;
    
    open $src, "<$src_utf" or die "$src_utf : $!";
    if (PerlIO::Layer->find('perlio')){
	binmode($src, ":utf8");
	$uni = join('', <$src>);
    }else{ # ugh!
	binmode($src);
	$uni = join('', <$src>);
	Encode::_utf8_on($uni);
    }
    close $src;

    eval{ $txt = $transcoder->encode($uni,1) };    
    $@ and print $@;
    ok(defined($txt),   "encode $charset"); $seq++;
    is(length($uni), 0, "encode $charset completely");  $seq++;

    open $dst,">$dst_enc" or die "$dst_utf : $!";
    binmode($dst);
    print $dst $txt;
    close($dst); 
    is(compare_text($src_enc, $dst_enc), 0 => "$dst_enc eq $src_enc")
	or ($DEBUG and rename $dst_enc, "$dst_enc.$seq");
    $seq++;
    
    for my $canon (@{$Charset{$charset}}){
	is($uni, decode($canon, encode($canon, $uni)), 
	   "RT/$charset/$canon");
	$seq++;
     }
    unlink($dst_utf, $dst_enc);
}
