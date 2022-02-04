# sanjuuni
Converts images and videos into a format that can be displayed in ComputerCraft. Spiritual successor to [juroku](https://github.com/tmpim/juroku), which is hard to build and isn't as flexible.

## Building
Requirements:
* C++11 or later compiler
* libav + libswresample (included with FFmpeg)
* Poco

Simply build `sanjuuni.cpp` with a C++ compiler, linking in the following libraries:
```
-lavcodec -lavformat -lavutil -lswscale -lswresample -lpthread -lPocoFoundation -lPocoUtil -lPocoNet
```

## Usage
```
-ifile, --input=file      Input image or video
-opath, --output=path     Output file path
-l, --lua                 Output a Lua script file (default for images)
-r, --raw                 Output a rawmode-based image/video file (required 
                          for videos)
-hport, --http=port       Serve an HTTP server that has each frame split up + 
                          a player program
-wport, --websocket=port  Serve a WebSocket that sends the image/video with 
                          audio
-p, --default-palette     Use the default CC palette instead of generating an 
                          optimized one
-h, --help                Show help
```

## License
sanjuuni is licensed under the GPL license.