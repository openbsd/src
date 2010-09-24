#!perl -w
use strict;
use FindBin;
use lib (($FindBin::Bin."/lib")=~/^(.*)$/);
use Test::More tests => 2;

my @packages = eval { Zot->_dist_types };
is($@, '',                "No warnings");
is(scalar(@packages), 0,  "Correctly only got 1 package");


package Zot;
use strict;
use Module::Pluggable (
        sub_name => '_dist_types',
        search_path => __PACKAGE__,
        only => qr/Zot::\w+$/,
        require => 1,
    );

1;
