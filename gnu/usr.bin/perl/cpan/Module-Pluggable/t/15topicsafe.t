#!perl -w

use strict;
use FindBin;
use lib (($FindBin::Bin."/lib")=~/^(.*)$/);
use Test::More 'no_plan';

use Module::Pluggable search_path => 'Acme::MyTest';

my $topic = "topic";

for ($topic) {
  main->plugins;
}

is($topic, 'topic', "we've got the right topic");
