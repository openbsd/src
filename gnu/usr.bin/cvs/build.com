$ set def [.zlib]
$ @build_zlib.com
$ set def [-.diff]
$ @build_diff.com
$ set def [-.vms]
$ @build_vms.com
$ set def [-.lib]
$ @build_lib.com
$ set def [-.src]
$ @build_src.com
