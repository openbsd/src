#!/usr/local/bin/perl -w

# Test ability to escape() and unescape() punctuation characters
# except for qw(- . _).

$| = 1;

use Test::More tests => 57;
use Config;
use_ok ( 'CGI::Util', qw(escape unescape) );

# ASCII order, ASCII codepoints, ASCII repertoire

my %punct = (
    ' ' => '20',  '!' => '21',  '"' => '22',  '#' =>  '23', 
    '$' => '24',  '%' => '25',  '&' => '26',  '\'' => '27', 
    '(' => '28',  ')' => '29',  '*' => '2A',  '+' =>  '2B', 
    ',' => '2C',                              '/' =>  '2F',  # '-' => '2D',  '.' => '2E' 
    ':' => '3A',  ';' => '3B',  '<' => '3C',  '=' =>  '3D', 
    '>' => '3E',  '?' => '3F',  '[' => '5B',  '\\' => '5C', 
    ']' => '5D',  '^' => '5E',                '`' =>  '60',  # '_' => '5F',
    '{' => '7B',  '|' => '7C',  '}' => '7D',  # '~' =>  '7E', 
         );

# The sort order may not be ASCII on EBCDIC machines:

my $i = 1;

foreach(sort(keys(%punct))) { 
    $i++;
    my $escape = "AbC\%$punct{$_}dEF";
    my $cgi_escape = escape("AbC$_" . "dEF");
    is($escape, $cgi_escape , "# $escape ne $cgi_escape");
    $i++;
    my $unescape = "AbC$_" . "dEF";
    my $cgi_unescape = unescape("AbC\%$punct{$_}dEF");
    is($unescape, $cgi_unescape , "# $unescape ne $cgi_unescape");
}

