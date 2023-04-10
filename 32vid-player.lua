local inflate do

    local floor = math.floor
    local ceil = math.ceil
    local min = math.min
    local max = math.max

    -- utility functions

    local function memoize(f)
        local cache = {}
        return function(...)
            local key = table.concat({...}, "-")
            if not cache[key] then
                cache[key] = f(...)
            end
            return cache[key]
        end
    end

    local function int(bytes)
        local n = 0
        for i = 1, #bytes do
            n = 256*n + bytes:sub(i, i):byte()
        end
        return n
    end
    int = memoize(int)

    local function bint(bits)
        return tonumber(bits, 2) or 0
    end
    bint = memoize(bint)

    local function bits(b, width)
        local s = ""
        if type(b) == "number" then
            for i = 1, width do
                s = b%2 .. s
                b = floor(b/2)
            end
        else
            for i = 1, #b do
                s = s .. bits(b:byte(i), 8):reverse()
                assert(#s == i * 8, s)
            end
        end
        return s
    end
    bits = memoize(bits)

    local function fill(bytes, len)
        return bytes:rep(floor(len / #bytes)) .. bytes:sub(1, len % #bytes)
    end

    local function zip(t1, t2)
        local zipped = {}
        for i = 1, max(#t1, #t2) do
            zipped[#zipped + 1] = {t1[i], t2[i]}
        end
        return zipped
    end

    local function unzip(zipped)
        local t1, t2 = {}, {}
        for i = 1, #zipped do
            t1[#t1 + 1] = zipped[i][1]
            t2[#t2 + 1] = zipped[i][2]
        end
        return t1, t2
    end

    local function map(f, t)
        local mapped = {}
        for i = 1, #t do
            mapped[#mapped + 1] = f(t[i], i)
        end
        return mapped
    end

    local function filter(pred, t)
        local filtered = {}
        for i = 1, #t do
            if pred(t[i], i) then
                filtered[#filtered + 1] = t[i]
            end
        end
        return filtered
    end

    local function find(key, t)
        if type(key) == "function" then
            for i = 1, #t do
                if key(t[i]) then
                    return i
                end
            end
            return nil
        else
            return find(function(x) return x == key end, t)
        end
    end

    local function slice(t, i, j, step)
        local sliced = {}
        for k = i < 1 and 1 or i, i < 1 and #t + i or j or #t, step or 1 do
            sliced[#sliced + 1] = t[k]
        end
        return sliced
    end

    local function range(i, j)
        local r = {}
        for k = j and i or 0, j or i - 1 do
            r[#r + 1] = k
        end
        return r
    end

    -- streams

    local function output_stream()
        local stream, buffer = {}, {}
        local curr = 0

        function stream:write(bytes)
            for i = 1, #bytes do
                buffer[#buffer + 1] = bytes:sub(i, i)
            end
            curr = curr + #bytes
        end

        function stream:back_read(offset, n)
            local read = {}
            for i = curr - offset + 1, curr - offset + n do
                read[#read + 1] = buffer[i]
            end
            return table.concat(read)
        end

        function stream:back_copy(dist, len)
            local start, copied = curr - dist + 1, {}
            for i = start, min(start + len, curr) do
                copied[#copied + 1] = buffer[i]
            end
            self:write(fill(table.concat(copied), len))
        end

        function stream:pos()
            return curr
        end

        function stream:raw()
            return table.concat(buffer)
        end

        return stream
    end

    local function bit_stream(raw, offset)
        local stream = {}
        local curr = 0
        offset = offset or 0

        function stream:read(n, reverse)
            local start = floor(curr/8) + offset + 1
            local bb = ""
            for i = start, start + ceil(n/8) do
                local b = raw:byte(i)
                for j = 0, 7 do bb = bb .. bit32.extract(b, j, 1) end
            end
            local b = bb:sub(curr%8 + 1, curr%8 + n)
            curr = curr + n
            return reverse and b or b:reverse()
        end

        function stream:seek(n)
            if n == "beg" then
                curr = 0
            elseif n == "end" then
                curr = #raw
            else
                curr = curr + n
            end
            return self
        end

        function stream:is_empty()
            return curr >= 8*#raw
        end

        function stream:pos()
            return curr
        end

        return stream
    end

    -- inflate

    local CL_LENS_ORDER = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15}
    local MAX_BITS = 15
    local PT_WIDTH = 8

    local function cl_code_lens(stream, hclen)
        local code_lens = {}
        for i = 1, hclen do
            code_lens[#code_lens + 1] = bint(stream:read(3))
        end
        return code_lens
    end

    local function code_tree(lens, alphabet)
        alphabet = alphabet or range(#lens)
        local using = filter(function(x, i) return lens[i] and lens[i] > 0 end, alphabet)
        lens = filter(function(x) return x > 0 end, lens)
        local tree = zip(lens, using)
        table.sort(tree, function(a, b)
            if a[1] == b[1] then
                return a[2] < b[2]
            else
                return a[1] < b[1]
            end
        end)
        return unzip(tree)
    end

    local function codes(lens)
        local codes = {}
        local code = 0
        for i = 1, #lens do
            codes[#codes + 1] = bits(code, lens[i])
            if i < #lens then
                code = (code + 1)*2^(lens[i + 1] - lens[i])
            end
        end
        return codes
    end

    local prefix_table
    local function handle_long_codes(codes, alphabet, pt)
        local i = find(function(x) return #x > PT_WIDTH end, codes)
        local long = slice(zip(codes, alphabet), i)
        i = 0
        repeat
            local prefix = long[i + 1][1]:sub(1, PT_WIDTH)
            local same = filter(function(x) return x[1]:sub(1, PT_WIDTH) == prefix end, long)
            same = map(function(x) return {x[1]:sub(PT_WIDTH + 1), x[2]} end, same)
            pt[prefix] = {rest = prefix_table(unzip(same)), unused = 0}
            i = i + #same
        until i == #long
    end

    function prefix_table(codes, alphabet)
        local pt = {}
        if #codes[#codes] > PT_WIDTH then
            handle_long_codes(codes, alphabet, pt)
        end
        for i = 1, #codes do
            local code = codes[i]
            if #code > PT_WIDTH then
                break
            end
            local entry = {value = alphabet[i], unused = PT_WIDTH - #code}
            if entry.unused == 0 then
                pt[code] = entry
            else
                for i = 0, 2^entry.unused - 1 do
                    pt[code .. bits(i, entry.unused)] = entry
                end
            end
        end
        return pt
    end

    local function huffman_decoder(lens, alphabet)
        local base_codes = prefix_table(codes(lens), alphabet)
        return function(stream)
            local codes = base_codes
            local entry
            repeat
                entry = codes[stream:read(PT_WIDTH, true)]
                stream:seek(-entry.unused)
                codes = entry.rest
            until not codes
            return entry.value
        end
    end

    local function code_lens(stream, decode, n)
        local lens = {}
        repeat
            local value = decode(stream)
            if value < 16 then
                lens[#lens + 1] = value
            elseif value == 16 then
                for i = 1, bint(stream:read(2)) + 3 do
                    lens[#lens + 1] = lens[#lens]
                end
            elseif value == 17 then
                for i = 1, bint(stream:read(3)) + 3 do
                    lens[#lens + 1] = 0
                end
            elseif value == 18 then
                for i = 1, bint(stream:read(7)) + 11 do
                    lens[#lens + 1] = 0
                end
            end
        until #lens == n
        return lens
    end

    local function code_trees(stream)
        local hlit = bint(stream:read(5)) + 257
        local hdist = bint(stream:read(5)) + 1
        local hclen = bint(stream:read(4)) + 4
        local cl_decode = huffman_decoder(code_tree(cl_code_lens(stream, hclen), CL_LENS_ORDER))
        local ll_decode = huffman_decoder(code_tree(code_lens(stream, cl_decode, hlit)))
        local d_decode = huffman_decoder(code_tree(code_lens(stream, cl_decode, hdist)))
        return ll_decode, d_decode
    end

    local function extra_bits(value)
        if value >= 4 and value <= 29 then
            return floor(value/2) - 1
        elseif value >= 265 and value <= 284 then
            return ceil(value/4) - 66
        else
            return 0
        end
    end
    extra_bits = memoize(extra_bits)

    local function decode_len(value, bits)
        assert(value >= 257 and value <= 285, "value out of range")
        assert(#bits == extra_bits(value), "wrong number of extra bits")
        if value <= 264 then
            return value - 254
        elseif value == 285 then
            return 258
        end
        local len = 11
        for i = 1, #bits - 1 do
            len = len + 2^(i+2)
        end
        return floor(bint(bits) + len + ((value - 1) % 4)*2^#bits)
    end
    decode_len = memoize(decode_len)

    local function a(n)
        if n <= 3 then
            return n + 2
        else
            return a(n-1) + 2*a(n-2) - 2*a(n-3)
        end
    end
    a = memoize(a)

    local function decode_dist(value, bits)
        assert(value >= 0 and value <= 29, "value out of range")
        assert(#bits == extra_bits(value), "wrong number of extra bits)")
        return bint(bits) + a(value - 1)
    end
    decode_dist = memoize(decode_dist)

    function inflate(data)
        local stream = bit_stream(data)
        local ostream = output_stream()
        repeat
            local bfinal, btype = bint(stream:read(1)), bint(stream:read(2))
            assert(btype == 2, "compression method not supported")
            local ll_decode, d_decode = code_trees(stream)
            while true do
                local value = ll_decode(stream)
                if value < 256 then
                    ostream:write(string.char(value))
                elseif value == 256 then
                    break
                else
                    local len = decode_len(value, stream:read(extra_bits(value)))
                    value = d_decode(stream)
                    local dist = decode_dist(value, stream:read(extra_bits(value)))
                    ostream:back_copy(dist, len)
                end
            end
        until bfinal == 1
        return ostream:raw()
    end

end
local dfpwm = require "cc.audio.dfpwm"

local hexstr = "0123456789abcdef"
local file, err = fs.open(shell.resolve(...), "rb")
if not file then error(err) end
if file.read(4) ~= "32VD" then file.close() error("Not a 32vid file") end
local width, height, fps, nStreams, flags = ("<HHBBH"):unpack(file.read(8))
local video, audio, subtitles
for _ = 1, nStreams do
    local size, nFrames, frameType = ("<IIB"):unpack(file.read(9))
    print(("%X"):format(file.seek()), size, nFrames, frameType)
    if frameType == 0 and not video then
        local data = file.read(size)
        if bit32.band(flags, 3) == 2 then data = inflate(data) end
        video = {}
        local pos = 1
        local start = os.epoch "utc"
        for i = 1, nFrames do
            if i % 100 == 0 or i >= nFrames - 10 then print(i, os.epoch "utc" - start) start = os.epoch "utc" sleep(0) end
            local frame = {palette = {}}
            local tmp, tmppos = 0, 0
            local use5bit, customcompress = bit32.btest(flags, 16), bit32.band(flags, 3) == 3
            local codetree = {}
            local solidchar, runlen
            local function readField(isColor)
                if customcompress then
                    if runlen then
                        local c = solidchar
                        runlen = runlen - 1
                        if runlen == 0 then runlen = nil end
                        return c
                    end
                    if not isColor and solidchar then return solidchar end
                    -- MARK: Huffman decoding
                    local node = codetree
                    while true do
                        local n = bit32.extract(tmp, tmppos, 1)
                        tmppos = tmppos - 1
                        if tmppos < 0 then tmp, pos, tmppos = data:byte(pos), pos + 1, 7 end
                        if type(node) ~= "table" then error(("Invalid tree state: position %X, frame %d"):format(pos+file.seek()-size-1, i)) end
                        if type(node[n]) == "number" then
                            local c = node[n]
                            if isColor then
                                if c > 15 then runlen = 2^(c-15)-1 return assert(solidchar)
                                else solidchar = c end
                            end
                            return c
                        else node = node[n] end
                    end
                else
                    local n
                    if tmppos * 5 + 5 > 32 then n = bit32.extract(math.floor(tmp / 0x1000000), tmppos * 5 - 24, 5)
                    else n = bit32.extract(tmp, tmppos * 5, 5) end
                    tmppos = tmppos - 1
                    if tmppos < 0 then tmp, pos = (">I5"):unpack(data, pos) tmppos = 7 end
                    return n
                end
            end
            if customcompress then
                -- MARK: Huffman tree reconstruction
                -- read bit lengths
                local bitlen = {}
                if use5bit then
                    for j = 0, 15 do
                        bitlen[j*2+1], bitlen[j*2+2] = {s = j*2, l = bit32.rshift(data:byte(pos+j), 4)}, {s = j*2+1, l = bit32.band(data:byte(pos+j), 0x0F)}
                    end
                    pos = pos + 16
                else
                    for j = 0, 7 do
                        tmp, pos = (">I5"):unpack(data, pos)
                        bitlen[j*8+1] = {s = j*8+1, l = math.floor(tmp / 0x800000000)}
                        bitlen[j*8+2] = {s = j*8+2, l = math.floor(tmp / 0x40000000) % 32}
                        for k = 3, 8 do bitlen[j*8+k] = {s = j*8+k, l = bit32.extract(tmp, (8-k)*5, 5)} end
                    end
                end
                do
                    local j = 1
                    while j <= #bitlen do
                        if bitlen[j].l == 0 then table.remove(bitlen, j)
                        else j = j + 1 end
                    end
                end
                if #bitlen == 0 then
                    -- screen is solid character
                    solidchar = data:byte(pos)
                    pos = pos + 1
                else
                    -- reconstruct codes from bit lengths
                    table.sort(bitlen, function(a, b) if a.l == b.l then return a.s < b.s else return a.l < b.l end end)
                    bitlen[1].c = 0
                    for j = 2, #bitlen do bitlen[j].c = bit32.lshift(bitlen[j-1].c + 1, bitlen[j].l - bitlen[j-1].l) end
                    -- create tree from codes
                    for j = 1, #bitlen do
                        local c = bitlen[j].c
                        local node = codetree
                        for k = bitlen[j].l - 1, 1, -1 do
                            local n = bit32.extract(c, k, 1)
                            if not node[n] then node[n] = {} end
                            node = node[n]
                            if type(node) == "number" then error(("Invalid tree state: position %X, frame %d, #bitlen = %d, current entry = %d"):format(pos, i, #bitlen, j)) end
                        end
                        local n = bit32.extract(c, 0, 1)
                        node[n] = bitlen[j].s
                    end
                    -- read first byte
                    tmp, tmppos, pos = data:byte(pos), 7, pos + 1
                end
            else readField() end
            for y = 1, height do
                local line = {"", "", "", {}}
                for x = 1, width do
                    if pos + 5 + 1 >= #data then print(i, pos, x, y) error() end
                    local n = readField()
                    line[1] = line[1] .. string.char(128 + (n % 0x20))
                    line[4][x] = bit32.btest(n, 0x20)
                end
                frame[y] = line
            end
            if customcompress then
                if tmppos == 7 then pos = pos - 1 end
                codetree = {}
                -- MARK: Huffman tree reconstruction
                -- read bit lengths
                local bitlen = {}
                for j = 0, 11 do
                    bitlen[j*2+1], bitlen[j*2+2] = {s = j*2, l = bit32.rshift(data:byte(pos+j), 4)}, {s = j*2+1, l = bit32.band(data:byte(pos+j), 0x0F)}
                end
                pos = pos + 12
                do
                    local j = 1
                    while j <= #bitlen do
                        if bitlen[j].l == 0 then table.remove(bitlen, j)
                        else j = j + 1 end
                    end
                end
                if #bitlen == 0 then
                    -- screen is solid color
                    solidchar = data:byte(pos)
                    pos = pos + 1
                    runlen = math.huge
                else
                    -- reconstruct codes from bit lengths
                    table.sort(bitlen, function(a, b) if a.l == b.l then return a.s < b.s else return a.l < b.l end end)
                    bitlen[1].c = 0
                    for j = 2, #bitlen do bitlen[j].c = bit32.lshift(bitlen[j-1].c + 1, bitlen[j].l - bitlen[j-1].l) end
                    -- create tree from codes
                    for j = 1, #bitlen do
                        local c = bitlen[j].c
                        local node = codetree
                        for k = bitlen[j].l - 1, 1, -1 do
                            local n = bit32.extract(c, k, 1)
                            if not node[n] then node[n] = {} end
                            node = node[n]
                            if type(node) == "number" then error(("Invalid tree state: position %X, frame %d, #bitlen = %d, current entry = %d"):format(pos, i, #bitlen, j)) end
                        end
                        local n = bit32.extract(c, 0, 1)
                        node[n] = bitlen[j].s
                    end
                    -- read first byte
                    tmp, tmppos, pos = data:byte(pos), 7, pos + 1
                end
                for y = 1, height do
                    local line = frame[y]
                    for x = 1, width do
                        local c = readField(true)
                        line[2] = line[2] .. hexstr:sub(c+1, c+1)
                    end
                end
                runlen = nil
                for y = 1, height do
                    local line = frame[y]
                    for x = 1, width do
                        local c = readField(true)
                        line[3] = line[3] .. hexstr:sub(c+1, c+1)
                    end
                end
                if tmppos == 7 then pos = pos - 1 end
            else
                for y = 1, height do
                    local line = frame[y]
                    for x = 1, width do
                        local c = data:byte(pos)
                        pos = pos + 1
                        if line[4][x] then c = bit32.bor(bit32.band(bit32.lshift(c, 4), 0xF0), bit32.rshift(c, 4)) end
                        line[2] = line[2] .. hexstr:sub(bit32.band(c, 15)+1, bit32.band(c, 15)+1)
                        line[3] = line[3] .. hexstr:sub(bit32.rshift(c, 4)+1, bit32.rshift(c, 4)+1)
                    end
                    line[4] = nil
                end
            end
            for n = 1, 16 do frame.palette[n], pos = {data:byte(pos) / 255, data:byte(pos+1) / 255, data:byte(pos+2) / 255}, pos + 3 end
            video[i] = frame
        end
    elseif frameType == 1 and not audio then
        audio = {}
        if bit32.band(flags, 12) == 0 then
            for i = 1, math.ceil(size / 48000) do
                local data
                if jit then
                    data = {}
                    for j = 0, 5 do
                        local t = {file.read(math.min(size - (i-1) * 48000 - j * 8000, 8000)):byte(1, -1)}
                        if #t > 0 then for k = 1, #t do data[j*8000+k] = t[k] end end
                    end
                else data = {file.read(math.min(size - (i-1) * 48000, 48000)):byte(1, -1)} end
                for j = 1, #data do data[j] = data[j] - 128 end
                audio[i] = data
            end
        elseif bit32.band(flags, 12) == 4 then
            local decode = dfpwm.make_decoder()
            for i = 1, math.ceil(size / 6000) do
                local data = file.read(math.min(size - (i-1)*6000, 6000))
                if not data then break end
                audio[i] = decode(data)
            end
        end
    elseif frameType == 8 and not subtitles then
        subtitles = {}
        for _ = 1, nFrames do
            local start, length, x, y, color, sz = ("<IIHHBxH"):unpack(file.read(16))
            local text = file.read(sz)
            local sub = {text, hexstr:sub(bit32.band(color, 15)+1, bit32.band(color, 15)+1):rep(#text),
                hexstr:sub(bit32.rshift(color, 4)+1, bit32.rshift(color, 4)+1):rep(#text), x = x, y = y}
            for n = start, start + length - 1 do
                subtitles[n] = subtitles[n] or {}
                subtitles[n][#subtitles[n]+1] = sub
            end
        end
    end
end
file.close()
if not video then error("No video stream found") end
sleep(0)

local speaker = peripheral.find "speaker"
term.clear()
local ok, err = pcall(parallel.waitForAll, function()
    local start = os.epoch "utc"
    for f, image in ipairs(video) do
        for i, v in ipairs(image.palette) do term.setPaletteColor(2^(i-1), table.unpack(v)) end
        for y, r in ipairs(image) do
            term.setCursorPos(1, y)
            term.blit(table.unpack(r))
        end
        if subtitles and subtitles[f-1] then
            for _, v in ipairs(subtitles[f-1]) do
                term.setCursorPos(v.x, v.y)
                term.blit(table.unpack(v))
            end
        end
        while os.epoch "utc" < start + (f + 1) / fps * 1000 do sleep(1 / fps) end
    end
end, function()
    if not speaker or not speaker.playAudio or not audio then return end
    for _, chunk in ipairs(audio) do
        while not speaker.playAudio(chunk) do repeat local ev, sp = os.pullEvent("speaker_audio_empty") until sp == peripheral.getName(speaker) end
    end
end)
for i = 0, 15 do term.setPaletteColor(2^i, term.nativePaletteColor(2^i)) end
term.setBackgroundColor(colors.black)
term.setTextColor(colors.white)
term.setCursorPos(1, 1)
term.clear()
if not ok then printError(err) end
