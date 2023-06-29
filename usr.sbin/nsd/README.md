# NSD

[![Cirrus Build Status](https://api.cirrus-ci.com/github/NLnetLabs/nsd.svg?branch=master)](https://cirrus-ci.com/github/NLnetLabs/nsd)
[![Packaging status](https://repology.org/badge/tiny-repos/nsd.svg)](https://repology.org/project/nsd/versions)
[![CII Best Practices](https://bestpractices.coreinfrastructure.org/projects/1462/badge)](https://bestpractices.coreinfrastructure.org/projects/1462)
[![Mastodon Follow](https://img.shields.io/mastodon/follow/109262826617293067?domain=https%3A%2F%2Ffosstodon.org&style=social)](https://fosstodon.org/@nlnetlabs)

The NLnet Labs Name Server Daemon (NSD) is an authoritative DNS name server.
It has been developed for operations in environments where speed,
reliability, stability and security are of high importance.  If you
have any feedback, we would love to hear from you. Don’t hesitate to
[create an issue on Github](https://github.com/NLnetLabs/nsd/issues/new)
or post a message on the
[NSD mailing list](https://lists.nlnetlabs.nl/mailman/listinfo/nsd-users).
You can learn more about NSD by reading our
[documentation](https://nsd.docs.nlnetlabs.nl/).

## Compiling

Make sure you have the following installed:
  * C toolchain (the set of tools to compile C such as a compiler, linker, and assembler)
  * OpenSSL, with its include files (usually these are included in the "dev" version of the library)
  * libevent, with its include files (usually these are included in the "dev" version of the library)
  * flex
  * bison

The repository does not contain `./configure`, but you can generate it like
this (note that the `./configure` is included in release tarballs so they do not have to be generated):

```
autoreconf -fi
```

NSD can be compiled and installed using:

```
./configure && make && make install
```

## NSD configuration

The configuration options for NSD are described in the man pages, which are
installed (use `man nsd.conf`) and are available on the NSD
[documentation page](https://nsd.docs.nlnetlabs.nl/).

An example configuration file is located in
[nsd.conf.sample](https://github.com/NLnetLabs/nsd/blob/master/nsd.conf.sample.in).
