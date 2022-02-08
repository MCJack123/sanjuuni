# sanjuuni
Converts images and videos into a format that can be displayed in ComputerCraft. Spiritual successor to [juroku](https://github.com/tmpim/juroku), which is hard to build and isn't as flexible.

## Building
Requirements:
* C++11 or later compiler
* FFmpeg libraries
* Poco
* zlib (usually required by Poco)

To build:
```sh
./configure
make
```

On Windows, use the Visual Studio solution with vcpkg to build.

## Usage
```
usage: sanjuuni [options] -i <input> [-o <output>]
sanjuuni converts images and videos into a format that can be displayed in 
ComputerCraft.

-i file, --input=file                                Input image or video
-o path, --output=path                               Output file path
-l, --lua                                            Output a Lua script file (default for images; only does one frame)
-r, --raw                                            Output a rawmode-based image/video file (default for videos)
-3, --32vid                                          Output a 32vid format binary video file with compression + audio
-s port, --http=port                                 Serve an HTTP server that has each frame split up + a player program
-w port, --websocket=port                            Serve a WebSocket that sends the image/video with audio
-p, --default-palette                                Use the default CC palette instead of generating an optimized one
-t, --threshold                                      Use thresholding instead of dithering
-8, --octree                                         Use octree for higher quality color conversion (slower)
-5, --5bit                                           Use 5-bit codes in 32vid (internal testing)
-c none|lzw|deflate, --compression=none|lzw|deflate  Compression type for 32vid videos
-L 1-9, --compression-level=1-9                      Compression level for 32vid videos when using DEFLATE
-w size, --width=size                                Resize the image to the specified width
-H size, --height=size                               Resize the image to the specified height
-h, --help                                           Show this help
```

### Playback programs
* `32vid-player.lua` plays back 32vid video/audio files from the disk. Simply give it the file name and it will decode and play the file.
* `raw-player.lua` plays back raw video files from the disk. Simply give it the file name and it will decode and play the file.
* `websocket-player.lua` plays a stream from a sanjuuni WebSocket server. Simply give it the WebSocket URL and it will play the stream, with audio if a speaker is attached.

## Formats
* The Lua file output creates a simple script that displays the image and waits for the return key to be pressed. The data is stored as a plain blit table, so it can be copied to another file as desired with the display code.
* The raw file output creates a file based on [CraftOS-PC Raw Mode](https://www.craftos-pc.cc/docs/rawmode). Each frame packet is stored on one line, and the first two lines contain a version header and the FPS. (If the FPS is 0, this is a plain image.)
* The 32vid file output creates a file that uses the 32vid format described below, which is a binary file that stores compressed and optimized versions of the video and audio data in multiple streams
* The HTTP server output starts an HTTP server that serves a single player file at the root (`/`), which can be run through `wget run`. The metadata of the stream is stored at `/info` as a JSON file with the fields `length` and `fps`. Each frame is stored in a separate URL under a `video` or `audio` folder, e.g. `/video/0` returns the first frame of the video, and `/audio/0` returns the first second of audio.
* The WebSocket server output starts a WebSocket server that sends frames over the connection. It uses a simple request/response protocol, with the first character of the frame indicating the request type, and an optional parameter follows the character. For `v` and `a` commands, success is indicated by the message being a binary message. (Failure messages will always be exactly `"!"`, but successful messages may be `"!"` as well!) The following commands are available:
  * `v<n>`: Returns the `n`th frame of video as a Lua script that returns the image + palette
  * `a<n>`: Returns one second of audio starting at the `n`th sample in unsigned 8-bit PCM
  * `n`: Returns the number of video frames available
  * `f`: Returns the framerate of the video

### 32vid file format
The 32vid format consists of a number of streams, which can hold video, audio, or subtitles/overlay text. Each stream may be compressed as specified in the flags. Video data is stored in an optimized format for drawing characters, using only 5-6 + 8 bits per character cell, and pixels and colors are split into two planes to optimize for compression algorithms. Audio data is always at 48000 Hz, and (in the current version) is unsigned 8-bit PCM or DFPWM.

#### Header
| Offset | Bytes | Description                |
|--------|-------|----------------------------|
| 0x00   | 4     | Magic `"32VD"`             |
| 0x04   | 2     | Video width in characters  |
| 0x06   | 2     | Video height in characters |
| 0x08   | 1     | Frames per second          |
| 0x09   | 1     | Number of streams          |
| 0x0A   | 2     | Flags (see below)          |
| 0x0C   | *n*   | List of streams            |

**Flags:**
* Bits 0-1: Compression for video; 0 = none, 1 = LZW, 2 = DEFLATE
* Bits 2-3: Compression for audio; 0 = none (PCM), 1 = DFPWM
* Bit 4: Whether the video uses 5-bit (1) or 6-bit (0) codes

#### Streams
| Offset | Bytes | Description                |
|--------|-------|----------------------------|
| 0x00   | 4     | Size of data               |
| 0x04   | 4     | Data length: number of frames for video, number of samples for audio, number of events for subtitles |
| 0x08   | 1     | Type of chunk              |
| 0x09   | *n*   | Compressed stream data     |

**Chunk types:**
* 0: Video
* 1: Audio (mono)
* 2: Left audio (stereo)
* 3: Right audio (stereo)
* 4-7: Additional audio channels if desired
* 8: Primary subtitles
* 9-11: Alternate subtitles if desired

#### Video data
Video data is stored as a list of frames, with each frame consisting of packed pixel data, then colors. Pixel data is stored in a bitstream as 5- or 6-bit codes depending on the 4th bit of the global flags. The low 5 bits correspond to the low 5 bits in the drawing characters, and the character can be derived by adding 128 to the value.

The 6th bit, if present, indicates that the colors should be switched. This is intended to allow for better compression over the color plane, as flipping the colors is a common occurrence, and this bit allows longer runs of the same color which can improve compression. However, this does add an extra bit to every character. The benefits of 6-bit mode are still being tested.

Pixels are grouped in tokens of 5 bytes/8 characters in 5-bit mode, and 3 bytes/4 characters in 6-bit mode. If the characters cannot fit in a multiple of 4/8 characters, the token is right-padded with zeroes to fit. After pixel data, each cell's colors are written with one byte per cell, with the high nibble storing the background and the low nibble storing the text color. Finally, the palette is written as 16 sets of 24-bit RGB colors.

The length of a frame can be determined with the formula `ceil(width * height / 8) * 5 + width * height` in 5-bit mode, or `ceil(width * height / 4) * 3 + width * height` in 6-bit mode.

#### Subtitle events
| Offset | Bytes | Description                          |
|--------|-------|--------------------------------------|
| 0x00   | 4     | Frame number to show the text on     |
| 0x04   | 4     | Number of frames to display for      |
| 0x08   | 2     | X position of the text in characters |
| 0x0A   | 2     | Y position of the text in characters |
| 0x0C   | 1     | Color set for the text (BG+FG)       |
| 0x0D   | 1     | Flags (reserved)                     |
| 0x0E   | 2     | Text length                          |
| 0x10   | *n*   | Text                                 |

Subtitle streams are arranged as sequences of subtitle events. Events MUST be ordered by start time; decoders are not required to respect events that are out of order.

## License
sanjuuni is licensed under the GPL license.