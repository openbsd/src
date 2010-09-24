use Test::More tests => 4;
use CGI 'unescapeHTML';

is( unescapeHTML( '&amp;'), '&', 'unescapeHTML: &');
is( unescapeHTML( '&quot;'), '"', 'unescapeHTML: "');
is( unescapeHTML( '&#60;'), '<', 'unescapeHTML: < (using a numbered sequence)'); 
is( unescapeHTML( 'Bob & Tom went to the store; Where did you go?'), 
    'Bob & Tom went to the store; Where did you go?', 'unescapeHTML: a case where &...; should not be escaped.');
