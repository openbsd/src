#
# $Id: mime-header.t,v 2.0 2004/05/16 20:55:19 dankogai Exp $
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
use Test::More tests => 10;
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


$dheader=<<'EOS';
From: 小飼 弾 <dankogai@dan.co.jp>
To: dankogai@dan.co.jp (小飼=Kogai, 弾=Dan)
Subject: 漢字、カタカナ、ひらがなを含む、非常に長いタイトル行が一体全体どのようにしてEncodeされるのか？
EOS

my $bheader =<<'EOS';
From:=?UTF-8?B?IOWwj+mjvCDlvL4g?=<dankogai@dan.co.jp>
To: dankogai@dan.co.jp (=?UTF-8?B?5bCP6aO8?==Kogai,=?UTF-8?B?IOW8vg==?==Dan
 )
Subject:
 =?UTF-8?B?IOa8ouWtl+OAgeOCq+OCv+OCq+ODiuOAgeOBsuOCieOBjOOBquOCkuWQq+OCgA==?=
 =?UTF-8?B?44CB6Z2e5bi444Gr6ZW344GE44K/44Kk44OI44Or6KGM44GM5LiA5L2T5YWo?=
 =?UTF-8?B?5L2T44Gp44Gu44KI44GG44Gr44GX44GmRW5jb2Rl44GV44KM44KL44Gu44GL?=
 =?UTF-8?B?77yf?=
EOS

my $qheader=<<'EOS';
From:=?UTF-8?Q?=20=E5=B0=8F=E9=A3=BC=20=E5=BC=BE=20?=<dankogai@dan.co.jp>
To: dankogai@dan.co.jp (=?UTF-8?Q?=E5=B0=8F=E9=A3=BC?==Kogai,
 =?UTF-8?Q?=20=E5=BC=BE?==Dan)
Subject:
 =?UTF-8?Q?=20=E6=BC=A2=E5=AD=97=E3=80=81=E3=82=AB=E3=82=BF=E3=82=AB?=
 =?UTF-8?Q?=E3=83=8A=E3=80=81=E3=81=B2=E3=82=89=E3=81=8C=E3=81=AA=E3=82=92?=
 =?UTF-8?Q?=E5=90=AB=E3=82=80=E3=80=81=E9=9D=9E=E5=B8=B8=E3=81=AB=E9=95=B7?=
 =?UTF-8?Q?=E3=81=84=E3=82=BF=E3=82=A4=E3=83=88=E3=83=AB=E8=A1=8C=E3=81=8C?=
 =?UTF-8?Q?=E4=B8=80=E4=BD=93=E5=85=A8=E4=BD=93=E3=81=A9=E3=81=AE=E3=82=88?=
 =?UTF-8?Q?=E3=81=86=E3=81=AB=E3=81=97=E3=81=A6Encode=E3=81=95?=
 =?UTF-8?Q?=E3=82=8C=E3=82=8B=E3=81=AE=E3=81=8B=EF=BC=9F?=
EOS

is(Encode::decode('MIME-Header', $bheader), $dheader, "decode B");
is(Encode::decode('MIME-Header', $qheader), $dheader, "decode Q");
is(Encode::encode('MIME-B', $dheader)."\n", $bheader, "encode B");
is(Encode::encode('MIME-Q', $dheader)."\n", $qheader, "encode Q");

$dheader = "What is =?UTF-8?B?w4RwZmVs?= ?";
$bheader = "What is =?UTF-8?B?PT9VVEYtOD9CP3c0UndabVZzPz0=?= ?";
$qheader = "What is =?UTF-8?Q?=3D=3FUTF=2D8=3FB=3Fw4RwZmVs=3F=3D?= ?";
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
__END__;
