use strict;
use Test;
BEGIN {
    use Config;
    if ($Config::Config{'extensions'} !~ /\bEncode\b/) {
      print "1..0 # Skip: Encode was not built\n";
      exit 0;
    }

    plan tests => 3;
}

{
    use encoding::warnings;
    ok(encoding::warnings->VERSION);

    if ($] < 5.009004) {
        ok('skipped');
        ok('skipped');
        exit;
    }

    my ($a, $b, $c, $warned);

    local $SIG{__WARN__} = sub {
        if ($_[0] =~ /upgraded/) { $warned = 1 }
    };

    utf8::encode($a = chr(20000));
    $b = chr(20000);
    $c = $a . $b;
    ok($warned);
}

{
    my ($a, $b, $c, $warned);

    local $SIG{__WARN__} = sub {
        if ($_[0] =~ /upgraded/) { $warned = 1 }
    };

    utf8::encode($a = chr(20000));
    $b = chr(20000);
    $c = $a . $b;
    ok(!$warned);
}


__END__
