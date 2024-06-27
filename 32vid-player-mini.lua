local bit32_band, bit32_lshift, bit32_rshift, math_frexp = bit32.band, bit32.lshift, bit32.rshift, math.frexp
local function log2(n) local _, r = math_frexp(n) return r-1 end
local dfpwm = require "cc.audio.dfpwm"

local speaker = peripheral.find "speaker"
local file
local path = ...
if path:match "^https?://" then
    file = assert(http.get(path, nil, true))
else
    file = assert(fs.open(shell.resolve(path), "rb"))
end

if file.read(4) ~= "32VD" then file.close() error("Not a 32Vid file") end
local width, height, fps, nstreams, flags = ("<HHBBH"):unpack(file.read(8))
--print(width, height, fps, nstreams, flags)
if nstreams ~= 1 then file.close() error("Separate stream files not supported by this tool") end
if bit32_band(flags, 1) == 0 then file.close() error("DEFLATE or no compression not supported by this tool") end
local _, nframes, ctype = ("<IIB"):unpack(file.read(9))
if ctype ~= 0x0C then file.close() error("Stream type not supported by this tool") end

local monitors, mawidth, maheight
if bit32.btest(flags, 0x20) then
    mawidth, maheight = bit32_band(bit32_rshift(flags, 6), 7) + 1, bit32_band(bit32_rshift(flags, 9), 7) + 1
    monitors = settings.get('sanjuuni.multimonitor')
    if not monitors or #monitors < maheight or #monitors[1] < mawidth then
        term.clear()
        term.setCursorPos(1, 1)
        print('This video needs monitors to be calibrated before being displayed. Please right-click each monitor in order, from the top left corner to the bottom right corner, going right first, then down.\n')
        monitors = {}
        local a = {}
        for b = 1, maheight do
            monitors[b] = {}
            for c = 1, mawidth do
                local d, e = term.getCursorPos()
                for f = 1, maheight do
                    term.setCursorPos(3, e + f - 1)
                    term.clearLine()
                    for g = 1, mawidth do term.blit('\x8F ', g == c and f == b and '00' or '77', 'ff') end
                end
                term.setCursorPos(3, e + maheight)
                term.write('(' .. c .. ', ' .. b .. ')')
                term.setCursorPos(1, e)
                repeat
                    local d, h = os.pullEvent('monitor_touch')
                    monitors[b][c] = h
                until not a[h]
                a[monitors[b][c]] = true
                sleep(0.25)
            end
        end
        settings.set('sanjuuni.multimonitor', monitors)
        settings.save()
        print('Calibration complete. Settings have been saved for future use.')
    end
    for _, r in ipairs(monitors) do for i, m in ipairs(r) do r[i] = peripheral.wrap(m) peripheral.call(m, "setTextScale", 0.5) peripheral.call(m, "clear") end end
end

local function readDict(size)
    local retval = {}
    for i = 0, size - 1, 2 do
        local b = file.read()
        retval[i] = bit32.rshift(b, 4)
        retval[i+1] = bit32.band(b, 15)
    end
    return retval
end
local init, read
if bit32_band(flags, 3) == 1 then
    local decodingTable, X, readbits, isColor
    function init(c)
        isColor = c
        local R = file.read()
        local L = 2^R
        local Ls = readDict(c and 24 or 32)
        if R == 0 then
            decodingTable = file.read()
            X = nil
            return
        end
        local a = 0
        for i = 0, #Ls do Ls[i] = Ls[i] == 0 and 0 or 2^(Ls[i]-1) a = a + Ls[i] end
        assert(a == L, a)
        decodingTable = {R = R}
        local x, step, next, symbol = 0, 0.625 * L + 3, {}, {}
        for i = 0, #Ls do
            next[i] = Ls[i]
            for _ = 1, Ls[i] do
                while symbol[x] do x = (x + 1) % L end
                x, symbol[x] = (x + step) % L, i
            end
        end
        for x = 0, L - 1 do
            local s = symbol[x]
            local t = {s = s, n = R - log2(next[s])}
            t.X, decodingTable[x], next[s] = bit32_lshift(next[s], t.n) - L, t, 1 + next[s]
        end
        local partial, bits, pos = 0, 0, 1
        function readbits(n)
            if not n then n = bits % 8 end
            if n == 0 then return 0 end
            while bits < n do pos, bits, partial = pos + 1, bits + 8, bit32_lshift(partial, 8) + file.read() end
            local retval = bit32_band(bit32_rshift(partial, bits-n), 2^n-1)
            bits = bits - n
            return retval
        end
        X = readbits(R)
    end
    function read(nsym)
        local retval = {}
        if X == nil then
            for i = 1, nsym do retval[i] = decodingTable end
            return retval
        end
        local i = 1
        local last = 0
        while i <= nsym do
            local t = decodingTable[X]
            if isColor and t.s >= 16 then
                local l = 2^(t.s - 15)
                for n = 0, l-1 do retval[i+n] = last end
                i = i + l
            else retval[i], last, i = t.s, t.s, i + 1 end
            X = t.X + readbits(t.n)
        end
        --print(X)
        return retval
    end
else
    error("Unimplemented!")
end

local blitColors = {[0] = "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "a", "b", "c", "d", "e", "f"}
local start = os.epoch "utc"
local lastyield = start
local vframe = 0
local subs = {}
term.clear()
for _ = 1, nframes do
    local size, ftype = ("<IB"):unpack(file.read(5))
    --print(size, ftype, file.seek())
    if ftype == 0 then
        if os.epoch "utc" - lastyield > 3000 then sleep(0) lastyield = os.epoch "utc" end
        local dcstart = os.epoch "utc"
        --print("init screen", vframe, file.seek())
        init(false)
        --print("read screen", vframe, file.seek())
        local screen = read(width * height)
        --print("init colors", vframe, file.seek())
        init(true)
        --print("read bg colors", vframe)
        local bg = read(width * height)
        --print("read fg colors", vframe)
        local fg = read(width * height)
        local dctime = os.epoch "utc" - dcstart
        while os.epoch "utc" < start + vframe * 1000 / fps do end
        local texta, fga, bga = {}, {}, {}
        for y = 0, height - 1 do
            local text, fgs, bgs = "", "", ""
            for x = 1, width do
                text = text .. string.char(128 + screen[y*width+x])
                fgs = fgs .. blitColors[fg[y*width+x]]
                bgs = bgs .. blitColors[bg[y*width+x]]
            end
            texta[y+1], fga[y+1], bga[y+1] = text, fgs, bgs
        end
        for i = 0, 15 do term.setPaletteColor(2^i, file.read() / 255, file.read() / 255, file.read() / 255) end
        for y = 1, height do
            term.setCursorPos(1, y)
            term.blit(texta[y], fga[y], bga[y])
        end
        local delete = {}
        for i, v in ipairs(subs) do
            if vframe <= v.frame + v.length then
                term.setCursorPos(v.x, v.y)
                term.setBackgroundColor(v.bgColor)
                term.setTextColor(v.fgColor)
                term.write(v.text)
            else delete[#delete+1] = i end
        end
        for i, v in ipairs(delete) do table.remove(subs, v - i + 1) end
        --term.setCursorPos(1, height + 1)
        --term.clearLine()
        --print("Frame decode time:", dctime, "ms")
        vframe = vframe + 1
    elseif ftype == 1 then
        local audio = file.read(size)
        if speaker then
            if bit32_band(flags, 12) == 0 then
                local chunk = {audio:byte(1, -1)}
                for i = 1, #chunk do chunk[i] = chunk[i] - 128 end
                speaker.playAudio(chunk)
            else
                speaker.playAudio(dfpwm.decode(audio))
            end
        end
    elseif ftype == 8 then
        local data = file.read(size)
        local sub = {}
        sub.frame, sub.length, sub.x, sub.y, sub.color, sub.flags, sub.text = ("<IIHHBBs2"):unpack(data)
        sub.bgColor, sub.fgColor = 2^bit32_rshift(sub.color, 4), 2^bit32_band(sub.color, 15)
        subs[#subs+1] = sub
    elseif ftype >= 0x40 and ftype < 0x80 then
        if ftype == 64 then vframe = vframe + 1 end
        local mx, my = bit32_band(bit32_rshift(ftype, 3), 7) + 1, bit32_band(ftype, 7) + 1
        --print("(" .. mx .. ", " .. my .. ")")
        local term = monitors[my][mx]
        if os.epoch "utc" - lastyield > 3000 then sleep(0) lastyield = os.epoch "utc" end
        local width, height = ("<HH"):unpack(file.read(4))
        local dcstart = os.epoch "utc"
        --print("init screen", vframe, file.seek())
        init(false)
        --print("read screen", vframe, file.seek())
        local screen = read(width * height)
        --print("init colors", vframe, file.seek())
        init(true)
        --print("read bg colors", vframe)
        local bg = read(width * height)
        --print("read fg colors", vframe)
        local fg = read(width * height)
        local dctime = os.epoch "utc" - dcstart
        while os.epoch "utc" < start + vframe * 1000 / fps do end
        local texta, fga, bga = {}, {}, {}
        for y = 0, height - 1 do
            local text, fgs, bgs = "", "", ""
            for x = 1, width do
                text = text .. string.char(128 + screen[y*width+x])
                fgs = fgs .. blitColors[fg[y*width+x]]
                bgs = bgs .. blitColors[bg[y*width+x]]
            end
            texta[y+1], fga[y+1], bga[y+1] = text, fgs, bgs
        end
        for i = 0, 15 do term.setPaletteColor(2^i, file.read() / 255, file.read() / 255, file.read() / 255) end
        for y = 1, height do
            term.setCursorPos(1, y)
            term.blit(texta[y], fga[y], bga[y])
        end
        --[[local delete = {}
        for i, v in ipairs(subs) do
            if vframe <= v.frame + v.length then
                term.setCursorPos(v.x, v.y)
                term.setBackgroundColor(v.bgColor)
                term.setTextColor(v.fgColor)
                term.write(v.text)
            else delete[#delete+1] = i end
        end
        for i, v in ipairs(delete) do table.remove(subs, v - i + 1) end]]
        --term.setCursorPos(1, height + 1)
        --term.clearLine()
        --print("Frame decode time:", dctime, "ms")
    else file.close() error("Unknown frame type " .. ftype) end
end

for i = 0, 15 do term.setPaletteColor(2^i, term.nativePaletteColor(2^i)) end
term.setBackgroundColor(colors.black)
term.setTextColor(colors.white)
term.setCursorPos(1, 1)
term.clear()
