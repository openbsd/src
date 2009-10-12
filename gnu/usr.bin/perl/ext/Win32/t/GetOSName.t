use strict;
use Test;
use Win32;

my @tests = (
    #              $id, $major, $minor, $pt, $build, $tag
    [ "WinWin32s",   0                                  ],
    [ "Win95",       1, 4,  0                           ],
    [ "Win95",       1, 4,  0,  0, 67109814, "(a)"      ],
    [ "Win95",       1, 4,  0,  0, 67306684, "(b1)"     ],
    [ "Win95",       1, 4,  0,  0, 67109975, "(b2)"     ],
    [ "Win98",       1, 4, 10                           ],
    [ "Win98",       1, 4, 10,  0, 67766446, "(2nd ed)" ],
    [ "WinMe",       1, 4, 90                           ],
    [ "WinNT3.51",   2, 3, 51                           ],
    [ "WinNT4",      2, 4,  0                           ],
    [ "Win2000",     2, 5,  0                           ],
    [ "WinXP/.Net",  2, 5,  1                           ],
    [ "Win2003",     2, 5,  2                           ],
    [ "WinVista",    2, 6,  0,  1                       ],
    [ "Win2008",     2, 6,  0,  2                       ],
    [ "Win7",        2, 6,  1                           ],
);

plan tests => 2*scalar(@tests) + 1;

# Test internal implementation function
for my $test (@tests) {
    my($expect, $id, $major, $minor, $pt, $build, $tag) = @$test;
    my($os, $desc) = Win32::_GetOSName("", $major, $minor, $build||0, $id, $pt);
    ok($os, $expect);
    ok($desc, $tag||"");
}

# Does Win32::GetOSName() return the correct value for the current OS?
my(undef, $major, $minor, $build, $id, undef, undef, undef, $pt)
    = Win32::GetOSVersion();
my($os, $desc) = Win32::_GetOSName("", $major, $minor, $build, $id, $pt);
ok(scalar Win32::GetOSName(), $os);
