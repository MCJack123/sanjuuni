# sanjuuni
Converts images and videos into a format that can be displayed in ComputerCraft. Spiritual successor to [juroku](https://github.com/tmpim/juroku), which is hard to build and isn't as flexible.

## Building
Requirements:
* C++11 or later compiler
* FFmpeg libraries
* Poco

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

-ifile, --input=file      Input image or video
-opath, --output=path     Output file path
-l, --lua                 Output a Lua script file (default for images; only 
                          does one frame)
-r, --raw                 Output a rawmode-based image/video file (default for
                          videos)
-sport, --http=port       Serve an HTTP server that has each frame split up + 
                          a player program
-wport, --websocket=port  Serve a WebSocket that sends the image/video with 
                          audio
-p, --default-palette     Use the default CC palette instead of generating an 
                          optimized one
-t, --threshold           Use thresholding instead of dithering
-8, --octree              Use octree for higher quality color conversion 
                          (slower)
-wsize, --width=size      Resize the image to the specified width
-Hsize, --height=size     Resize the image to the specified height
-h, --help                Show this help
```

### Playback programs
* `32vid-player.lua` plays back raw video files from the disk. Simply give it the file name and it will decode and play the file.
* `websocket-player.lua` plays a stream from a sanjuuni WebSocket server. Simply give it the WebSocket URL and it will play the stream, with audio if a speaker is attached.

## Formats
* The Lua file output creates a simple script that displays the image and waits for the return key to be pressed. The data is stored as a plain blit table, so it can be copied to another file as desired with the display code.
* The raw file output creates a file based on [CraftOS-PC Raw Mode](https://www.craftos-pc.cc/docs/rawmode). Each frame packet is stored on one line, and the first two lines contain a version header and the FPS. (If the FPS is 0, this is a plain image.)
* The HTTP server output starts an HTTP server that serves a single player file at the root (`/`), which can be run through `wget run`. The metadata of the stream is stored at `/info` as a JSON file with the fields `length` and `fps`. Each frame is stored in a separate URL under a `video` or `audio` folder, e.g. `/video/0` returns the first frame of the video, and `/audio/0` returns the first second of audio.
* The WebSocket server output starts a WebSocket server that sends frames over the connection. It uses a simple request/response protocol, with the first character of the frame indicating the request type, and an optional parameter follows the character. For `v` and `a` commands, success is indicated by the message being a binary message. (Failure messages will always be exactly `"!"`, but successful messages may be `"!"` as well!) The following commands are available:
  * `v<n>`: Returns the `n`th frame of video as a Lua script that returns the image + palette
  * `a<n>`: Returns one second of audio starting at the `n`th sample in unsigned 8-bit PCM
  * `n`: Returns the number of video frames available
  * `f`: Returns the framerate of the video

## License
sanjuuni is licensed under the GPL license.