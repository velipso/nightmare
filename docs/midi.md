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
synthesized into sound.  For example, there is a Note On event, which indicates that a key has been
pressed, and a Note Off event when it's released.

The MIDI protocol is designed to be used in real-time -- as the musician is generating events with
the keyboard, the data is being transmitted over the wire, which a synthesizer can react to in
real-time by outputting sound.

In order to store a stream of MIDI events in a file, events are timestamped.  This allows a
program to "play back" the stream of events at the correct time.

The bulk of the complexity is understanding the events themselves.  The MIDI protocol has maintained
backwards compatibility, which means it's easy to see the raw bytes that make up a event, but
understanding how to unpack and interpret each event is complicated.

General MIDI 2 is an additional layer of requirements on top of the basic MIDI protocol that further
specifies the meaning behind events, and systems/devices claiming to support GM 2 are required to
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
files have junk data between chunks (for example, a string of repeated bytes like `00 00 00 ...`),
or data that *looks* useful, but I can't figure out how to decode it.  The best strategy is probably
to start searching 7 bytes backwards (in case the previous track reported too large a Data Size),
and search forward looking for known Chunk Types until the end of the file.

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

| Name         | Size                 | Description                        |
|--------------|----------------------|------------------------------------|
| Chunk Type   | 4 bytes              | `'MThd'` (`4D 54 68 64`)           |
| Data Size    | 4 bytes (big endian) | 6 (`00 00 00 06`)                  |
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

1. If bit 15 is cleared, then bits 14-0 represent the ticks per quarter note
2. If bit 15 is set, then bits 14-8 represent the negative SMPTE format, and bits 7-0 represent the
   ticks per frame

In practice, no files have bit 15 set (other than `ffmqbatl.mid` which has it set due to a corrupted
header).  If someone can find me some test MIDI files that use SMPTE timing, then I would love to
look into it.  Otherwise, I will just reject files that have bit 15 set.

Figuring out the timing of the events is, unfortunately, not straight forward.  There is an entire
section on calculating timing below.

### `'MThd'` Statistics

* One file (`candyluv.mid`) has a header with Data Size set to 10 (`00 00 00 0A`)
* Distribution of Formats:
    * Format 0: 13.5% (6529 of 48216)
    * Format 1: 86.4% (41662 of 48216)
    * Format 2: 0.05% (23 of 48216)
    * Bad Format: 0.004% (2 of 48216, `boythorn.mid` has `74 01` and `possible.mid` has `70 01`)
* 0.74% of files have an incorrect Track Chunks count (359 of 48197)
* Division values range from 2 to 25000, with the most common values being:
    * 120: 28.9% (13948 of 48213)
    * 480: 16.2% (7809 of 48213)
    * 192: 16.1% (7746 of 48213)
    * 384: 15.2% (7316 of 48213)
    * 240: 10.2% (4939 of 48213)
    * 96: 9.9% (4770 of 48213)

Track Chunk `'MTrk'`
--------------------

Track chunks consist of a stream of timestamped events.

| Name        | Size                 | Description                                                |
|-------------|----------------------|------------------------------------------------------------|
| Chunk Type  | 4 bytes              | `'MTrk'` (`4D 54 72 6B`)                                   |
| Data Size   | 4 bytes (big endian) | Length of the data (not including Chunk Type of Data Size) |
| MTrk Events | Data Size            | One or more MTrk events                                    |

A single MTrk event consists of a delta timestamp and an event:

| Name            | Size         | Description                                                    |
|-----------------|--------------|----------------------------------------------------------------|
| Delta Timestamp | Variable Int | Number of ticks this event occurs relative to previous event   |
| Event           | Varies according to event | Channel Message, SysEx Event, or Meta Event       |

### Variable Int Quantities

Some MTrk events contain Variable Int quantities, like the Delta Timestamp.  This is a 32-bit
integer with a maximum value of `0FFFFFFF` (*not* `FFFFFFFF`).  It is encoded 7 bits per byte,
most significant bits first, where the most significant bit of each encoded byte is reserved to
indicate whether more bytes need to be read.

The encoding ranges from 1 to 4 bytes, with the following binary decodings:

| Encoding                              | Decoding                              |
|---------------------------------------|---------------------------------------|
| `0aaaaaaa`                            | `00000000 00000000 00000000 0aaaaaaa` |
| `1aaaaaaa 0bbbbbbb`                   | `00000000 00000000 00aaaaaa abbbbbbb` |
| `1aaaaaaa 1bbbbbbb 0ccccccc`          | `00000000 000aaaaa aabbbbbb bccccccc` |
| `1aaaaaaa 1bbbbbbb 1ccccccc 0ddddddd` | `0000aaaa aaabbbbb bbcccccc cddddddd` |

Examples:

| Encoding      | Decoding   |
|---------------|------------|
| `00`          | `00000000` |
| `67`          | `00000067` |
| `7F`          | `0000007F` |
| `81 00`       | `00000080` |
| `C6 45`       | `00002345` |
| `FF 7F`       | `00003FFF` |
| `81 80 00`    | `00004000` |
| `C8 E8 56`    | `00123456` |
| `FF FF 7F`    | `001FFFFF` |
| `81 80 80 00` | `00200000` |
| `C4 EA F9 5E` | `089ABCDE` |
| `FF FF FF 7F` | `0FFFFFFF` |

### Channel Messages

Channel Messages start with a single byte in the range `80` to `EF`.  The lower 4 bits represent
the channel `n` (0-15 are usually displayed as 1-16), and the higher 4 bits represent the message
type.  For example, `94` is a Note On message for channel 5.

| Message | Parameter 1             | Parameter 2                     | Description               |
|---------|-------------------------|---------------------------------|---------------------------|
| `8n`    | Note (`00` to `7F`)     | Release Velocity (`00` to `7F`) | Note Off                  |
| `9n`    | Note (`00` to `7F`)     | Hit Velocity (`00` to `7F`)     | Note On                   |
| `An`    | Note (`00` to `7F`)     | Pressure (`00` to `7F`)         | Note Pressure             |
| `Bn`    | Control (`00` to `7F`)  | Value (`00` to `7F`)            | Control Change            |
| `Cn`    | Patch (`00` to `7F`)    | N/A                             | Program Change            |
| `Dn`    | Pressure (`00` to `7F`) | N/A                             | Channel Pressure          |
| `En`    | Bend Change LSB (`00` to `7F`) | Bend Change MSB (`00` to `7F`) | Pitch Bend          |

```
(Control Change Status) BnH
00-77 bunch of stuff
78 00 All Sound Off
79 00 Reset All Controllers
7A 00 off, 7F on Local Control
7B 00 All Notes Off
7C 00 Omni Off
7D 00 Omni On
7E MM (number of channels, 00 for all) Mono On (Poly Off)
7F 00 Poly On (Mono Off)
```

#### Running Status

TODO: running status only applies to Channel Messages

### SysEx Event

`F0`, `F7`

TODO

### Meta Event

Meta Events are always at least 3 bytes in length.

The first byte is `FF` which is what identifies the event as a Meta Event.

The second byte determines the type of meta event.

The third byte determines the byte length of the remaining parameters.  This is useful because you
don't need to understand the meta event type in order to skip over the event.

Any parameters spanning multiple bytes should be interpreted as big endian.

| Byte Sequence       | Description                                                               |
|---------------------|---------------------------------------------------------------------------|
| `FF 00 02 SS SS`    | Sequence Number `SSSS`                                                    |
| `FF 01 LL text`     | Generic Text of length `LL`                                               |
| `FF 02 LL text`     | Copyright Notice of length `LL`                                           |
| `FF 03 LL text`     | Track Name of length `LL`                                                 |
| `FF 04 LL text`     | Instrument Name of length `LL`                                            |
| `FF 05 LL text`     | Lyric of length `LL`                                                      |
| `FF 06 LL text`     | Marker of length `LL` ('First Verse', 'Chrous', etc)                      |
| `FF 07 LL text`     | Cue Point of length `LL` ('curtain opens', 'character is slapped', etc)   |
| `FF 20 01 NN`       | Channel Prefix, select channel `NN` (0-15) for future SysEx/Meta events   |
| `FF 21 01 PP`       | MIDI Port, select output port `PP` for MIDI events                        |
| `FF 2E ?? ????`     | Track Loop Event (?)                                                      |
| `FF 2F 00`          | End of Track, required as last event in a MTrk chunk                      |
| `FF 51 03 TT TT TT` | Set Tempo, in `TTTTTT` microseconds per MIDI quarter-note                 |
| `FF 54 05 HH MM SS RR TT` | SMPTE Offset, hour, minute, second, frame, 1/100th of frame         |
| `FF 58 04 NN MM LL TT` | Time Signature, see below for details                                  |
| `FF 59 02 SS MM`    | Key Signature, see below for details                                      |
| `FF 7F LL data`     | Sequencer-Specific Meta Event, see below for details                      |

#### Key Signature Meta Event

TODO

#### Sequencer-Specific Meta Event

TODO

### Calculating Timing

The MIDI specification has a lot of ways to represent timing.

The unit of the Division parameter in the header is *ticks per quarter-note*.

The unit of the Delta Timestamps is *ticks* passed since previous event.

The unit of the Set Tempo Meta Event is *microseconds per quarter-note*.

If Set Tempo is not set, then the default value is 120 *beats per minute*.

The Time Signature Meta Event's denominator (`MM`) represents *quarter-notes per beat* in a goofy
way:

| `MM` | Time Signature Denominator | Quarter-Notes per Beat |
|------|----------------------------|------------------------|
| `00` | 1                          | 4                      |
| `01` | 2                          | 2                      |
| `02` | 4                          | 1                      |
| `03` | 8                          | 1/2                    |
| `04` | 16                         | 1/4                    |
| `MM` | 2<sup>`MM`</sup>           | 2<sup>2 - `MM`</sup>   |
