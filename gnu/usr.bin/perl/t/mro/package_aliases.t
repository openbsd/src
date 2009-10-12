#!./perl

BEGIN {
    unless (-d 'blib') {
        chdir 't' if -d 't';
        @INC = '../lib';
    }
}

use strict;
use warnings;
require q(./test.pl); plan(tests => 4);

{
    package New;
    use strict;
    use warnings;

    package Old;
    use strict;
    use warnings;

    {
      no strict 'refs';
      *{'Old::'} = *{'New::'};
    }
}

ok (Old->isa (New::), 'Old inherits from New');
ok (New->isa (Old::), 'New inherits from Old');

isa_ok (bless ({}, Old::), New::, 'Old object');
isa_ok (bless ({}, New::), Old::, 'New object');
