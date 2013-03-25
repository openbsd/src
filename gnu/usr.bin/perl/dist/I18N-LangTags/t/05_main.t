use strict;
use Test::More tests => 64;
BEGIN {use_ok('I18N::LangTags', ':ALL');}

note("Perl v$], I18N::LangTags v$I18N::LangTags::VERSION");

foreach (['', 0],
	 ['fr', 1],
	 ['fr-ca', 1],
	 ['fr-CA', 1],
	 ['fr-CA-', 0],
	 ['fr_CA', 0],
	 ['fr-ca-joal', 1],
	 ['frca', 0],
	 ['nav', 1, 'not actual tag'],
	 ['nav-shiprock', 1, 'not actual tag'],
	 ['nav-ceremonial', 0, 'subtag too long'],
	 ['x', 0],
	 ['i', 0],
	 ['i-borg', 1, 'fictitious tag'],
	 ['x-borg', 1],
	 ['x-borg-prot5123', 1],
	) {
    my ($tag, $expect, $note) = @$_;
    $note = $note ? " # $note" : '';
    is(is_language_tag($tag), $expect, "is_language_tag('$tag')$note");
}
is(same_language_tag('x-borg-prot5123', 'i-BORG-Prot5123'), 1);
is(same_language_tag('en', 'en-us'), 0);

is(similarity_language_tag('en-ca', 'fr-ca'), 0);
is(similarity_language_tag('en-ca', 'en-us'), 1);
is(similarity_language_tag('en-us-southern', 'en-us-western'), 2);
is(similarity_language_tag('en-us-southern', 'en-us'), 2);

ok grep $_ eq 'hi', panic_languages('kok');
ok grep $_ eq 'en', panic_languages('x-woozle-wuzzle');
ok ! grep $_ eq 'mr', panic_languages('it');
ok grep $_ eq 'es', panic_languages('it');
ok grep $_ eq 'it', panic_languages('es');


note("Now the ::List tests...");
note("# Perl v$], I18N::LangTags::List v$I18N::LangTags::List::VERSION");

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
 ak
 aka
 jv
 jw
 no
 no-nyn
 nn
 i-lux
 lb
 wa
 yi
 ji
 den-syllabic
 den-syllabic-western
 den-western
 den-latin
 cre-syllabic
 cre-syllabic-western
 cre-western
 cre-latin
 cr-syllabic
 cr-syllabic-western
 cr-western
 cr-latin
)) {
  my $name = I18N::LangTags::List::name($lt);
  isnt($name, undef, "I18N::LangTags::List::name('$lt')");
}
