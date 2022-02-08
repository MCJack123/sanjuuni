local b64str = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
local function base64decode(str)
    local retval = ""
    for s in str:gmatch "...." do
        if s:sub(3, 4) == '==' then
            retval = retval .. string.char(bit32.bor(bit32.lshift(b64str:find(s:sub(1, 1)) - 1, 2), bit32.rshift(b64str:find(s:sub(2, 2)) - 1, 4)))
        elseif s:sub(4, 4) == '=' then
            local n = (b64str:find(s:sub(1, 1))-1) * 4096 + (b64str:find(s:sub(2, 2))-1) * 64 + (b64str:find(s:sub(3, 3))-1)
            retval = retval .. string.char(bit32.extract(n, 10, 8)) .. string.char(bit32.extract(n, 2, 8))
        else
            local n = (b64str:find(s:sub(1, 1))-1) * 262144 + (b64str:find(s:sub(2, 2))-1) * 4096 + (b64str:find(s:sub(3, 3))-1) * 64 + (b64str:find(s:sub(4, 4))-1)
            retval = retval .. string.char(bit32.extract(n, 16, 8)) .. string.char(bit32.extract(n, 8, 8)) .. string.char(bit32.extract(n, 0, 8))
        end
    end
    return retval
end

local file, err = fs.open(shell.resolve(...), "r")
if not file then error(err) end
if file.readLine() ~= "32Vid 1.1" then error("Unsupported file") end
local fps = tonumber(file.readLine())
local first, second = file.readLine(), file.readLine()
if second == "" or second == nil then fps = 0 end
term.clear()
while true do
    local frame
    if first then frame, first = first, nil
    elseif second then frame, second = second, nil
    else frame = file.readLine() end
    if frame == "" or frame == nil then break end
    local mode = frame:match("^!CP([CD])")
    if not mode then error("Invalid file") end
    local b64data
    if mode == "C" then
        local len = tonumber(frame:sub(5, 8), 16)
        b64data = frame:sub(9, len + 8)
    else
        local len = tonumber(frame:sub(5, 16), 16)
        b64data = frame:sub(17, len + 16)
    end
    local data = base64decode(b64data)
    -- TODO: maybe verify checksums?
    assert(data:sub(1, 4) == "\0\0\0\0" and data:sub(9, 16) == "\0\0\0\0\0\0\0\0", "Invalid file")
    local width, height = ("HH"):unpack(data, 5)
    local c, n, pos = string.unpack("c1B", data, 17)
    local text = {}
    for y = 1, height do
        text[y] = ""
        for x = 1, width do
            text[y] = text[y] .. c
            n = n - 1
            if n == 0 then c, n, pos = string.unpack("c1B", data, pos) end
        end
    end
    c = c:byte()
    for y = 1, height do
        local fg, bg = "", ""
        for x = 1, width do
            fg, bg = fg .. ("%x"):format(bit32.band(c, 0x0F)), bg .. ("%x"):format(bit32.rshift(c, 4))
            n = n - 1
            if n == 0 then c, n, pos = string.unpack("BB", data, pos) end
        end
        term.setCursorPos(1, y)
        term.blit(text[y], fg, bg)
    end
    pos = pos - 2
    local r, g, b
    for i = 0, 15 do
        r, g, b, pos = string.unpack("BBB", data, pos)
        term.setPaletteColor(2^i, r / 255, g / 255, b / 255)
    end
    if fps == 0 then read() break
    else sleep(1 / fps) end
end
file.close()
for i = 0, 15 do term.setPaletteColor(2^i, term.nativePaletteColor(2^i)) end
term.setBackgroundColor(colors.black)
term.setTextColor(colors.white)
term.clear()
term.setCursorPos(1, 1)