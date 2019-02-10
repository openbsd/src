This is a list of possible work projects within openrsync, rated by difficulty.

First, porting: see
[Porting](https://github.com/kristapsdz/openrsync/blob/master/README.md#Portability)
for information on this topic.
I've included the specific security porting topics below.

This list also does not include adding support for features (e.g., **-u** and
so on).

- Easy: add a hashtable to `blk_find()` in
  [blocks.c](https://github.com/kristapsdz/openrsync/blob/master/blocks.c)
  for quickly looking up fast-hash matches.

- Easy: print more statistics, such as transfer times and rates.

- Easy: tighten the [pledge(2)](https://man.openbsd.org/pledge.2) and
  [unveil(2)](https://man.openbsd.org/unveil.2) to work with **-n**, as
  it does not touch files.

- Easy: find the shared path for all input files and
  [unveil(2)](https://man.openbsd.org/unveil.2) only the shared path
  instead of each one.

- Medium: have the log messages when multiplex writing (server mode) is
  enabled by flushed out through the multiplex channel.
  Right now, they're emitted on `stderr` just like with the client.

- Medium: porting the security precautions
  ([unveil(2)](https://man.openbsd.org/unveil.2),
  [pledge(2)](https://man.openbsd.org/pledge.2)) to
  [FreeBSD](https://www.freebsd.org)'s
  [Capsicum](https://wiki.freebsd.org/Capsicum).
  Without this in place, you're exposing your file-system to whatever is
  coming down over the wire.
  This is certainly possible, as openrsync makes exclusive use of the "at"
  functions (e.g., [openat(2)](https://man.openbsd.org/openat.2)) for working
  with files.

- Hard: the same, but for Linux.

Above all, `grep FIXME *.c *.h` and start from there.
