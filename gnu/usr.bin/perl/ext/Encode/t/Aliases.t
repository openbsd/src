#!../perl

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
}

use strict;
use Encode;
use Encode::Alias;
my %a2c;
my $ON_EBCDIC;

sub init_a2c{
    %a2c = (
	    'US-ascii' => 'ascii',
	    'ISO-646-US' => 'ascii',
	    'UTF-8'    => 'utf8',
	    'UCS-2'    => 'UCS-2BE',
	    'UCS2'     => 'UCS-2BE',
	    'iso-10646-1' => 'UCS-2BE',
	    'ucs2-le'  => 'UCS-2LE',
	    'ucs2-be'  => 'UCS-2BE',
	    'utf16'    => 'UTF-16',
	    'utf32'    => 'UTF-32',
	    'utf16-be'  => 'UTF-16BE',
	    'utf32-be'  => 'UTF-32BE',
	    'utf16-le'  => 'UTF-16LE',
	    'utf32-le'  => 'UTF-32LE',
	    'UCS4-BE'   => 'UTF-32BE',
	    'UCS-4-LE'  => 'UTF-32LE',
	    'cyrillic' => 'iso-8859-5',
	    'arabic'   => 'iso-8859-6',
	    'greek'    => 'iso-8859-7',
	    'hebrew'   => 'iso-8859-8',
	    'thai'     => 'iso-8859-11',
	    'tis620'   => 'iso-8859-11',
	    'WinLatin1'     => 'cp1252',
	    'WinLatin2'     => 'cp1250',
	    'WinCyrillic'   => 'cp1251',
	    'WinGreek'      => 'cp1253',
	    'WinTurkish'    => 'cp1254',
	    'WinHebrew'     => 'cp1255',
	    'WinArabic'     => 'cp1256',
	    'WinBaltic'     => 'cp1257',
	    'WinVietnamese' => 'cp1258',
	    'koi8r'         => 'koi8-r',
	    'koi8u'         => 'koi8-u',
	    'ja_JP.euc'	    => $ON_EBCDIC ? '' : 'euc-jp',
	    'x-euc-jp'	    => $ON_EBCDIC ? '' : 'euc-jp',
	    'zh_CN.euc'	    => $ON_EBCDIC ? '' : 'euc-cn',
	    'x-euc-cn'	    => $ON_EBCDIC ? '' : 'euc-cn',
	    'ko_KR.euc'	    => $ON_EBCDIC ? '' : 'euc-kr',
	    'x-euc-kr'	    => $ON_EBCDIC ? '' : 'euc-kr',
	    'ujis'	    => $ON_EBCDIC ? '' : 'euc-jp',
	    'Shift_JIS'	    => $ON_EBCDIC ? '' : 'shiftjis',
	    'x-sjis'	    => $ON_EBCDIC ? '' : 'shiftjis',
	    'jis'	    => $ON_EBCDIC ? '' : '7bit-jis',
	    'big-5'	    => $ON_EBCDIC ? '' : 'big5-eten',
	    'zh_TW.Big5'    => $ON_EBCDIC ? '' : 'big5-eten',
	    'tca-big5'	    => $ON_EBCDIC ? '' : 'big5-eten',
	    'big5-hk'	    => $ON_EBCDIC ? '' : 'big5-hkscs',
	    'hkscs-big5'    => $ON_EBCDIC ? '' : 'big5-hkscs',
	    'GB_2312-80'    => $ON_EBCDIC ? '' : 'euc-cn',
	    'KS_C_5601-1987'    => $ON_EBCDIC ? '' : 'cp949',
	    #
	    'gb12345-raw'   => $ON_EBCDIC ? '' : 'gb12345-raw',
	    'gb2312-raw'    => $ON_EBCDIC ? '' : 'gb2312-raw',
	    'jis0201-raw'   => $ON_EBCDIC ? '' : 'jis0201-raw',
	    'jis0208-raw'   => $ON_EBCDIC ? '' : 'jis0208-raw',
	    'jis0212-raw'   => $ON_EBCDIC ? '' : 'jis0212-raw',
	    'ksc5601-raw'   => $ON_EBCDIC ? '' : 'ksc5601-raw',
	   );

    for my $i (1..11,13..16){
	$a2c{"ISO 8859 $i"} = "iso-8859-$i";
    }
    for my $i (1..10){
	$a2c{"ISO Latin $i"} = "iso-8859-$Encode::Alias::Latin2iso[$i]";
    }
    for my $k (keys %Encode::Alias::Winlatin2cp){
	my $v = $Encode::Alias::Winlatin2cp{$k};
	$a2c{"Win" . ucfirst($k)} = "cp" . $v;
	$a2c{"IBM-$v"} = $a2c{"MS-$v"} = "cp" . $v;
	$a2c{"cp-" . $v} = "cp" . $v;
    }
    my @a2c = keys %a2c;
    for my $k (@a2c){
	$a2c{uc($k)} = $a2c{$k};
	$a2c{lc($k)} = $a2c{$k};
	$a2c{lcfirst($k)} = $a2c{$k};
	$a2c{ucfirst($k)} = $a2c{$k};
    }
}

BEGIN{
    $ON_EBCDIC = ord("A") == 193;
    @ARGV and $ON_EBCDIC = $ARGV[0] eq 'EBCDIC';
    $Encode::ON_EBCDIC = $ON_EBCDIC;
    init_a2c();
}

if ($ON_EBCDIC){
    delete @Encode::ExtModule{
	qw(euc-cn gb2312 gb12345 gbk cp936 iso-ir-165 MacChineseSimp
	   euc-jp iso-2022-jp 7bit-jis shiftjis MacJapanese cp932
	   euc-kr ksc5601 cp949 MacKorean
	   big5	big5-hkscs cp950 MacChineseTrad
	   gb18030 big5plus euc-tw)
	};
}

use Test::More tests => (scalar keys %a2c) * 4;

print "# alias test;  \$ON_EBCDIC == $ON_EBCDIC\n";

foreach my $a (keys %a2c){	
    my $e = Encode::find_encoding($a);
    is((defined($e) and $e->name), $a2c{$a},$a)
	or warn "alias was $a";;
}

# now we override some of the aliases and see if it works fine

define_alias(
	     qr/ascii/i    => 'WinLatin1',
	     qr/cyrillic/i => 'WinCyrillic',
	     qr/arabic/i   => 'WinArabic',
	     qr/greek/i    => 'WinGreek',
	     qr/hebrew/i   => 'WinHebrew'
	    );

print "# alias test with alias overrides\n";

foreach my $a (keys %a2c){	
    my $e = Encode::find_encoding($a);
    is((defined($e) and $e->name), $a2c{$a}, "Override $a")
	or warn "alias was $a";
}

print "# alias undef test\n";

Encode::Alias->undef_aliases;
foreach my $a (keys %a2c){	
    my $e = Encode::find_encoding($a);
    ok(!defined($e) || $e->name =~ /-raw$/o,"Undef $a")
	or warn "alias was $a";
}

print "# alias reinit test\n";

Encode::Alias->init_aliases;
init_a2c();
foreach my $a (keys %a2c){	
    my $e = Encode::find_encoding($a);
    is((defined($e) and $e->name), $a2c{$a}, "Reinit $a")
	or warn "alias was $a";
}
__END__
for my $k (keys %a2c){
    $k =~ /[A-Z]/ and next;
    print "$k => $a2c{$k}\n";
}



