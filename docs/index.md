# sanjuuni
A fast and powerful image and video converter for ComputerCraft.

![screenshot](https://raw.githubusercontent.com/MCJack123/sanjuuni-ui/master/screenshot.png)

sanjuuni allows you to convert images and videos into a format suitable for display in ComputerCraft. It automatically decodes files, creates an optimal palette for each image, dithers to smoothen edges, and packs pixels into CC's 2x3 "sixel" characters.

sanjuuni supports output to the following formats:
- Lua script, which can be run standalone without a player
- Blit images (BIMG)
- Paintutils NFP (does not support small pixels)
- CraftOS-PC Raw Mode protocol packets
- sanjuuni custom 32Vid video format
- HTTP server, which can be played with `wget run`
- WebSocket server, either self-hosted or run through a client-client pipe

Because sanjuuni uses FFmpeg to decode media files, it can support nearly every type of image and video format available. It can also take advantage of GPU acceleration to speed up conversion even further.

sanjuuni is split up into two different projects: `sanjuuni`, which is the core program and uses a CLI interface, and `sanjuuni-ui`, which is a GUI wrapper that makes it more friendly to use.

## Using (GUI)
First, open the input file using the "Open Input" button in the top-left corner, and find the file in the file browser. You can also drag and drop the file onto the window to quickly select it.

Next, select the options for conversion quality:
- The "Size" section allows you to resize the input before converting for ComputerCraft. Use this to adjust the size to fit the screen the image will be displayed on. Note that smaller sizes will convert *much* faster than large ones.
- The "Quality" section adjusts how colors are adapted to fit ComputerCraft's 16 color palette. The following options are available:
  - **D**efault: Use the default palette from ComputerCraft - do not optimize the palette
  - **M**edian cut: Fast and simple color conversion that takes the average of 16 groups of colors
  - **k**-means: Slower improvement over median cut that tries to find clusters of colors and generates center points accordingly
  - **O**ctree: Slowest algorithm that splits the colors by dividing into a 3D tree structure
- The "Dithering" section selects a filter to use to smoothen out the harsh edges that reducing colors produces. The following options are available:
  - **N**o dithering: Does not perform dithering, leaving edges sharp but clean (also known as thresholding)
  - **O**rdered (Bayer) dithering: Uses a pre-defined pattern to shift colors darker or lighter, causing gradients to be smoother and pixelated
  - **F**loyd-**S**teinberg dithering: Uses error diffusion to push the differences in colors to nearby pixels, creating smooth edges with realistic mixing
- "Use Lab color space" tells sanjuuni to convert colors to a format that is more accurate to how the human eye sees colors, which can make the final color palette have better range.
- The "Advanced..." button opens the Advanced Options menu, which has a few options that are usually not important for average users. Hover over each option to see what it does.

After changing any of the parameters, the preview will automatically update itself to show what the image will look like in-game.

Once you finish adjusting the settings, select the type of file to output. If you chose HTTP or WebSocket server outputs, choose the port to host the server on. If you chose the WebSocket client output, type the URL of the WebSocket to connect to in the output box at the bottom. Otherwise, use the "Browse..." button to select where to save the new image or video file.

Then click "Start" to start conversion. For videos (or on very slow computers), a progress bar will appear to show the progress through conversion. You can click the "Stop" button at any time to cancel the conversion process.

Once the file is converted, you can proceed to copy it into ComputerCraft using any of the supported methods (drag & drop, copy to world folder, [CraftOS-PC Remote](https://remote.craftos-pc.cc), etc.). For file formats that aren't Lua scripts or HTTP servers, use the player programs provided in the download to display them on the computer.

## Usage (CLI)
```
usage: ./sanjuuni [options] -i <input> [-o <output> | -s <port> | -w <port> | -u <url>]
sanjuuni converts images and videos into a format that can be displayed in 
ComputerCraft.

-ifile, --input=file            Input image or video
-Sfile, --subtitle=file         ASS-formatted subtitle file to add to the video
-opath, --output=path           Output file path
-l, --lua                       Output a Lua script file (default for images; only does one frame)
-n, --nfp                       Output an NFP format image for use in paint (changes proportions!)
-r, --raw                       Output a rawmode-based image/video file (default for videos)
-b, --blit-image                Output a blit image (BIMG) format image/animation file
-3, --32vid                     Output a 32vid format binary video file with compression + audio
-sport, --http=port             Serve an HTTP server that has each frame split up + a player program
-wport, --websocket=port        Serve a WebSocket that sends the image/video with audio
-uurl, --websocket-client=url   Connect to a WebSocket server to send image/video with audio
-T, --streamed                  For servers, encode data on-the-fly instead of doing it ahead of time (saves memory at the cost of speed and only one client)
-p, --default-palette           Use the default CC palette instead of generating an optimized one
-t, --threshold                 Use thresholding instead of dithering
-O, --ordered                   Use ordered dithering
-L, --lab-color                 Use CIELAB color space for higher quality color conversion
-8, --octree                    Use octree for higher quality color conversion (slower)
-k, --kmeans                    Use k-means for highest quality color conversion (slowest)
-cmode, --compression=mode      Compression type for 32vid videos; available modes: none|lzw|deflate|custom
-B, --binary                    Output blit image files in a more-compressed binary format (requires opening the file in binary mode)
-d, --dfpwm                     Use DFPWM compression on audio
-m, --mute                      Remove audio from output
-Wsize, --width=size            Resize the image to the specified width
-Hsize, --height=size           Resize the image to the specified height
-h, --help                      Show this help
```

## License
sanjuuni and sanjuuni-ui are licensed under the GPLv2+ license.
