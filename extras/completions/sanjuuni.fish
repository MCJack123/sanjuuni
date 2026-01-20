#!/usr/bin/env fish
# Fish completions for the Sanjuuni command-line tool

complete -c sanjuuni -f

complete -c sanjuuni -s h -l help -d 'Print a short help text and exit' # Pretty much standard in CLI

# File IO
complete -c sanjuuni -o ifile -l input -d "Input image or video"
complete -c sanjuuni -o Sfile -l subtitle -d "ASS-formatted subtitle file to add to the video"
complete -c sanjuuni -o opath -l output -d "Output file path"

# Export options
complete -c sanjuuni -s l -l lua -d "Output a Lua script file (default for images; only supports one frame)"
complete -c sanjuuni -s r -l raw -d "Output a rawmode-based image/video file (default for videos)"
complete -c sanjuuni -s b -l blit-image -d "Output a blit image (BIMG) format image/animation file"
complete -c sanjuuni -s 3 -l 32vid -d "Output a 32vid format binary video file with compression + audio"

# Size
complete -c sanjuuni -o Wsize -l width -d "Resize the image to be the specified width"
complete -c sanjuuni -o Hsize -l height -d "Resize the image to be the specified height"

# HTTP and WebSocket
complete -c sanjuuni -s T -l streamed -d "For servers, encode data on-the-fly instead of doing it ahead of time (saves memory at the cost of speed, and only one client can connect"

complete -c sanjuuni -o sport -l http -d "Serve an HTTP server that has each frame split up + a player program"

complete -c sanjuuni -o wport -l websocket -d "Serve a WebSocket that sends the image/video with audio"
complete -c sanjuuni -o uurl -l websocket-client -d "Connect to a WebSocket server to send image/video with audio"

# Compression and optimization
complete -c sanjuuni -s p -l default-pallete -d "Use the default CC palette instead of generating an optimized one"
complete -c sanjuuni -s t -l threshold -d "Use thresholding instead of dithering"
complete -c sanjuuni -s 8 -l octree -d "Use octree for higher quality color conversion (slower)"
complete -c sanjuuni -s k -l kmeans -d "Use k-means for highest quality color conversion (slowest)"
complete -c sanjuuni -o cmode -l compression -d "Compression type for 32vid videos" -a "none lzw deflate custom"
complete -c sanjuuni -s d -l dfpwm -d "Use DFPWM compression on audio"

complete -c sanjuuni -o L1 -l compression-level=1 -d "Compression level for 32vid videos when using DEFLATE"
complete -c sanjuuni -o L2 -l compression-level=2 -d "Compression level for 32vid videos when using DEFLATE"
complete -c sanjuuni -o L3 -l compression-level=3 -d "Compression level for 32vid videos when using DEFLATE"
complete -c sanjuuni -o L4 -l compression-level=4 -d "Compression level for 32vid videos when using DEFLATE"
complete -c sanjuuni -o L5 -l compression-level=5 -d "Compression level for 32vid videos when using DEFLATE"
complete -c sanjuuni -o L6 -l compression-level=6 -d "Compression level for 32vid videos when using DEFLATE"
complete -c sanjuuni -o L7 -l compression-level=7 -d "Compression level for 32vid videos when using DEFLATE"
complete -c sanjuuni -o L8 -l compression-level=8 -d "Compression level for 32vid videos when using DEFLATE"
complete -c sanjuuni -o L9 -l compression-level=9 -d "Compression level for 32vid videos when using DEFLATE"
