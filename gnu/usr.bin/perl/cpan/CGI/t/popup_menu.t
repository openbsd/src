#!perl
# Tests for popup_menu();
use Test::More 'no_plan';
use CGI;

my $q  = CGI->new;

is ( $q->popup_menu(-name=>"foo", - values=>[0,1], -default=>0),
'<select name="foo" >
<option selected="selected" value="0">0</option>
<option value="1">1</option>
</select>'
, 'popup_menu(): basic test, including 0 as a default value');

is(
    CGI::popup_menu(-values=>[CGI::optgroup(-values=>["b+"])],-default=>"b+"),
    '<select name="" >
<optgroup label="">
<option selected="selected" value="b+">b+</option>
</optgroup>
</select>'
    , "<optgroup> selections work when the default values contain regex characters (RT#49606)"); 
