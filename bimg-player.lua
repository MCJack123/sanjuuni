local path = ...
if not path then error("Usage: bimg-player <file.bimg>") end
local file, err = fs.open(shell.resolve(path), "rb")
if not file then error("Could not open file: " .. err) end
local img = textutils.unserialize(file.readAll())
file.close()
local function drawFrame(frame, term)
    for y, row in ipairs(frame) do
        term.setCursorPos(1, y)
        term.blit(table.unpack(row))
    end
    if frame.palette then for i = 0, #frame.palette do
        local c = frame.palette[i]
        if type(c) == "table" then term.setPaletteColor(2^i, table.unpack(c))
        else term.setPaletteColor(2^i, c) end
    end end
    if img.multiMonitor then term.setTextScale(img.multiMonitor.scale or 0.5) end
end
if img.multiMonitor then
    local width, height = img.multiMonitor.width, img.multiMonitor.height
    local monitors = settings.get('sanjuuni.multimonitor')
    if not monitors or #monitors < height or #monitors[1] < width then
        term.clear()
        term.setCursorPos(1, 1)
        print('This image needs monitors to be calibrated before being displayed. Please right-click each monitor in order, from the top left corner to the bottom right corner, going right first, then down.')
        monitors = {}
        local names = {}
        for y = 1, height do
            monitors[y] = {}
            for x = 1, width do
                local _, oy = term.getCursorPos()
                for ly = 1, height do
                    term.setCursorPos(3, oy + ly - 1)
                    term.clearLine()
                    for lx = 1, width do term.blit('\x8F ', (lx == x and ly == y) and '00' or '77', 'ff') end
                end
                term.setCursorPos(3, oy + height)
                term.write('(' .. x .. ', ' .. y .. ')')
                term.setCursorPos(1, oy)
                repeat
                    local _, name = os.pullEvent('monitor_touch')
                    monitors[y][x] = name
                until not names[name]
                names[monitors[y][x]] = true
                sleep(0.25)
            end
        end
        settings.set('sanjuuni.multimonitor', monitors)
        settings.save()
        print('Calibration complete. Settings have been saved for future use.')
    end
    for i = 1, #img, width * height do
        for y = 1, height do
            for x = 1, width do
                drawFrame(img[i + (y-1) * width + x-1], peripheral.wrap(monitors[y][x]))
            end
        end
        if img.animation then sleep(img[i].duration or img.secondsPerFrame or 0.05)
        else read() break end
    end
else
    term.clear()
    for _, frame in ipairs(img) do
        drawFrame(frame, term)
        if img.animation then sleep(frame.duration or img.secondsPerFrame or 0.05)
        else read() break end
    end
    term.setBackgroundColor(colors.black)
    term.setTextColor(colors.white)
    term.clear()
    term.setCursorPos(1, 1)
    for i = 0, 15 do term.setPaletteColor(2^i, term.nativePaletteColor(2^i)) end
end
