#
# $Id: mime-header.t,v 2.8 2016/01/25 14:54:13 dankogai Exp dankogai $
# This script is written in utf8
#
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
    $| = 1;
}

no utf8;

use strict;
#use Test::More qw(no_plan);
use Test::More tests => 14;
use_ok("Encode::MIME::Header");

my $eheader =<<'EOS';
From: =?US-ASCII?Q?Keith_Moore?= <moore@cs.utk.edu>
To: =?ISO-8859-1?Q?Keld_J=F8rn_Simonsen?= <keld@dkuug.dk>
CC: =?ISO-8859-1?Q?Andr=E9?= Pirard <PIRARD@vm1.ulg.ac.be>
Subject: =?ISO-8859-1?B?SWYgeW91IGNhbiByZWFkIHRoaXMgeW8=?=
 =?ISO-8859-2?B?dSB1bmRlcnN0YW5kIHRoZSBleGFtcGxlLg==?=
EOS

my $dheader=<<"EOS";
From: Keith Moore <moore\@cs.utk.edu>
To: Keld J\xF8rn Simonsen <keld\@dkuug.dk>
CC: Andr\xE9 Pirard <PIRARD\@vm1.ulg.ac.be>
Subject: If you can read this you understand the example.
EOS

is(Encode::decode('MIME-Header', $eheader), $dheader, "decode ASCII (RFC2047)");

use utf8;

my $uheader =<<'EOS';
From: =?US-ASCII?Q?Keith_Moore?= <moore@cs.utk.edu>
To: =?ISO-8859-1?Q?Keld_J=F8rn_Simonsen?= <keld@dkuug.dk>
CC: =?ISO-8859-1?Q?Andr=E9?= Pirard <PIRARD@vm1.ulg.ac.be>
Subject: =?ISO-8859-1?B?SWYgeW91IGNhbiByZWFkIHRoaXMgeW8=?=
 =?ISO-8859-2?B?dSB1bmRlcnN0YW5kIHRoZSBleGFtcGxlLg==?=
EOS

is(Encode::decode('MIME-Header', $uheader), $dheader, "decode UTF-8 (RFC2047)");

my $lheader =<<'EOS';
From: =?US-ASCII*en-US?Q?Keith_Moore?= <moore@cs.utk.edu>
To: =?ISO-8859-1*da-DK?Q?Keld_J=F8rn_Simonsen?= <keld@dkuug.dk>
CC: =?ISO-8859-1*fr-BE?Q?Andr=E9?= Pirard <PIRARD@vm1.ulg.ac.be>
Subject: =?ISO-8859-1*en?B?SWYgeW91IGNhbiByZWFkIHRoaXMgeW8=?=
 =?ISO-8859-2?B?dSB1bmRlcnN0YW5kIHRoZSBleGFtcGxlLg==?=
EOS

is(Encode::decode('MIME-Header', $lheader), $dheader, "decode language tag (RFC2231)");


$dheader=<<'EOS';
From: 小飼 弾 <dankogai@dan.co.jp>
To: dankogai@dan.co.jp (小飼=Kogai, 弾=Dan)
Subject: 漢字、カタカナ、ひらがなを含む、非常に長いタイトル行が一体全体どのようにしてEncodeされるのか？
EOS

my $bheader =<<'EOS';
From: =?UTF-8?B?5bCP6aO8IOW8viA8ZGFua29nYWlAZGFuLmNvLmpwPg==?=
To: =?UTF-8?B?ZGFua29nYWlAZGFuLmNvLmpwICjlsI/po7w9S29nYWksIOW8vj1EYW4p?=
Subject: 
 =?UTF-8?B?5ryi5a2X44CB44Kr44K/44Kr44OK44CB44Gy44KJ44GM44Gq44KS5ZCr44KA?=
 =?UTF-8?B?44CB6Z2e5bi444Gr6ZW344GE44K/44Kk44OI44Or6KGM44GM5LiA5L2T5YWo?=
 =?UTF-8?B?5L2T44Gp44Gu44KI44GG44Gr44GX44GmRW5jb2Rl44GV44KM44KL44Gu44GL?=
 =?UTF-8?B?77yf?=
EOS

my $qheader=<<'EOS';
From: =?UTF-8?Q?=E5=B0=8F=E9=A3=BC=20=E5=BC=BE=20=3Cdankogai=40?=
 =?UTF-8?Q?dan=2Eco=2Ejp=3E?=
To: =?UTF-8?Q?dankogai=40dan=2Eco=2Ejp=20=28?=
 =?UTF-8?Q?=E5=B0=8F=E9=A3=BC=3DKogai=2C=20=E5=BC=BE=3DDan?= =?UTF-8?Q?=29?=
Subject: 
 =?UTF-8?Q?=E6=BC=A2=E5=AD=97=E3=80=81=E3=82=AB=E3=82=BF=E3=82=AB=E3=83=8A?=
 =?UTF-8?Q?=E3=80=81=E3=81=B2=E3=82=89=E3=81=8C=E3=81=AA=E3=82=92=E5=90=AB?=
 =?UTF-8?Q?=E3=82=80=E3=80=81=E9=9D=9E=E5=B8=B8=E3=81=AB=E9=95=B7=E3=81=84?=
 =?UTF-8?Q?=E3=82=BF=E3=82=A4=E3=83=88=E3=83=AB=E8=A1=8C=E3=81=8C=E4=B8=80?=
 =?UTF-8?Q?=E4=BD=93=E5=85=A8=E4=BD=93=E3=81=A9=E3=81=AE=E3=82=88=E3=81=86?=
 =?UTF-8?Q?=E3=81=AB=E3=81=97=E3=81=A6Encode=E3=81=95=E3=82=8C?=
 =?UTF-8?Q?=E3=82=8B=E3=81=AE=E3=81=8B=EF=BC=9F?=
EOS

is(Encode::decode('MIME-Header', $bheader), $dheader, "decode B");
is(Encode::decode('MIME-Header', $qheader), $dheader, "decode Q");
is(Encode::encode('MIME-B', $dheader)."\n", $bheader, "encode B");
is(Encode::encode('MIME-Q', $dheader)."\n", $qheader, "encode Q");

$dheader = "What is =?UTF-8?B?w4RwZmVs?= ?";
$bheader = "=?UTF-8?B?V2hhdCBpcyA9P1VURi04P0I/dzRSd1ptVnM/PSA/?=";
$qheader = "=?UTF-8?Q?What=20is=20=3D=3FUTF=2D8=3FB=3Fw4R?="
         . "\n " . "=?UTF-8?Q?wZmVs=3F=3D=20=3F?=";
is(Encode::encode('MIME-B', $dheader), $bheader, "Double decode B");
is(Encode::encode('MIME-Q', $dheader), $qheader, "Double decode Q");
{
    # From: Dave Evans <dave@rudolf.org.uk>
    # Subject: Bug in Encode::MIME::Header
    # Message-Id: <3F43440B.7060606@rudolf.org.uk>
    use charnames ":full";
    my $pound_1024 = "\N{POUND SIGN}1024";
    is(Encode::encode('MIME-Q' => $pound_1024), '=?UTF-8?Q?=C2=A31024?=',
       'pound 1024');
}

is(Encode::encode('MIME-Q', "\x{fc}"), '=?UTF-8?Q?=C3=BC?=', 'Encode latin1 characters');

# RT42627

my $rt42627 = Encode::decode_utf8("\x{c2}\x{a3}xxxxxxxxxxxxxxxxxxx0");
is(Encode::encode('MIME-Q', $rt42627), 
   '=?UTF-8?Q?=C2=A3xxxxxxxxxxxxxxxxxxx?= =?UTF-8?Q?0?=',
   'MIME-Q encoding does not truncate trailing zeros');

# RT87831
is(Encode::encode('MIME-Header', '0'), '=?UTF-8?B?MA==?=', 'RT87831');
__END__;
