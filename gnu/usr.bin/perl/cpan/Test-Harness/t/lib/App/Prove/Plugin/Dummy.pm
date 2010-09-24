package App::Prove::Plugin::Dummy;

use strict;

sub import {
    main::test_log_import(@_);
}

1;
