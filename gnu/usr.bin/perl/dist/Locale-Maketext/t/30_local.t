#!/usr/bin/perl -Tw

use strict;

use Test::More tests => 4;
use Locale::Maketext;
print "# Hi there...\n";
pass();

print "# --- Making sure that Perl globals are localized ---\n";

# declare a class...
{
  package Woozle;
  our @ISA = ('Locale::Maketext');
  our %Lexicon = (
    _AUTO => 1
  );
  keys %Lexicon; # dodges the 'used only once' warning
}

my $lh = Woozle->new();
ok(ref $lh, 'Basic sanity');

$@ = 'foo';
is($lh->maketext('Eval error: [_1]', $@), 'Eval error: foo',
  'Make sure $@ is localized');

print "# Byebye!\n";
pass();
