require 5;
 # Time-stamp: "2004-10-06 23:07:06 ADT"
use strict;
use Test;
BEGIN { plan tests => 22 };
BEGIN { ok 1 }
use I18N::LangTags (':ALL');

print "# Perl v$], I18N::LangTags v$I18N::LangTags::VERSION\n";
print "#  Loaded from ", $INC{'I18N/LangTags.pm'} || "??", "\n";

ok lc locale2language_tag('en'),    'en';
ok lc locale2language_tag('en_US'),    'en-us';
ok lc locale2language_tag('en_US.ISO8859-1'),    'en-us';
ok lc(locale2language_tag('C')||''),    '';
ok lc(locale2language_tag('POSIX')||''), '';


ok lc locale2language_tag('eu_mt'),           'eu-mt';
ok lc locale2language_tag('eu'),              'eu';
ok lc locale2language_tag('it'),              'it';
ok lc locale2language_tag('it_IT'),           'it-it';
ok lc locale2language_tag('it_IT.utf8'),      'it-it';
ok lc locale2language_tag('it_IT.utf8@euro'), 'it-it';
ok lc locale2language_tag('it_IT@euro'),      'it-it';


ok lc locale2language_tag('zh_CN.gb18030'), 'zh-cn';
ok lc locale2language_tag('zh_CN.gbk'),     'zh-cn';
ok lc locale2language_tag('zh_CN.utf8'),    'zh-cn';
ok lc locale2language_tag('zh_HK'),         'zh-hk';
ok lc locale2language_tag('zh_HK.utf8'),    'zh-hk';
ok lc locale2language_tag('zh_TW'),         'zh-tw';
ok lc locale2language_tag('zh_TW.euctw'),   'zh-tw';
ok lc locale2language_tag('zh_TW.utf8'),    'zh-tw';

print "# So there!\n";
ok 1;
