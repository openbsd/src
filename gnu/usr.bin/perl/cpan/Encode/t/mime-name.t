#
# $Id: mime-name.t,v 1.1 2007/05/12 06:42:19 dankogai Exp $
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

use strict;
use warnings;
use Encode;
#use Test::More qw(no_plan);
use Test::More tests => 68;

use_ok("Encode::MIME::Name");
for my $canon ( sort keys %Encode::MIME::Name::MIME_NAME_OF ) {
    my $enc       = find_encoding($canon);
    my $mime_name = $Encode::MIME::Name::MIME_NAME_OF{$canon};
    is $enc->mime_name, $mime_name,
      qq(\$enc->mime_name("$canon") eq $mime_name);
}

__END__;
