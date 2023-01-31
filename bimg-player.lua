local path = ...
if not path then error("Usage: bimg-player <file.bimg>") end
local file, err = fs.open(shell.resolve(path), "rb")
if not file then error("Could not open file: " .. err) end
local img = textutils.unserialize(file.readAll())
file.close()
term.clear()
for _, frame in ipairs(img) do
    for y, row in ipairs(frame) do
        term.setCursorPos(1, y)
        term.blit(table.unpack(row))
    end
    if frame.palette then for i = 0, #frame.palette do
        local c = frame.palette[i]
        if type(c) == "table" then term.setPaletteColor(2^i, table.unpack(c))
        else term.setPaletteColor(2^i, c) end
    end end
    if img.animation then sleep(frame.duration or img.secondsPerFrame or 0.05)
    else read() break end
end
term.setBackgroundColor(colors.black)
term.setTextColor(colors.white)
term.clear()
term.setCursorPos(1, 1)
for i = 0, 15 do term.setPaletteColor(2^i, term.nativePaletteColor(2^i)) end
