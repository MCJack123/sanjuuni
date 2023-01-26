-- This file is stored minified in the C++ source. It is kept here for reference.
local url = '${URL}'
local function get(path)
    local handle, err = http.get('http://' .. url .. path, nil, true)
    if not handle then error(err) end
    local data = handle.readAll()
    handle.close()
    return data
end
local info = textutils.unserializeJSON(get('/info'))
local frames, audios = {}, {}
local speaker = peripheral.find 'speaker'
term.clear()
local lock = 2
parallel.waitForAll(function()
    for f = 0, info.length - 1 do frames[f] = get('/video/' .. f) if lock > 0 then lock = lock - 1 end end
end, function()
    pcall(function() for f = 0, info.length / info.fps do audios[f] = get('/audio/' .. f) if lock > 0 then lock = lock - 1 end end end)
end, function()
    while lock > 0 do os.pullEvent() end
    local start = os.epoch 'utc'
    for f = 0, info.length - 1 do
        while not frames[f] do os.pullEvent() end
        local frame = frames[f]
        frames[f] = nil
        local image, palette = assert(load(frame, '=frame', 't', {}))()
        for i = 0, #palette do term.setPaletteColor(2^i, table.unpack(palette[i])) end
        for y, r in ipairs(image) do
            term.setCursorPos(1, y)
            term.blit(table.unpack(r))
        end
        while os.epoch 'utc' < start + (f + 1) / info.fps * 1000 do sleep(1 / info.fps) end
    end
end, function()
    if not speaker or not speaker.playAudio then return end
    while lock > 0 do os.pullEvent() end
    local pos = 0
    while pos < info.length / info.fps do
        while not audios[pos] do os.pullEvent() end
        local audio = audios[pos]
        audios[pos] = nil
        audio = {audio:byte(1, -1)}
        for i = 1, #audio do audio[i] = audio[i] - 128 end
        pos = pos + 1
        if not speaker.playAudio(audio) then repeat local ev, sp = os.pullEvent('speaker_audio_empty') until sp == peripheral.getName(speaker) end
    end
end)
for i = 0, 15 do term.setPaletteColor(2^i, term.nativePaletteColor(2^i)) end
term.setBackgroundColor(colors.black)
term.setTextColor(colors.white)
term.setCursorPos(1, 1)
term.clear()