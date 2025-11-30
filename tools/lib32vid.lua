-- MIT License
--
-- Copyright (c) 2025 JackMacWindows
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in all
-- copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
-- SOFTWARE.

local bit32_band, bit32_lshift, bit32_rshift, math_frexp = bit32.band, bit32.lshift, bit32.rshift, math.frexp
local function log2(n) local _, r = math_frexp(n) return r-1 end
local dfpwm = require "cc.audio.dfpwm"
local blitColors = {[0] = "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "a", "b", "c", "d", "e", "f"}

---@class lib32vid
---@field read fun(n: number): string|nil
---@field read fun(): number|nil
---@field width number
---@field height number
---@field fps number
---@field flags number
---@field nframes number
---@field currentframe number
---@field vframe number
---@field subs table
local lib32vid = {}
local lib32vid_mt = {__index = lib32vid}

--- Returns a new 32vid reader from string data.
---@param data string The data to load
---@return lib32vid reader The new reader
function lib32vid.load(data)
    local obj = setmetatable({}, lib32vid_mt)
    local pos = 1
    function obj.read(bytes)
        if pos > #data then return nil end
        if bytes == nil then
            local c = data:byte(pos)
            pos = pos + 1
            return c
        end
        local str = data:sub(pos, pos + bytes - 1)
        pos = pos + bytes
        return str
    end
    return obj:init()
end

--- Returns a new 32vid reader from a file.
---@param path string The file to load
---@return lib32vid reader The new reader
function lib32vid.open(path)
    local file = assert(fs.open(path, "rb"))
    local obj = setmetatable({}, lib32vid_mt)
    obj.read = file.read
    obj.close = file.close
    return obj:init()
end

--- Returns a new 32vid reader from a URL.
---@param url string The URL to load
---@param headers table|nil Any headers to use
---@return lib32vid reader The new reader
function lib32vid.get(url, headers)
    local handle = assert(http.get(url, headers, true))
    local obj = setmetatable({}, lib32vid_mt)
    obj.read = handle.read
    obj.close = handle.close
    return obj:init()
end

---@private
---@return lib32vid self
function lib32vid:init()
    if self.read(4) ~= "32VD" then self:close() error("Not a 32Vid file", 2) end
    local data = self.read(8)
    if not data then self:close() error("Incomplete 32Vid file", 2) end
    local width, height, fps, nstreams, flags = ("<HHBBH"):unpack(data)
    --print(width, height, fps, nstreams, flags)
    if nstreams ~= 1 then self:close() error("Separate stream files not supported by this tool", 2) end
    if bit32_band(flags, 1) == 0 then self:close() error("DEFLATE or no compression not supported by this tool", 2) end
    data = self.read(9)
    if not data then self:close() error("Incomplete 32Vid file", 2) end
    local _, nframes, ctype = ("<IIB"):unpack(data)
    if ctype ~= 0x0C then self:close() error("Stream type not supported by this tool", 2) end
    self.width = width
    self.height = height
    self.fps = fps
    self.flags = flags
    self.nframes = nframes
    self.currentframe = 1
    self.vframe = 0
    self.subs = {}

    local function readDict(size)
        local retval = {}
        for i = 0, size - 1, 2 do
            local b = self.read()
            retval[i] = bit32.rshift(b, 4)
            retval[i+1] = bit32.band(b, 15)
        end
        return retval
    end
    if bit32_band(flags, 3) == 1 then
        local decodingTable, X, readbits, isColor
        function self.coder_init(c)
            isColor = c
            local R = self.read()
            local L = 2^R
            local Ls = readDict(c and 24 or 32)
            if R == 0 then
                decodingTable = self.read()
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
                while bits < n do pos, bits, partial = pos + 1, bits + 8, bit32_lshift(partial, 8) + self.read() end
                local retval = bit32_band(bit32_rshift(partial, bits-n), 2^n-1)
                bits = bits - n
                return retval
            end
            X = readbits(R)
        end
        function self.coder_read(nsym)
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

    return self
end

--- Returns the next frame in the file.
---@return "audio"|"video"|"subtitle"|nil type The type of frame returned
---@return table|nil data The data for the frame
---@return number|nil monitorX If a multimonitor video frame, the X position of the monitor
---@return number|nil monitorY If a multimonitor video frame, the Y position of the monitor
function lib32vid:next()
    if self.currentframe > self.nframes then return end
    local d = self.read(5)
    if not d then return end
    local size, ftype = ("<IB"):unpack(d)
    --print(size, ftype, file.seek())
    if ftype == 0 then
        --local dcstart = os.epoch "utc"
        --print("init screen", vframe, file.seek())
        self.coder_init(false)
        --print("read screen", vframe, file.seek())
        local screen = self.coder_read(self.width * self.height)
        --print("init colors", vframe, file.seek())
        self.coder_init(true)
        --print("read bg colors", vframe)
        local bg = self.coder_read(self.width * self.height)
        --print("read fg colors", vframe)
        local fg = self.coder_read(self.width * self.height)
        --local dctime = os.epoch "utc" - dcstart
        local bimg = {palette = {}}
        for y = 0, self.height - 1 do
            local text, fgs, bgs = "", "", ""
            for x = 1, self.width do
                text = text .. string.char(128 + screen[y*self.width+x])
                fgs = fgs .. blitColors[fg[y*self.width+x]]
                bgs = bgs .. blitColors[bg[y*self.width+x]]
            end
            bimg[y+1] = {text, fgs, bgs}
        end
        for i = 0, 15 do bimg.palette[i] = {self.read() / 255, self.read() / 255, self.read() / 255} end
        local delete = {}
        for i, v in ipairs(self.subs) do
            if self.vframe <= v.frame + v.length then
                term.setCursorPos(v.x, v.y)
                term.setBackgroundColor(v.bgColor)
                term.setTextColor(v.fgColor)
                term.write(v.text)
            else delete[#delete+1] = i end
        end
        for i, v in ipairs(delete) do table.remove(self.subs, v - i + 1) end
        --term.setCursorPos(1, height + 1)
        --term.clearLine()
        --print("Frame decode time:", dctime, "ms")
        self.vframe = self.vframe + 1
        return "video", bimg
    elseif ftype == 1 then
        local audio = self.read(size)
        if not audio then error("Incomplete frame", 2) end
        if bit32_band(self.flags, 12) == 0 then
            local chunk = {audio:byte(1, -1)}
            for i = 1, #chunk do chunk[i] = chunk[i] - 128 end
            return "audio", chunk
        else
            return "audio", dfpwm.decode(audio)
        end
    elseif ftype == 8 then
        local data = self.read(size)
        if not data then error("Incomplete frame", 2) end
        local sub = {}
        sub.frame, sub.length, sub.x, sub.y, sub.color, sub.flags, sub.text = ("<IIHHBBs2"):unpack(data)
        sub.bgColor, sub.fgColor = 2^bit32_rshift(sub.color, 4), 2^bit32_band(sub.color, 15)
        self.subs[#self.subs+1] = sub
        return "subtitle", sub
    elseif ftype >= 0x40 and ftype < 0x80 then
        if ftype == 64 then self.vframe = self.vframe + 1 end
        local mx, my = bit32_band(bit32_rshift(ftype, 3), 7) + 1, bit32_band(ftype, 7) + 1
        --print("(" .. mx .. ", " .. my .. ")")
        local data = self.read(4)
        if not data then error("Incomplete frame", 2) end
        local width, height = ("<HH"):unpack(data)
        --local dcstart = os.epoch "utc"
        --print("init screen", vframe, file.seek())
        self.coder_init(false)
        --print("read screen", vframe, file.seek())
        local screen = read(width * height)
        --print("init colors", vframe, file.seek())
        self.coder_init(true)
        --print("read bg colors", vframe)
        local bg = read(width * height)
        --print("read fg colors", vframe)
        local fg = read(width * height)
        --local dctime = os.epoch "utc" - dcstart
        local bimg = {palette = {}}
        for y = 0, self.height - 1 do
            local text, fgs, bgs = "", "", ""
            for x = 1, self.width do
                text = text .. string.char(128 + screen[y*self.width+x])
                fgs = fgs .. blitColors[fg[y*self.width+x]]
                bgs = bgs .. blitColors[bg[y*self.width+x]]
            end
            bimg[y+1] = {text, fgs, bgs}
        end
        for i = 0, 15 do bimg.palette[i] = {self.read() / 255, self.read() / 255, self.read() / 255} end
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
        return "video", bimg, mx, my
    else error("Unknown frame type " .. ftype, 2) end
end

--- Closes the underlying data stream.
function lib32vid:close()
    -- do nothing, default impl
end

return lib32vid
