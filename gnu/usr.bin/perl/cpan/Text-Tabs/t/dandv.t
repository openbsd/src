
use Text::Wrap;
use Test::More tests => 2;
$Text::Wrap::columns = 4;
eval { $x = Text::Wrap::wrap('', '123', 'some text'); };
is($@, '');
is($x, "some\n123t\n123e\n123x\n123t");

