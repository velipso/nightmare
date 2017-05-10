MIDI File Format
================

This document is my attempt at simplifying and clairifying the MIDI file format.

I've used three primary sources of information:

1. [Official MIDI specifications](https://midi.org)
2. [Yamaha XF Format specification](http://www.jososoft.dk/yamaha//pdf/xfspec.pdf)
3. Real-world MIDI files

I also searched the Internet for random things I found in the files, to hopefully dig up some
additional context and information.

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

1. If bit 15 is cleared, then bits 14-0 represent the ticks per quarter-note
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

#### Notes

Converting bytes 0-127 to notes is based on the fact that note 69 (`45` hex) is A440.  Oddly, many
websites report the wrong octave for a given MIDI note value.  The table below assigns octaves
based on the fact that Middle C should be Octave 4.

Default tuning will be: Frequency = 440 &times; 2<sup>(Note - 69) / 12</sup>

| Octave |  C   |  C#  |  D   |  D#  |  E   |  F   |  F#  |  G   |  G#  |  A   |  A#  |  B   |
|:------:|:----:|:----:|:----:|:----:|:----:|:----:|:----:|:----:|:----:|:----:|:----:|:----:|
|  -1    | `00` | `01` | `02` | `03` | `04` | `05` | `06` | `07` | `08` | `09` | `0A` | `0B` |
|   0    | `0C` | `0D` | `0E` | `0F` | `10` | `11` | `12` | `13` | `14` | `15` | `16` | `17` |
|   1    | `18` | `19` | `1A` | `1B` | `1C` | `1D` | `1E` | `1F` | `20` | `21` | `22` | `23` |
|   2    | `24` | `25` | `26` | `27` | `28` | `29` | `2A` | `2B` | `2C` | `2D` | `2E` | `2F` |
|   3    | `30` | `31` | `32` | `33` | `34` | `35` | `36` | `37` | `38` | `39` | `3A` | `3B` |
|   4    | `3C` | `3D` | `3E` | `3F` | `40` | `41` | `42` | `43` | `44` | `45` | `46` | `47` |
|   5    | `48` | `49` | `4A` | `4B` | `4C` | `4D` | `4E` | `4F` | `50` | `51` | `52` | `53` |
|   6    | `54` | `55` | `56` | `57` | `58` | `59` | `5A` | `5B` | `5C` | `5D` | `5E` | `5F` |
|   7    | `60` | `61` | `62` | `63` | `64` | `65` | `66` | `67` | `68` | `69` | `6A` | `6B` |
|   8    | `6C` | `6D` | `6E` | `6F` | `70` | `71` | `72` | `73` | `74` | `75` | `76` | `77` |
|   9    | `78` | `79` | `7A` | `7B` | `7C` | `7D` | `7E` | `7F` | N/A  | N/A  | N/A  | N/A  |

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

The unit of the Delta Timestamps is *ticks* passed since previous event.  But what is a *tick*?
Specifically, we need to calculate *samples per tick*, so that when we see how many ticks until the
next event, we can move forward the appropriate amount of samples.

The unit of the Division parameter in the header is *ticks per quarter-note*.

The unit of the Set Tempo Meta Event (`TTTTTT`) is *microseconds per quarter-note*.  Reminder: there
are 1,000,000 *microseconds per second*.

If Set Tempo is not set, then the default value is 120 *beats per minute*.

The Time Signature Meta Event's denominator (`MM`) represents *quarter-notes per beat* in a goofy
way:

| `MM` | Time Signature Denominator | Quarter-Notes per Beat |
|------|----------------------------|------------------------|
| `00` | 1                          | 4                      |
| `01` | 2                          | 2                      |
| `02` | 4                          | 1                      |
| `03` | 8                          | <sup>1</sup>/<sub>2</sub> |
| `04` | 16                         | <sup>1</sup>/<sub>4</sub> |
| `MM` | 2<sup>`MM`</sup>           | 2<sup>2 - `MM`</sup>   |

If Time Signature is not set, the denominator defaults to 4, which represents 1 quarter-note per
beat.

The MIDI specification also talks about another unit, the *MIDI clock*, which is simply
<sup>1</sup>/<sub>24th</sub> the length of a quarter-note.

Lastly, Sample Rate (*samples per second*) is provided by the audio output format.

So with all this information, we can calculate the *samples per tick*:

We start assuming 120 *beats per minute* and 1 *quarter-note per beat*, therefore we have 120
*quarter-notes per minute*, or 2 *quarter-notes per second*.

When we receive the Division parameter from the header (*ticks per quarter-note*) and the Sample
Rate (*samples per second*) from the audio device, we can calculate *samples per tick* via:
Sample Rate / (Division &times; 2)

If we receive a Set Tempo event (`TTTTTT` *microseconds per quarter-note*), we can recalculate
*samples per tick* to be: (Sample Rate &times; `TTTTTT`) / (1,000,000 &times; Division)

If a Time Signature Meta Event comes after a Set Tempo event, it can safely be ignored -- it will
change the size of a quarter-note, but that doesn't matter because the calculation cancels out the
quarter-notes.

However, if a Time Signature Meta Event comes *before* a Set Tempo event, then that will change the
quarter-notes per beat, which means we'll use the 120 beats per minute default tempo (2 beats per
second) to figure out the *samples per tick*: Sample Rate /
(Division &times; 2 &times; 2<sup>2 - `MM`</sup>)

One last note: there needs to be care taken when calculating timing because a MIDI file can change
the tempo or time signature during the middle of the song, which will change how ticks map to
samples.

Appendix: SysEx Manufactuerer Id Numbers
========================================

Assignment of an Id number requires payment to MIDI.org every year.  According to the website, those
who stop paying have their Id's rescinded.  I'm not sure what that means from a technical
standpoint - it doesn't look like the Id numbers are reallocated to new companies.  As of 2017, the
fee ranges from $500 to $20000 per year, depending on the size of the company.

| Id         | Company                                                                            |
|------------|------------------------------------------------------------------------------------|
| `00`       | Used for extensions, below                                                         |
| `01`       | Sequential                                                                         |
| `02`       | IDP                                                                                |
| `03`       | Voyetra Turtle Beach, Inc. / Octave-Plateau                                        |
| `04`       | Moog Music                                                                         |
| `05`       | Passport Designs                                                                   |
| `06`       | Lexicon Inc.                                                                       |
| `07`       | Kurzweil / Young Chang                                                             |
| `08`       | Fender                                                                             |
| `09`       | MIDI9 / Gulbransen                                                                 |
| `0A`       | AKG Acoustics                                                                      |
| `0B`       | Voyce Music                                                                        |
| `0C`       | Waveframe Corp (Timeline)                                                          |
| `0D`       | ADA Signal Processors, Inc.                                                        |
| `0E`       | Garfield Electronics                                                               |
| `0F`       | Ensoniq                                                                            |
| `10`       | Oberheim / Gibson Labs                                                             |
| `11`       | Apple, Inc.                                                                        |
| `12`       | Grey Matter Response                                                               |
| `13`       | Digidesign Inc. (rescinded)                                                        |
| `14`       | Palmtree Instruments                                                               |
| `15`       | JLCooper Electronics                                                               |
| `16`       | Lowrey Organ Company                                                               |
| `17`       | Adams-Smith                                                                        |
| `18`       | E-mu / Ensoniq                                                                     |
| `19`       | Harmony Systems                                                                    |
| `1A`       | ART                                                                                |
| `1B`       | Baldwin                                                                            |
| `1C`       | Eventide                                                                           |
| `1D`       | Inventronics                                                                       |
| `1E`       | Key Concepts (rescinded)                                                           |
| `1F`       | Clarity                                                                            |
| `20`       | Passac                                                                             |
| `21`       | Proel Labs (SIEL)                                                                  |
| `22`       | Synthaxe (UK)                                                                      |
| `23`       | Stepp (rescinded)                                                                  |
| `24`       | Hohner                                                                             |
| `25`       | Twister                                                                            |
| `26`       | Ketron s.r.l. / Solton                                                             |
| `27`       | Jellinghaus MS                                                                     |
| `28`       | Southworth Music Systems                                                           |
| `29`       | PPG (Germany)                                                                      |
| `2A`       | JEN                                                                                |
| `2B`       | Solid State Logic Organ Systems / SSL Limited                                      |
| `2C`       | Audio Veritrieb-P. Struven                                                         |
| `2D`       | Neve (rescinded)                                                                   |
| `2E`       | Soundtracs Ltd. (rescinded)                                                        |
| `2F`       | Elka                                                                               |
| `30`       | Dynacord                                                                           |
| `31`       | Viscount International Spa (Intercontinental Electronics) (rescinded)              |
| `32`       | Drawmer (rescinded)                                                                |
| `33`       | Clavia Digital Instruments                                                         |
| `34`       | Audio Architecture                                                                 |
| `35`       | GeneralMusic Corp SpA                                                              |
| `36`       | Cheetah Marketing (rescinded)                                                      |
| `37`       | C.T.M. (rescinded)                                                                 |
| `38`       | Simmons UK (rescinded)                                                             |
| `39`       | Soundcraft Electronics                                                             |
| `3A`       | Steinberg Media Technologies AG (rescinded)                                        |
| `3B`       | Wersi Gmbh                                                                         |
| `3C`       | AVAB Niethammer AB / Avab Electronik                                               |
| `3D`       | Digigram                                                                           |
| `3E`       | Waldorf Electronics GmbH                                                           |
| `3F`       | Quasimidi                                                                          |
| `40`       | Kawai                                                                              |
| `41`       | Roland                                                                             |
| `42`       | Korg                                                                               |
| `43`       | Yamaha                                                                             |
| `44`       | Casio                                                                              |
| `45`       | ?                                                                                  |
| `46`       | Kamiya Studio                                                                      |
| `47`       | Akai                                                                               |
| `48`       | Japan Victor                                                                       |
| `49`       | Mesosha                                                                            |
| `4A`       | Hoshino Gakki                                                                      |
| `4B`       | Fujitsu Elect                                                                      |
| `4C`       | Sony                                                                               |
| `4D`       | Nisshin Onpa                                                                       |
| `4E`       | TEAC                                                                               |
| `4F`       | ?                                                                                  |
| `50`       | Matsushita Electric                                                                |
| `51`       | Fostex                                                                             |
| `52`       | Zoom                                                                               |
| `53`       | Midori Electronics                                                                 |
| `54`       | Matsushita Communication Industrial                                                |
| `55`       | Suzuki Musical Inst. Mfg.                                                          |
| `56`       | ?                                                                                  |
| `57`       | ?                                                                                  |
| `58`       | ?                                                                                  |
| `59`       | ?                                                                                  |
| `5A`       | ?                                                                                  |
| `5B`       | ?                                                                                  |
| `5C`       | ?                                                                                  |
| `5D`       | ?                                                                                  |
| `5E`       | ?                                                                                  |
| `5F`       | ?                                                                                  |
| `60`       | ?                                                                                  |
| `61`       | ?                                                                                  |
| `62`       | ?                                                                                  |
| `63`       | ?                                                                                  |
| `64`       | ?                                                                                  |
| `65`       | ?                                                                                  |
| `66`       | ?                                                                                  |
| `67`       | ?                                                                                  |
| `68`       | ?                                                                                  |
| `69`       | ?                                                                                  |
| `6A`       | ?                                                                                  |
| `6B`       | ?                                                                                  |
| `6C`       | ?                                                                                  |
| `6D`       | ?                                                                                  |
| `6E`       | ?                                                                                  |
| `6F`       | ?                                                                                  |
| `70`       | ?                                                                                  |
| `71`       | ?                                                                                  |
| `72`       | ?                                                                                  |
| `73`       | ?                                                                                  |
| `74`       | ?                                                                                  |
| `75`       | ?                                                                                  |
| `76`       | ?                                                                                  |
| `77`       | ?                                                                                  |
| `78`       | ?                                                                                  |
| `79`       | ?                                                                                  |
| `7A`       | ?                                                                                  |
| `7B`       | ?                                                                                  |
| `7C`       | ?                                                                                  |
| `7D`       | ?                                                                                  |
| `7E`       | Reserved for Non-Real Time Universal SysEx Messages                                |
| `7F`       | Reserved for Real Time Universal SysEx Messages                                    |
| --         | --                                                                                 |
| `00 00 00` | ?                                                                                  |
| `00 00 01` | Time/Warner Interactive                                                            |
| `00 00 02` | Advanced Gravis Comp. Tech Ltd. (rescinded)                                        |
| `00 00 03` | Media Vision (rescinded)                                                           |
| `00 00 04` | Dornes Research Group (rescinded)                                                  |
| `00 00 05` | K-Muse (rescinded)                                                                 |
| `00 00 06` | Stypher (rescinded)                                                                |
| `00 00 07` | Digital Music Corp.                                                                |
| `00 00 08` | IOTA Systems                                                                       |
| `00 00 09` | New England Digital                                                                |
| `00 00 0A` | Artisyn                                                                            |
| `00 00 0B` | IVL Technologies Ltd.                                                              |
| `00 00 0C` | Southern Music Systems                                                             |
| `00 00 0D` | Lake Butler Sound Company                                                          |
| `00 00 0E` | Alesis Studio Electronics                                                          |
| `00 00 0F` | Sound Creation (rescinded)                                                         |
| `00 00 10` | DOD Electronics Corp.                                                              |
| `00 00 11` | Studer-Editech                                                                     |
| `00 00 12` | Sonus (rescinded)                                                                  |
| `00 00 13` | Temporal Acuity Products (rescinded)                                               |
| `00 00 14` | Perfect Fretworks                                                                  |
| `00 00 15` | KAT Inc.                                                                           |
| `00 00 16` | Opcode Systems                                                                     |
| `00 00 17` | Rane Corporation                                                                   |
| `00 00 18` | Anadi Electronique                                                                 |
| `00 00 19` | KMX                                                                                |
| `00 00 1A` | Allen & Heath Brenell                                                              |
| `00 00 1B` | Peavey Electronics                                                                 |
| `00 00 1C` | 360 Systems                                                                        |
| `00 00 1D` | Spectrum Design and Development                                                    |
| `00 00 1E` | Marquis Music                                                                      |
| `00 00 1F` | Zeta Systems                                                                       |
| `00 00 20` | Axxes (Brian Parsonett)                                                            |
| `00 00 21` | Orban                                                                              |
| `00 00 22` | Indian Valley Mfg. (rescinded)                                                     |
| `00 00 23` | Triton (rescinded)                                                                 |
| `00 00 24` | KTI                                                                                |
| `00 00 25` | Breakaway Technologies                                                             |
| `00 00 26` | Leprecon / CAE Inc.                                                                |
| `00 00 27` | Harrison Systems Inc. (rescinded)                                                  |
| `00 00 28` | Future Lab/Mark Kuo (rescinded)                                                    |
| `00 00 29` | Rocktron Corporation                                                               |
| `00 00 2A` | PianoDisc                                                                          |
| `00 00 2B` | Cannon Research Group                                                              |
| `00 00 2C` | ?                                                                                  |
| `00 00 2D` | Rodgers Instrument LLC                                                             |
| `00 00 2E` | Blue Sky Logic                                                                     |
| `00 00 2F` | Encore Electronics                                                                 |
| `00 00 30` | Uptown                                                                             |
| `00 00 31` | Voce                                                                               |
| `00 00 32` | CTI Audio, Inc. (Musically Intel. Devs.)                                           |
| `00 00 33` | S3 Incorporated                                                                    |
| `00 00 34` | Broderbund / Red Orb                                                               |
| `00 00 35` | Allen Organ Co.                                                                    |
| `00 00 36` | ?                                                                                  |
| `00 00 37` | Music Quest (rescinded)                                                            |
| `00 00 38` | Aphex                                                                              |
| `00 00 39` | Gallien Krueger                                                                    |
| `00 00 3A` | IBM                                                                                |
| `00 00 3B` | Mark Of The Unicorn                                                                |
| `00 00 3C` | Hotz Corporation                                                                   |
| `00 00 3D` | ETA Lighting                                                                       |
| `00 00 3E` | NSI Corporation                                                                    |
| `00 00 3F` | Ad Lib, Inc.                                                                       |
| `00 00 40` | Richmond Sound Design                                                              |
| `00 00 41` | Microsoft                                                                          |
| `00 00 42` | Mindscape (Software Toolworks)                                                     |
| `00 00 43` | Russ Jones Marketing / Niche                                                       |
| `00 00 44` | Intone                                                                             |
| `00 00 45` | Advanced Remote Technologies (rescinded)                                           |
| `00 00 46` | White Instruments (rescinded)                                                      |
| `00 00 47` | GT Electronics/Groove Tubes                                                        |
| `00 00 48` | Pacific Research & Engineering (rescinded)                                         |
| `00 00 49` | Timeline Vista, Inc.                                                               |
| `00 00 4A` | Mesa Boogie Ltd.                                                                   |
| `00 00 4B` | FSLI (rescinded)                                                                   |
| `00 00 4C` | Sequoia Development Group                                                          |
| `00 00 4D` | Studio Electronics                                                                 |
| `00 00 4E` | Euphonix, Inc                                                                      |
| `00 00 4F` | InterMIDI, Inc.                                                                    |
| `00 00 50` | MIDI Solutions Inc.                                                                |
| `00 00 51` | 3DO Company                                                                        |
| `00 00 52` | Lightwave Research / High End Systems                                              |
| `00 00 53` | Micro-W Corporation                                                                |
| `00 00 54` | Spectral Synthesis, Inc.                                                           |
| `00 00 55` | Lone Wolf                                                                          |
| `00 00 56` | Studio Technologies Inc.                                                           |
| `00 00 57` | Peterson Electro-Musical Product (EMP), Inc.                                       |
| `00 00 58` | Atari Corporation                                                                  |
| `00 00 59` | Marion Systems Corporation                                                         |
| `00 00 5A` | Design Event                                                                       |
| `00 00 5B` | Winjammer Software Ltd.                                                            |
| `00 00 5C` | AT&T Bell Laboratories                                                             |
| `00 00 5D` | ?                                                                                  |
| `00 00 5E` | Symetrix                                                                           |
| `00 00 5F` | MIDI the World                                                                     |
| `00 00 60` | Spatializer / Desper Products                                                      |
| `00 00 61` | Micros 'N MIDI                                                                     |
| `00 00 62` | Accordians International                                                           |
| `00 00 63` | 3Com / EuPhonics                                                                   |
| `00 00 64` | Musonix                                                                            |
| `00 00 65` | Turtle Beach Systems (Voyetra)                                                     |
| `00 00 66` | Loud Technologies / Mackie Designs                                                 |
| `00 00 67` | Compuserve                                                                         |
| `00 00 68` | BEC Technologies                                                                   |
| `00 00 69` | QRS Music Inc                                                                      |
| `00 00 6A` | P.G. Music                                                                         |
| `00 00 6B` | Sierra Semiconductor                                                               |
| `00 00 6C` | EpiGraf Audio Visual                                                               |
| `00 00 6D` | Electronics Diversified Inc                                                        |
| `00 00 6E` | Tune 1000                                                                          |
| `00 00 6F` | Advanced Micro Devices                                                             |
| `00 00 70` | Mediamation                                                                        |
| `00 00 71` | Sabine Musical Mfg. Co. Inc.                                                       |
| `00 00 72` | Woog Labs                                                                          |
| `00 00 73` | Micropolis Corp                                                                    |
| `00 00 74` | Ta Horng Musical Inst.                                                             |
| `00 00 75` | eTek / Forte                                                                       |
| `00 00 76` | Electrovoice                                                                       |
| `00 00 77` | Midisoft                                                                           |
| `00 00 78` | Q-Sound Labs                                                                       |
| `00 00 79` | Westrex                                                                            |
| `00 00 7A` | NVidia                                                                             |
| `00 00 7B` | ESS Technology                                                                     |
| `00 00 7C` | MediaTrix Peripherals                                                              |
| `00 00 7D` | Brooktree                                                                          |
| `00 00 7E` | Otari                                                                              |
| `00 00 7F` | Key Electronics                                                                    |
| `00 00 80` | Crystalake Multimedia                                                              |
| `00 00 81` | Crystal Semiconductor                                                              |
| `00 00 82` | Rockwell Semiconductor                                                             |
| --         | --                                                                                 |
| `00 20 00` | Dream SAS                                                                          |
| `00 20 01` | Strand Lighting                                                                    |
| `00 20 02` | Amek Div of Harman Industries                                                      |
| `00 20 03` | Casa Di Risparmio Di Loreto (rescinded)                                            |
| `00 20 04` | Bohm Electronic GmbH                                                               |
| `00 20 05` | Syntec Digital Audio (rescinded)                                                   |
| `00 20 06` | Trident Audio Developments                                                         |
| `00 20 07` | Real World Studio                                                                  |
| `00 20 08` | Evolution Synthesis, Ltd (rescinded)                                               |
| `00 20 09` | Yes Technology                                                                     |
| `00 20 0A` | Audiomatica                                                                        |
| `00 20 0B` | Bontempi SpA (Sigma) / Farfisa                                                     |
| `00 20 0C` | F.B.T. Elettronica SpA                                                             |
| `00 20 0D` | MidiTemp GmbH                                                                      |
| `00 20 0E` | LA Audio (Larking Audio)                                                           |
| `00 20 0F` | Zero 88 Lighting Limited                                                           |
| `00 20 10` | Micon Audio Electronics GmbH                                                       |
| `00 20 11` | Forefront Technology                                                               |
| `00 20 12` | Studio Audio and Video Ltd. (rescinded)                                            |
| `00 20 13` | Kenton Electronics                                                                 |
| `00 20 14` | Celco / Electrosonic (rescinded)                                                   |
| `00 20 15` | ADB                                                                                |
| `00 20 16` | Marshall Products Limited                                                          |
| `00 20 17` | DDA                                                                                |
| `00 20 18` | BSS Audio Ltd.                                                                     |
| `00 20 19` | MA Lighting Technology                                                             |
| `00 20 1A` | Fatar SRL c/o Music Industries                                                     |
| `00 20 1B` | QSC Audio Products Inc. (rescinded)                                                |
| `00 20 1C` | Artisan Clasic Organ Inc.                                                          |
| `00 20 1D` | Orla Spa                                                                           |
| `00 20 1E` | Pinnacle Audio (Klark Teknik PLC)                                                  |
| `00 20 1F` | TC Electronics                                                                     |
| `00 20 20` | Doepfer Musikelektronik GmbH                                                       |
| `00 20 21` | Creative ATC / E-mu                                                                |
| `00 20 22` | Seyddo/Minami                                                                      |
| `00 20 23` | LG Electronics / Goldstar                                                          |
| `00 20 24` | Midisoft sas di M.Cima & C                                                         |
| `00 20 25` | Samick Musical Inst. Co. Ltd.                                                      |
| `00 20 26` | Penny and Giles (Bowthorpe PLC)                                                    |
| `00 20 27` | Acorn Computer                                                                     |
| `00 20 28` | LSC Electronics Pty. Ltd.                                                          |
| `00 20 29` | Novation EMS                                                                       |
| `00 20 2A` | Samkyung Mechatronics                                                              |
| `00 20 2B` | Medeli Electronics                                                                 |
| `00 20 2C` | Charlie Lab                                                                        |
| `00 20 2D` | Blue Chip Music Tech                                                               |
| `00 20 2E` | BEE OH Corp                                                                        |
