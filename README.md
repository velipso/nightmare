nightmare
=========

Audio engine for games based on synthesizing music and sound effects.

This engine powers [Spook Show Studio](https://spookshow.studio).  So, if you write cool music using
Spook Show Studio, you can export it for use in your own games.

**Under active development.**

Dependencies
============

Requires [`opus`, `opusfile`](https://opus-codec.org/downloads/),
and [`libogg`](https://xiph.org/downloads/).  Currently tested with:

* `opus` v1.3.1
* `opusfile` v0.12
* `libogg` v1.3.3

Tech Info
=========

* Channels: 2 (left + right)
* Bit depth: 32 (float)
* Sample rate: 48 kHz
* No dynamic memory (malloc/free)
