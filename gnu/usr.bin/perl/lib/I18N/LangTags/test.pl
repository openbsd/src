# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.
require 5;
 # Time-stamp: "2001-06-21 22:59:38 MDT"
use strict;
use Test;
BEGIN { plan tests => 46 };
BEGIN { ok 1 }
use I18N::LangTags (':ALL');

print "# Perl v$], I18N::LangTags v$I18N::LangTags::VERSION\n";

ok !is_language_tag('');
ok  is_language_tag('fr');
ok  is_language_tag('fr-ca');
ok  is_language_tag('fr-CA');
ok !is_language_tag('fr-CA-');
ok !is_language_tag('fr_CA');
ok  is_language_tag('fr-ca-joual');
ok !is_language_tag('frca');
ok  is_language_tag('nav');
ok  is_language_tag('nav-shiprock');
ok !is_language_tag('nav-ceremonial'); # subtag too long
ok !is_language_tag('x');
ok !is_language_tag('i');
ok  is_language_tag('i-borg'); # NB: fictitious tag
ok  is_language_tag('x-borg');
ok  is_language_tag('x-borg-prot5123');
ok  same_language_tag('x-borg-prot5123', 'i-BORG-Prot5123' );
ok !same_language_tag('en', 'en-us' );

ok 0 == similarity_language_tag('en-ca', 'fr-ca');
ok 1 == similarity_language_tag('en-ca', 'en-us');
ok 2 == similarity_language_tag('en-us-southern', 'en-us-western');
ok 2 == similarity_language_tag('en-us-southern', 'en-us');

ok grep $_ eq 'hi', panic_languages('kok');
ok grep $_ eq 'en', panic_languages('x-woozle-wuzzle');
ok ! grep $_ eq 'mr', panic_languages('it');
ok grep $_ eq 'es', panic_languages('it');
ok grep $_ eq 'it', panic_languages('es');


print "# Now the ::List tests...\n";
use I18N::LangTags::List;
foreach my $lt (qw(
 en
 en-us
 en-kr
 el
 elx
 i-mingo
 i-mingo-tom
 x-mingo-tom
 it
 it-it
 it-IT
 it-FR
 yi
 ji
 cre-syllabic
 cre-syllabic-western
 cre-western
 cre-latin
)) {
  my $name = I18N::LangTags::List::name($lt);
  if($name) {
    ok(1);
    print "#        $lt -> $name\n";
  } else {
    ok(0);
    print "#        Failed lookup on $lt\n";
  }
}

print "# So there!\n";

