
require 5;
 # Time-stamp: "2003-10-10 17:37:34 ADT"
use strict;
use Test;
BEGIN { plan tests => 17 };
BEGIN { ok 1 }
use I18N::LangTags::List;

print "# Perl v$], I18N::LangTags::List v$I18N::LangTags::List::VERSION\n";

ok  I18N::LangTags::List::name('fr'), 'French';
ok  I18N::LangTags::List::name('fr-fr');
ok !I18N::LangTags::List::name('El Zorcho');
ok !I18N::LangTags::List::name();


ok !I18N::LangTags::List::is_decent();
ok  I18N::LangTags::List::is_decent('fr');
ok  I18N::LangTags::List::is_decent('fr-blorch');
ok !I18N::LangTags::List::is_decent('El Zorcho');
ok !I18N::LangTags::List::is_decent('sgn');
ok  I18N::LangTags::List::is_decent('sgn-us');
ok !I18N::LangTags::List::is_decent('i');
ok  I18N::LangTags::List::is_decent('i-mingo');
ok  I18N::LangTags::List::is_decent('i-mingo-tom');
ok !I18N::LangTags::List::is_decent('cel');
ok  I18N::LangTags::List::is_decent('cel-gaulish');

ok 1; # one for the road
