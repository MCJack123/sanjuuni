local ws, err = http.websocket(...)
if not ws then error("Could not connect to WebSocket server: " .. err) end
ws.send("n")
local nFrames = tonumber(ws.receive(), 10)
ws.send("f")
local fps = tonumber(ws.receive(), 10)
local speaker = peripheral.find "speaker"
term.clear()
local lock = false
parallel.waitForAll(function()
    local start = os.epoch "utc"
    for f = 0, nFrames - 1 do
        while lock do os.pullEvent() end
        lock = true
        ws.send("v" .. f)
        local frame, ok = ws.receive()
        while #frame % 65535 == 0 do frame = frame .. ws.receive() end
        lock = false
        --if not ok then break end
        if load(frame) then
            local image, palette = assert(load(frame, "=frame", "t", {}))()
            for i = 0, #palette do term.setPaletteColor(2^i, table.unpack(palette[i])) end
            for y, r in ipairs(image) do
                term.setCursorPos(1, y)
                term.blit(table.unpack(r))
            end
        end
        while os.epoch "utc" < start + (f + 1) / fps * 1000 do sleep(1 / fps) end
    end
end, function()
    if not speaker or not speaker.playAudio then return end
    local pos = 0
    while pos < nFrames / fps * 48000 do
        while lock do os.pullEvent() end
        lock = true
        ws.send("a" .. pos)
        local audio, ok = ws.receive(1)
        lock = false
        if not ok then break end
        audio = {audio:byte(1, -1)}
        for i = 1, #audio do audio[i] = audio[i] - 128 end
        pos = pos + #audio
        while not speaker.playAudio(audio) do repeat local ev, sp = os.pullEvent("speaker_audio_empty") until sp == peripheral.getName(speaker) end
    end
end)
ws.close()
for i = 0, 15 do term.setPaletteColor(2^i, term.nativePaletteColor(2^i)) end
term.setBackgroundColor(colors.black)
term.setTextColor(colors.white)
term.setCursorPos(1, 1)
term.clear()
