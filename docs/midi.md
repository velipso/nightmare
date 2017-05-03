MIDI File Format
================

This document is my attempt at simplifying and clairifying the MIDI file format.

I've used three primary sources of information:

1. [Official MIDI specification](https://midi.org)
2. [Yamaha XF Format specification](http://www.jososoft.dk/yamaha//pdf/xfspec.pdf)
3. Real-world MIDI files

My intention is to document what a MIDI file *should* look like (for encoders), but also what
real-world data looks like (for lenient decoders).

Real-World Data
---------------

Thankfully, there exists a large body of real-world MIDI files to inspect:

[The Geocities MIDI Collection (2009)](https://archive.org/details/archiveteam-geocities-midi-collection-2009)

This archive has 48,197 MIDI files, totalling 1.3 GB of data.

The data also happens to be pretty damn dirty too -- which helps us in figuring out good strategies
for recovering useful information.

MIDI Overview
-------------

MIDI is a protocol for transmitting note and controller information across a serial connection.  A
good mental model is to think of MIDI as the data that comes off a music keyboard *before* being
synthesized into sound.  For example, there is a Note On message, which indicates that a key has
been pressed, and a Note Off message when it's released.

The MIDI protocol is designed to be used in real-time -- as the musician is generating events with
the keyboard, the data is being transmitted over the wire, which a synthesizer can react to in
real-time by outputting sound.

In order to store a stream of MIDI messages in a file, messages are timestamped.  This allows a
program to "play back" the stream of messages at the correct time.

The bulk of the complexity is understanding the messages themselves.  The MIDI protocol has
maintained backwards compatibility, which means it's easy to see the raw bytes that make up a
message, but understanding how to unpack and interpret each message is complicated.

General MIDI 2 is an additional layer of requirements on top of the basic MIDI protocol that further
specifies the meaning behind messages, and systems/devices claiming to support GM 2 are required to
support a minimum feature set.

Chunks
------

MIDI files consist of a stream of chunks.  Each chunk has the format:

| Name       | Size                 | Description                                                 |
|------------|----------------------|-------------------------------------------------------------|
| Chunk Type | 4 bytes              | Format of the chunk, typically either `'MThd'` or `'MTrk'`  |
| Data Size  | 4 bytes (big endian) | Length of the data (not including Chunk Type of Data Size)  |
| Data       | Specified by Data Size | Raw data for the chunk, interpreted based on Chunk Type   |

A MIDI file start with a `'MThd'` chunk, followed by one or more `'MTrk'` chunks.  I've also
seen `'XFIH'` and `'XFKM'` chunks, which after searching online, shows they are extensions by
Yamaha for karaoke.

Chunks *should* be immediately one after another, and most files follow that rule -- however, a few
files have junk data between chunks (for example, a string of repeated bytes like `00 00 00 ...`).
A robust decoder might want to search the area around the end of a chunk for the next chunk,
skipping over repeated byte data, including searching backwards in case the previous chunk reported
a Data Size too large.

The chunk stream starts with `'MThd'`, and *should* only contain `'MThd'` once.  In practice, all
MIDI files I've seen start with `'MThd'`, but some also have `'MThd'` chunks in the middle of the
file.  I suspect this should be treated like two MIDI files have been concatenated together into
one.

The largest Data Size I've seen is 362,528 bytes (`dou_01.mid`), which is 45 minutes of dense piano.
A good sanity check might be to make sure the Data Size isn't larger than, say, 1,000,000 bytes,
otherwise you're probably reading bad data.

### Chunk Statistics

The real-world data contains:

* 48,197 MIDI files, all of them starting with an `'MThd'` chunk
* 539,469 total recognizable chunks inside those files
    * 124 of all chunks were misaligned and found by searching near where a chunk should start
    * 19 of all chunks were `'MThd'` chunks in the middle of a file
* 1,767 files had garbage data at the end that couldn't be recognized as a chunk

Header Chunk `'MThd'`
---------------------

The header chunk has a type of `'MThd'` (`4D 54 68 64`).  The size should always be 6 bytes long
(i.e., Data Size is `00 00 00 06`).

| Name         | Size                 | Description                        |
|--------------|----------------------|------------------------------------|
| Format       | 2 bytes (big endian) | `0`, `1`, or `2`, described below  |
| Track Chunks | 2 bytes (big endian) | Number of track chunks in the file |
| Division     | 2 bytes (big endian) | Timestamp format, described below  |

### Format

The Format can either be `00 00`, `00 01`, or `00 02`, each with the following meanings:

| Format  | Description                                                                           |
|---------|---------------------------------------------------------------------------------------|
| `00 00` | The file contains a single multi-channel track (so Track Chunks should be `00 01`)    |
| `00 01` | The file contains one or more tracks that should be played simultaneously             |
| `00 02` | The file contains one or more tracks that are independant of each other               |

### Track Chunks

The Track Chunks should be a count of `'MTrk'` chunks in the file.  In practice, this number is
sometimes wrong.  A decoder should probably just ignore it, because the `'MTrk'` chunks can be
counted directly.

When multiple `'MThd'` chunks are in a file (which is against the specification), sometimes the
Track Count represents the tracks up until that point, and sometimes they represent the total number
of track chunks in the entire file.  And sometimes it's just completely wrong.  Yet another reason
to ignore the field.

### Division

According to the specification, the division can come in two formats:

<table>
	<thead>
		<tr>
			<th>&nbsp;</th>
			<th>15</th>
			<th>13</th>
			<th>13</th>
			<th>12</th>
			<th>11</th>
			<th>10</th>
			<th>9</th>
			<th>8</th>
			<th>7</th>
			<th>6</th>
			<th>5</th>
			<th>4</th>
			<th>3</th>
			<th>2</th>
			<th>1</th>
			<th>0</th>
		</tr>
	</thead>
	<tbody>
		<tr>
			<td style="text-align: center;">Ticks</td>
			<td style="text-align: center;">0</td>
			<td colspan="15" style="text-align: center;">Ticks per Quarter Note</td>
		</tr>
		<tr>
			<td style="text-align: center;">SMPTE</td>
			<td style="text-align: center;">1</td>
			<td colspan="7" style="text-align: center;">Negative SMPTE Format</td>
			<td colspan="8" style="text-align: center;">Ticks per Frame</td>
		</tr>
	</tbody>
</table>

### `'MThd'` Statistics

* One file (`candyluv.mid`) has a header with Data Size set to 10 (`00 00 00 0A`)
* Distribution of Formats:
    * Format 0: 13.5% (6529 of 48216)
    * Format 1: 86.4% (41662 of 48216)
    * Format 2: 0.05% (23 of 48216)
    * Bad Format: 0.004% (2 of 48216, `boythorn.mid` has `74 01` and `possible.mid` has `70 01`)
* 0.74% of files have an incorrect Track Chunks count (359 of 48197)

Track Chunk `'MTrk'`
--------------------

TODO: MORE
