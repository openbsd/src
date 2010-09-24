#!perl

use strict;
use warnings;

use Test::More tests => 18;

use CGI qw/ autoEscape escapeHTML button textfield password_field textarea popup_menu scrolling_list checkbox_group optgroup checkbox radio_group submit image_button button /;

is (button(-name => 'test<'), '<input type="button"  name="test&lt;" value="test&lt;" />', "autoEscape defaults to On");

my $before = escapeHTML("test<");
autoEscape(undef);
my $after = escapeHTML("test<");


is($before, "test&lt;", "reality check escapeHTML");

is ($before, $after, "passing undef to autoEscape doesn't break escapeHTML"); 
is (button(-name => 'test<'), '<input type="button"  name="test<" value="test<" />', "turning off autoescape actually works");
autoEscape(1);
is (button(-name => 'test<'), '<input type="button"  name="test&lt;" value="test&lt;" />', "autoescape turns back on");
$before = escapeHTML("test<");
autoEscape(0);
$after = escapeHTML("test<");

is ($before, $after, "passing 0 to autoEscape doesn't break escapeHTML"); 

# RT #25485: Needs Tests: autoEscape() bypassed for Javascript handlers, except in button()
autoEscape(undef);
 
is(textfield(
{
default => 'text field',
onclick => 'alert("===> text field")',
},
),
qq{<input type="text" name="" value="text field" onclick="alert("===> text field")" />},
'autoescape javascript turns off for textfield'
);

is(password_field(
{
default => 'password field',
onclick => 'alert("===> password
field")',
},
),
qq{<input type="password" name="" value="password field" onclick="alert("===> password
field")" />},
'autoescape javascript turns off for password field'
);

is(textarea(
{
name => 'foo',
default => 'text area',
rows => 10,
columns => 50,
onclick => 'alert("===> text area")',
},
),
qq{<textarea name="foo"  rows="10" cols="50" onclick="alert("===> text area")">text area</textarea>},
'autoescape javascript turns off for textarea'
);

is(popup_menu(
{
name => 'menu_name',
values => ['eenie','meenie','minie'],
default => 'meenie',
onclick => 'alert("===> popup menu")',
}
),
qq{<select name="menu_name"  onclick="alert("===> popup menu")">
<option value="eenie">eenie</option>
<option selected="selected" value="meenie">meenie</option>
<option value="minie">minie</option>
</select>},
'autoescape javascript turns off for popup_menu'
);

is(popup_menu(
-name=>'menu_name',
onclick => 'alert("===> menu group")',
-values=>[
qw/eenie meenie minie/,
optgroup(
-name=>'optgroup_name',
onclick =>
'alert("===> menu group option")',
-values => ['moe','catch'],
-attributes=>{'catch'=>{'class'=>'red'}}
)
],
-labels=>{
'eenie'=>'one',
'meenie'=>'two',
'minie'=>'three'
},
-default=>'meenie'
),
qq{<select name="menu_name"  onclick="alert("===> menu group")">
<option value="eenie">one</option>
<option selected="selected" value="meenie">two</option>
<option value="minie">three</option>
<optgroup label="optgroup_name" onclick="alert("===> menu group option")">
<option value="moe">moe</option>
<option class="red" value="catch">catch</option>
</optgroup>
</select>},
'autoescape javascript turns off for popup_menu #2'
);

is(scrolling_list(
-name=>'list_name',
onclick => 'alert("===> scrolling
list")',
-values=>['eenie','meenie','minie','moe'],
-default=>['eenie','moe'],
-size=>5,
-multiple=>'true',
),
qq{<select name="list_name"  size="5" multiple="multiple" onclick="alert("===> scrolling
list")">
<option selected="selected" value="eenie">eenie</option>
<option value="meenie">meenie</option>
<option value="minie">minie</option>
<option selected="selected" value="moe">moe</option>
</select>},
'autoescape javascript turns off for scrolling list'
);

is(checkbox_group(
-name=>'group_name',
onclick => 'alert("===> checkbox group")',
-values=>['eenie','meenie','minie','moe'],
-default=>['eenie','moe'],
-linebreak=>'true',
),
qq{<label><input type="checkbox" name="group_name" value="eenie" checked="checked" onclick="alert("===> checkbox group")" />eenie</label><br /> <label><input type="checkbox" name="group_name" value="meenie" onclick="alert("===> checkbox group")" />meenie</label><br /> <label><input type="checkbox" name="group_name" value="minie" onclick="alert("===> checkbox group")" />minie</label><br /> <label><input type="checkbox" name="group_name" value="moe" checked="checked" onclick="alert("===> checkbox group")" />moe</label><br />},
'autoescape javascript turns off for checkbox group'
);

is(checkbox(
-name=>'checkbox_name',
onclick => 'alert("===> single checkbox")',
onchange => 'alert("===> single checkbox
changed")',
-checked=>1,
-value=>'ON',
-label=>'CLICK ME'
),
qq{<label><input type="checkbox" name="checkbox_name" value="ON" checked="checked" onchange="alert("===> single checkbox
changed")" onclick="alert("===> single checkbox")" />CLICK ME</label>},
'autoescape javascript turns off for checkbox'
);

is(radio_group(
{
name=>'group_name',
onclick => 'alert("===> radio group")',
values=>['eenie','meenie','minie','moe'],
rows=>2,
columns=>2,
}
),
qq{<table><tr><td><label><input type="radio" name="group_name" value="eenie" checked="checked" onclick="alert("===> radio group")" />eenie</label></td><td><label><input type="radio" name="group_name" value="minie" onclick="alert("===> radio group")" />minie</label></td></tr><tr><td><label><input type="radio" name="group_name" value="meenie" onclick="alert("===> radio group")" />meenie</label></td><td><label><input type="radio" name="group_name" value="moe" onclick="alert("===> radio group")" />moe</label></td></tr></table>},
'autoescape javascript turns off for radio group'
);

is(submit(
-name=>'button_name',
onclick => 'alert("===> submit button")',
-value=>'value'
),
qq{<input type="submit" name="button_name" value="value" onclick="alert("===> submit button")" />},
'autoescape javascript turns off for submit'
);

is(image_button(
-name=>'button_name',
onclick => 'alert("===> image button")',
-src=>'/source/URL',
-align=>'MIDDLE'
),
qq{<input type="image" name="button_name" src="/source/URL" align="middle" onclick="alert("===> image button")" />},
'autoescape javascript turns off for image_button'
);

is(button(
{
onclick => 'alert("===> Button")',
title => 'Button',
},
),
qq{<input type="button"  onclick="alert("===> Button")" title="Button" />},
'autoescape javascript turns off for button'
);
