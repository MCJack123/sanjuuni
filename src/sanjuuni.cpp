/*
 * sanjuuni.cpp
 * Converts images and videos into a format that can be displayed in ComputerCraft.
 * Copyright (C) 2022 JackMacWindows
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "sanjuuni.hpp"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <zlib.h>
}
#include <Poco/URI.h>
#include <Poco/Util/OptionProcessor.h>
#include <Poco/Util/OptionSet.h>
#include <Poco/Util/OptionException.h>
#include <Poco/Util/IntValidator.h>
#include <Poco/Util/RegExpValidator.h>
#include <Poco/Util/HelpFormatter.h>
#ifndef NO_NET
#include <Poco/Net/Context.h>
#include <Poco/Net/NetException.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/WebSocket.h>
#endif
#include <chrono>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <csignal>
#ifdef USE_SDL
#include <SDL2/SDL.h>
#endif
#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#define SIZE_IOCTL
#include <sys/ioctl.h>
#endif
#ifdef _WIN32
#define SIZE_WIN
#include <windows.h>
#endif

using namespace std::chrono;
using namespace Poco::Util;
#ifndef NO_NET
using namespace Poco::Net;
#endif

enum class OutputType {
    Default,
    Lua,
    Raw,
    Vid32,
    HTTP,
    WebSocket,
    BlitImage,
    NFP
};

#ifdef STATUS_FUNCTION
extern void STATUS_FUNCTION(int nframe, int totalFrames, milliseconds elapsed, milliseconds remaining, int fps);
extern bool externalStop;
#endif

WorkQueue work;
std::mutex exitLock;
std::condition_variable exitNotify;
static std::vector<std::string> frameStorage;
static uint8_t * audioStorage = NULL;
static long audioStorageSize = 0, totalFrames = 0;
static std::mutex streamedLock;
static std::condition_variable streamedNotify;
static bool streamed = false;
static bool useDFPWM = false;
static const std::vector<Vec3b> defaultPalette = {
    {0xf0, 0xf0, 0xf0},
    {0x33, 0xb2, 0xf2},
    {0xd8, 0x7f, 0xe5},
    {0xf2, 0xb2, 0x99},
    {0x6c, 0xde, 0xde},
    {0x19, 0xcc, 0x7f},
    {0xcc, 0xb2, 0xf2},
    {0x4c, 0x4c, 0x4c},
    {0x99, 0x99, 0x99},
    {0xb2, 0x99, 0x4c},
    {0xe5, 0x66, 0xb2},
    {0xcc, 0x66, 0x33},
    {0x4c, 0x66, 0x7f},
    {0x4e, 0xa6, 0x57},
    {0x4c, 0x4c, 0xcc},
    {0x11, 0x11, 0x11}
};
static const std::string playLua = "'local function b(c)local d,e=http.get('http://'..a..c,nil,true)if not d then error(e)end;local f=d.readAll()d.close()return f end;local g=textutils.unserializeJSON(b('/info'))local h,i={},{}local j=peripheral.find'speaker'term.clear()local k=2;parallel.waitForAll(function()for l=0,g.length-1 do h[l]=b('/video/'..l)if k>0 then k=k-1 end end end,function()pcall(function()for l=0,g.length/g.fps do i[l]=b('/audio/'..l)if k>0 then k=k-1 end end end)end,function()while k>0 do os.pullEvent()end;local m=os.epoch'utc'for l=0,g.length-1 do while not h[l]do os.pullEvent()end;local n=h[l]h[l]=nil;local o,p=assert(load(n,'=frame','t',{}))()for q=0,#p do term.setPaletteColor(2^q,table.unpack(p[q]))end;for r,s in ipairs(o)do term.setCursorPos(1,r)term.blit(table.unpack(s))end;while os.epoch'utc'<m+(l+1)/g.fps*1000 do sleep(1/g.fps)end end end,function()if not j or not j.playAudio then return end;while k>0 do os.pullEvent()end;local t=0;while t<g.length/g.fps do while not i[t]do os.pullEvent()end;local u=i[t]i[t]=nil;u={u:byte(1,-1)}for q=1,#u do u[q]=u[q]-128 end;t=t+1;if not j.playAudio(u)then repeat local v,w=os.pullEvent('speaker_audio_empty')until w==peripheral.getName(j)end end end)for q=0,15 do term.setPaletteColor(2^q,term.nativePaletteColor(2^q))end;term.setBackgroundColor(colors.black)term.setTextColor(colors.white)term.setCursorPos(1,1)term.clear()";
static const std::string multiMonitorLua = "local monitors=settings.get('sanjuuni.multimonitor')if not monitors or#monitors<height or#monitors[1]<width then term.clear()term.setCursorPos(1,1)print('This image needs monitors to be calibrated before being displayed. Please right-click each monitor in order, from the top left corner to the bottom right corner, going right first, then down.\\n')monitors={}local a={}for b=1,height do monitors[b]={}for c=1,width do local d,e=term.getCursorPos()for f=1,height do term.setCursorPos(3,e+f-1)term.clearLine()for g=1,width do term.blit('\\x8F ',g==c and f==b and'00'or'77','ff')end end;term.setCursorPos(3,e+height)term.write('('..c..', '..b..')')term.setCursorPos(1,e)repeat local d,h=os.pullEvent('monitor_touch')monitors[b][c]=h until not a[h]a[monitors[b][c]]=true;sleep(0.25)end end;settings.set('sanjuuni.multimonitor',monitors)settings.save()print('Calibration complete. Settings have been saved for future use.')end\n";

class HelpException: public OptionException {
public:
    virtual const char * className() const noexcept {return "HelpException";}
    virtual const char * name() const noexcept {return "Help";}
};

static std::string urlEncode(const std::string& tmppath) {
    static const char * hexstr = "0123456789ABCDEF";
    std::string path;
    for (size_t i = 0; i < tmppath.size(); i++) {
        char c = tmppath[i];
        if (isalnum(c) || (c == '%' && i + 2 < tmppath.size() && isxdigit(tmppath[i+1]) && isxdigit(tmppath[i+2]))) path += c;
        else {
            switch (c) {
                case '!': case '#': case '$': case '&': case '\'': case '(':
                case ')': case '*': case '+': case ',': case '/': case ':':
                case ';': case '=': case '?': case '@': case '[': case ']':
                case '-': case '_': case '.': case '~': path += c; break;
                default: path += '%'; path += hexstr[c >> 4]; path += hexstr[c & 0x0F];
            }
        }
    }
    return path;
}

template<class _Rep, class _Period>
std::ostream& operator<<(std::ostream& stream, const duration<_Rep, _Period>& duration) {
    if (duration >= hours(1)) stream << std::setw(2) << std::setfill('0') << std::right << duration_cast<hours>(duration).count() << ":";
    stream << std::setw(2) << std::setfill('0') << std::right << (duration_cast<minutes>(duration).count() % 60) << ":";
    stream << std::setw(2) << std::setfill('0') << std::right << (duration_cast<seconds>(duration).count() % 60);
    return stream;
}

static void sighandler(int signal) {
    std::unique_lock<std::mutex> lock(exitLock);
    exitNotify.notify_all();
}

static std::string avErrorString(int err) {
    char errstr[256];
    av_strerror(err, errstr, 256);
    return std::string(errstr);
}

static double parseTime(const std::string& str) {
    return (str[0] - '0') * 3600 + (str[2] - '0') * 600 + (str[3] - '0') * 60 + (str[5] - '0') * 10 + (str[6] - '0') * 1 + (str[8] - '0') * 0.1 + (str[9] - '0') * 0.01;
}

static Vec3b parseColor(const std::string& str) {
    uint32_t color;
    if (str.substr(0, 2) == "&H") color = std::stoul(str.substr(2), NULL, 16);
    else color = std::stoul(str);
    return {color & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF};
}

#ifndef NO_NET
class HTTPListener: public HTTPRequestHandler {
public:
    double *fps;
    HTTPListener(double *f): fps(f) {}
    void handleRequest(HTTPServerRequest &request, HTTPServerResponse &response) override {
        std::string path = request.getURI();
        if (path.empty() || path == "/") {
            std::string file = "local a='" + request.getHost() + playLua;
            response.setStatusAndReason(HTTPResponse::HTTP_OK);
            response.setContentType("text/x-lua");
            response.setContentLength(file.size());
            response.send().write(file.c_str(), file.size());
        } else if (path == "/info") {
            std::string file = "{\n    \"length\": " + std::to_string(frameStorage.size()) + ",\n    \"fps\": " + std::to_string(*fps) + "\n}";
            response.setStatusAndReason(HTTPResponse::HTTP_OK);
            response.setContentType("application/json");
            response.setContentLength(file.size());
            response.send().write(file.c_str(), file.size());
        } else if (path.substr(0, 7) == "/video/") {
            int frame;
            try {
                frame = std::stoi(path.substr(7));
            } catch (std::exception &e) {
                response.setStatusAndReason(HTTPResponse::HTTP_BAD_REQUEST);
                response.setContentType("text/plain");
                response.send().write("Invalid path", 12);
                return;
            }
            if (frame < 0 || frame >= frameStorage.size()) {
                response.setStatusAndReason(HTTPResponse::HTTP_NOT_FOUND);
                response.setContentType("text/plain");
                response.send().write("404 Not Found", 13);
                return;
            }
            response.setStatusAndReason(HTTPResponse::HTTP_OK);
            response.setContentType("text/x-lua");
            response.setContentLength(frameStorage[frame].size());
            response.send().write(frameStorage[frame].c_str(), frameStorage[frame].size());
        } else if (path.substr(0, 7) == "/audio/") {
            int frame;
            long size = useDFPWM ? 6000 : 48000;
            try {
                frame = std::stoi(path.substr(7));
            } catch (std::exception &e) {
                response.setStatusAndReason(HTTPResponse::HTTP_BAD_REQUEST);
                response.setContentType("text/plain");
                response.send().write("Invalid path", 12);
                return;
            }
            if (useDFPWM) frame *= 8;
            if (frame < 0 || frame > audioStorageSize / size) {
                response.setStatusAndReason(HTTPResponse::HTTP_NOT_FOUND);
                response.setContentType("text/plain");
                response.send().write("404 Not Found", 13);
                return;
            }
            response.setStatusAndReason(HTTPResponse::HTTP_OK);
            response.setContentType("application/octet-stream");
            size_t sz = frame == audioStorageSize / size ? audioStorageSize % size : size;
            response.setContentLength(sz);
            response.send().write((char*)(audioStorage + frame * size), sz);
        } else {
            response.setStatusAndReason(HTTPResponse::HTTP_NOT_FOUND);
            response.setContentType("text/plain");
            response.send().write("404 Not Found", 13);
        }
    }
    class Factory: public HTTPRequestHandlerFactory {
    public:
        double *fps;
        Factory(double *f): fps(f) {}
        HTTPRequestHandler* createRequestHandler(const HTTPServerRequest&) override {
            return new HTTPListener(fps);
        }
    };
};

static void serveWebSocket(WebSocket * ws, double * fps) {
    char buf[256];
    int n, flags;
    do {
        flags = 0;
        try {n = ws->receiveFrame(buf, 256, flags);}
        catch (Poco::TimeoutException &e) {continue;}
        if (n > 0) {
            //std::cout << std::string(buf, n) << "\n";
            if (streamed) {
                std::unique_lock<std::mutex> lock(streamedLock);
                streamedNotify.notify_all();
                streamedNotify.wait(lock);
            }
            if (buf[0] == 'v') {
                int frame = std::stoi(std::string(buf + 1, n - 1));
                if (frame >= frameStorage.size() || frame < 0) ws->sendFrame("!", 1, WebSocket::FRAME_TEXT);
                else for (size_t i = 0; i < frameStorage[frame].size(); i += 65535)
                    ws->sendFrame(frameStorage[frame].c_str() + i, min(frameStorage[frame].size() - i, (size_t)65535), WebSocket::FRAME_BINARY);
                if (streamed) frameStorage[frame] = "";
            } else if (buf[0] == 'a') {
                int offset = std::stoi(std::string(buf + 1, n - 1)) / (useDFPWM ? 8 : 1);
                long size = useDFPWM ? 6000 : 48000;
                if (streamed) {
                    if (audioStorageSize < size) {
                        audioStorage = (uint8_t*)realloc(audioStorage, size);
                        memset(audioStorage + max(audioStorageSize, 0L), 0, size - max(audioStorageSize, 0L));
                    }
                    if (audioStorageSize > -size) ws->sendFrame(audioStorageSize < 0 ? audioStorage - audioStorageSize : audioStorage, size, WebSocket::FRAME_BINARY);
                    else ws->sendFrame("!", 1, WebSocket::FRAME_TEXT);
                    if (audioStorageSize > size) memmove(audioStorage, audioStorage + size, audioStorageSize - size);
                    audioStorageSize = max(audioStorageSize - size, 0L); //audioStorageSize -= size;
                }
                else if (offset >= audioStorageSize || offset < 0) ws->sendFrame("!", 1, WebSocket::FRAME_TEXT);
                else ws->sendFrame(audioStorage + offset, offset + size > audioStorageSize ? audioStorageSize - offset : size, WebSocket::FRAME_BINARY);
            } else if (buf[0] == 'n') {
                std::string data = std::to_string(max((size_t)totalFrames, frameStorage.size()));
                ws->sendFrame(data.c_str(), data.size(), WebSocket::FRAME_TEXT);
            } else if (buf[0] == 'f') {
                std::string data = std::to_string(*fps);
                ws->sendFrame(data.c_str(), data.size(), WebSocket::FRAME_TEXT);
            }
        }
    } while (n > 0 && (flags & WebSocket::FRAME_OP_BITMASK) != WebSocket::FRAME_OP_CLOSE);
}

class WebSocketServer: public HTTPRequestHandler {
public:
    double *fps;
    WebSocketServer(double *f): fps(f) {}
    void handleRequest(HTTPServerRequest &request, HTTPServerResponse &response) override {
        try {
            WebSocket ws(request, response);
            serveWebSocket(&ws, fps);
            try {ws.shutdown();} catch (...) {}
        } catch (Poco::Exception &e) {
            std::cerr << "WebSocket exception: " << e.displayText() << "\n";
        } catch (std::exception &e) {
            std::cerr << "WebSocket exception: " << e.what() << "\n";
        }
    }
    class Factory: public HTTPRequestHandlerFactory {
    public:
        double *fps;
        Factory(double *f): fps(f) {}
        HTTPRequestHandler* createRequestHandler(const HTTPServerRequest&) override {
            return new WebSocketServer(fps);
        }
    };
};
#endif

// Very basic parser - no error checking
std::unordered_multimap<int, ASSSubtitleEvent> parseASSSubtitles(const std::string& path, double framerate) {
    std::unordered_multimap<int, ASSSubtitleEvent> retval;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> styles;
    std::vector<std::string> format;
    uint8_t wrapStyle = 0;
    unsigned width = 0, height = 0;
    double speed = 1.0;
    bool isASS = false;
    std::ifstream in(path);
    if (!in.is_open()) return retval;
    while (!in.eof()) {
        std::string line;
        std::getline(in, line);
        if (line[0] == ';' || line.empty() || std::all_of(line.begin(), line.end(), isspace)) continue;
        size_t colon = line.find(":");
        if (colon == std::string::npos) continue;
        std::string type = line.substr(0, colon), data = line.substr(colon + 1);
        int space;
        for (space = 0; isspace(data[space]); space++);
        if (space) data = data.substr(space);
        if (data.empty()) continue;
        for (space = data.size() - 1; isspace(data[space]); space--);
        if (space < data.size() - 1) data = data.substr(0, space + 1);
        if (data.empty()) continue;
        if (type == "ScriptType") isASS = data == "v4.00+" || data == "V4.00+";
        else if (type == "PlayResX") width = std::stoul(data);
        else if (type == "PlayResY") height = std::stoul(data);
        else if (type == "WrapStyle") wrapStyle = data[0] - '0';
        else if (type == "Timer") speed = std::stod(data) / 100.0;
        else if (type == "Format") {
            format.clear();
            for (size_t pos = 0; pos != std::string::npos; pos = data.find(',', pos))
                {if (pos) pos++; format.push_back(data.substr(pos, data.find(',', pos) - pos));}
        } else if (type == "Style") {
            std::unordered_map<std::string, std::string> style;
            for (size_t i = 0, pos = 0; i < format.size(); i++, pos = data.find(',', pos) + 1)
                style[format[i]] = data.substr(pos, data.find(',', pos) - pos);
            styles[style["Name"]] = style;
        } else if (type == "Dialogue") {
            std::unordered_map<std::string, std::string> params;
            for (size_t i = 0, pos = 0; i < format.size(); i++, pos = data.find(',', pos) + 1)
                params[format[i]] = data.substr(pos, i == format.size() - 1 ? SIZE_MAX : data.find(',', pos) - pos);
            int start = parseTime(params["Start"]) * framerate, end = parseTime(params["End"]) * framerate;
            std::unordered_map<std::string, std::string>& style = styles.find(params["Style"]) == styles.end() ? styles["Default"] : styles[params["Style"]];
            for (int i = start; i < end; i++) {
                ASSSubtitleEvent event;
                event.width = width;
                event.height = height;
                event.startFrame = start;
                event.length = end - start;
                event.alignment = std::stoi(style["Alignment"]);
                if (!isASS) {
                    switch (event.alignment) {
                        case 9: case 10: case 11: event.alignment--;
                        case 5: case 6: case 7: event.alignment--;
                    }
                }
                if (!event.alignment) event.alignment = 2;
                event.marginLeft = std::stoi(params["MarginL"]) == 0 ? std::stoi(style["MarginL"]) : std::stoi(params["MarginL"]);
                event.marginRight = std::stoi(params["MarginR"]) == 0 ? std::stoi(style["MarginR"]) : std::stoi(params["MarginR"]);
                event.marginVertical = std::stoi(params["MarginV"]) == 0 ? std::stoi(style["MarginV"]) : std::stoi(params["MarginV"]);
                event.color = parseColor(style["PrimaryColour"]);
                event.text = params["Text"];
                retval.insert(std::make_pair(i, event));
            }
        }
    }
    in.close();
    return retval;
}

void renderSubtitles(const std::unordered_multimap<int, ASSSubtitleEvent>& subtitles, int nframe, uchar * characters, uchar * colors, const std::vector<Vec3b>& palette, int width, int height, std::vector<Vid32SubtitleEvent*> * vid32subs = NULL) {
    auto range = subtitles.equal_range(nframe);
    for (auto it = range.first; it != range.second; it++) {
        double scaleX = (double)it->second.width / (double)width, scaleY = (double)it->second.height / (double)height;
        std::vector<std::string> lines;
        std::string cur;
        int color = 0;
        nearestColor(palette, it->second.color, &color);
        for (int i = 0; i < it->second.text.size(); i++) {
            // TODO: add effects
            if (it->second.text[i] == '\\' && (it->second.text[i+1] == 'n' || it->second.text[i+1] == 'N')) {
                lines.push_back(cur);
                cur = "";
                i++;
            } else if (it->second.text[i] == '{') i = it->second.text.find('}', i);
            else cur += it->second.text[i];
        }
        lines.push_back(cur);
        for (int i = 0; i < lines.size(); i++) {
            int startX = 0, startY = 0;
            switch (it->second.alignment) {
                case 1: startX = it->second.marginLeft / scaleX; startY = height - ((double)it->second.marginVertical / scaleY) - (lines.size()-i-1)*3 - 1; break;
                case 2: startX = width / 2 - lines[i].size(); startY = height - ((double)it->second.marginVertical / scaleY) - (lines.size()-i-1)*3 - 1; break;
                case 3: startX = width - ((double)it->second.marginRight / scaleX) - lines[i].size() - 1; startY = height - ((double)it->second.marginVertical / scaleY) - (lines.size()-i-1)*3 - 1; break;
                case 4: startX = it->second.marginLeft / scaleX; startY = it->second.marginVertical / scaleY + i*3; break;
                case 5: startX = width / 2 - lines[i].size(); startY = it->second.marginVertical / scaleY + i*3; break;
                case 6: startX = width - ((double)it->second.marginRight / scaleX) - lines[i].size() - 1; startY = it->second.marginVertical / scaleY + i*3; break;
                case 7: startX = it->second.marginLeft / scaleX; startY = (height - lines.size()) / 2 + i*3; break;
                case 8: startX = width / 2 - lines[i].size(); startY = (height - lines.size()) / 2 + i*3; break;
                case 9: startX = width - ((double)it->second.marginRight / scaleX) - lines[i].size() - 1; startY = (height - lines.size()) / 2 + i*3; break;
            }
            if (vid32subs != NULL) {
                if (it->second.startFrame == nframe) {
                    Vid32SubtitleEvent * ev = (Vid32SubtitleEvent*)malloc(sizeof(Vid32SubtitleEvent) + lines[i].size());
                    ev->start = nframe;
                    ev->length = it->second.length;
                    ev->x = startX / 2;
                    ev->y = startY / 3;
                    ev->colors = 0xF0 | color;
                    ev->flags = 0;
                    ev->size = lines[i].size();
                    memcpy(ev->text, lines[i].c_str(), lines[i].size());
                    vid32subs->push_back(ev);
                }
            } else {
                int start = (startY / 3) * (width / 2) + (startX / 2);
                for (int x = 0; x < lines[i].size(); x++) {
                    characters[start+x] = lines[i][x];
                    colors[start+x] = 0xF0 | color;
                }
            }
        }
    }
}

static std::unordered_multimap<int, ASSSubtitleEvent> subtitles;
static OpenCL::Device * device = NULL;
static std::string input, output, subtitle, format;
static bool useDefaultPalette = false, noDither = false, useOctree = false, useKmeans = false, mute = false, binary = false, ordered = false, useLab = false, disableOpenCL = false, separateStreams = false, trimBorders = false;
static OutputType mode = OutputType::Default;
static int compression = VID32_FLAG_VIDEO_COMPRESSION_ANS;
static int port = 80, width = -1, height = -1, zlibCompression = 5, customPaletteCount = 16, monitorWidth = 0, monitorHeight = 0, monitorArrayWidth = 0, monitorArrayHeight = 0, monitorScale = 1;
static Vec3b customPalette[16];
static uint16_t customPaletteMask = 0;

static void convertImage(Mat& rs, uchar ** characters, uchar ** colors, std::vector<Vec3b>& palette, size_t& width, size_t& height, int nframe) {
    Mat labImage = (!useLab || useDefaultPalette) ? rs : makeLabImage(rs, device);
    if (customPaletteMask == 0xFFFF) palette = std::vector<Vec3b>(customPalette, customPalette + 16);
    else if (useDefaultPalette) palette = defaultPalette;
    else if (useOctree) palette = reducePalette_octree(labImage, customPaletteCount, device);
    else if (useKmeans) palette = reducePalette_kMeans(labImage, customPaletteCount, device);
    else palette = reducePalette_medianCut(labImage, 16, device);
    if (customPaletteMask && customPaletteCount) {
        std::vector<Vec3b> newPalette(16);
        for (int i = 0; i < 16; i++) {
            if (customPaletteMask & (1 << i)) newPalette[i] = (useLab && !useDefaultPalette) ? convertColorToLab(customPalette[i]) : customPalette[i];
            else if (palette.size() == 16) newPalette[i] = palette[i];
            else {newPalette[i] = palette.back(); palette.pop_back();}
        }
        palette = newPalette;
    }
    Mat out;
    if (noDither) out = thresholdImage(labImage, palette, device);
    else if (ordered) out = ditherImage_ordered(labImage, palette, device);
    else out = ditherImage(labImage, palette, device);
    Mat1b pimg = rgbToPaletteImage(out, palette, device);
    if (useLab && !useDefaultPalette) palette = convertLabPalette(palette);
    makeCCImage(pimg, palette, characters, colors, device);
    if (!subtitle.empty() && mode != OutputType::Vid32) renderSubtitles(subtitles, nframe, *characters, *colors, palette, pimg.width, pimg.height);
    width = pimg.width; height = pimg.height;
}

int main(int argc, const char * argv[]) {
    OptionSet options;
    options.addOption(Option("input", "i", "Input image or video", true, "file", true));
    options.addOption(Option("subtitle", "S", "ASS-formatted subtitle file to add to the video", false, "file", true));
    options.addOption(Option("format", "f", "Force a format to use for the input file", false, "format", true));
    options.addOption(Option("output", "o", "Output file path", false, "path", true));
    options.addOption(Option("lua", "l", "Output a Lua script file (default)"));
    options.addOption(Option("nfp", "n", "Output an NFP format image for use in paint (changes proportions!)"));
    options.addOption(Option("raw", "r", "Output a rawmode-based image/video file"));
    options.addOption(Option("blit-image", "b", "Output a blit image (BIMG) format image/animation file"));
    options.addOption(Option("32vid", "3", "Output a 32vid format binary video file with compression + audio"));
    options.addOption(Option("http", "s", "Serve an HTTP server that has each frame split up + a player program", false, "port", true).validator(new IntValidator(1, 65535)));
    options.addOption(Option("websocket", "w", "Serve a WebSocket that sends the image/video with audio", false, "port", true).validator(new IntValidator(1, 65535)));
    options.addOption(Option("websocket-client", "u", "Connect to a WebSocket server to send image/video with audio", false, "url", true).validator(new RegExpValidator("^wss?://(?:[A-Za-z0-9!#$%&'*+\\-\\/=?^_`{|}~]+(?:\\.[A-Za-z0-9!#$%&'*+\\-\\/=?^_`{|}~]+)*|\\[[\x21-\x5A\x5E-\x7E]*\\])(?::\\d+)?(?:/[^/]+)*$")));
    options.addOption(Option("streamed", "T", "For servers, encode data on-the-fly instead of doing it ahead of time (saves memory at the cost of speed, and only one client can connect)"));
    options.addOption(Option("default-palette", "p", "Use the default CC palette instead of generating an optimized one"));
    options.addOption(Option("palette", "P", "Use a custom palette instead of generating one, or lock certain colors", false, "palette", true).validator(new RegExpValidator("^((#?[0-9a-fA-F]{6}|X?),){15}(#?[0-9a-fA-F]{6}|X)$")));
    options.addOption(Option("threshold", "t", "Use thresholding instead of dithering"));
    options.addOption(Option("ordered", "O", "Use ordered dithering"));
    options.addOption(Option("lab-color", "L", "Use CIELAB color space for higher quality color conversion"));
    options.addOption(Option("octree", "8", "Use octree for higher quality color conversion (slower)"));
    options.addOption(Option("kmeans", "k", "Use k-means for highest quality color conversion (slowest)"));
    options.addOption(Option("compression", "c", "Compression type for 32vid videos; available modes: none|ans|deflate|custom", false, "mode", true).validator(new RegExpValidator("^(none|lzw|deflate|custom)$")));
    options.addOption(Option("binary", "B", "Output blit image files in a more-compressed binary format (requires opening the file in binary mode)"));
    options.addOption(Option("separate-streams", "S", "Output 32vid files using separate streams (slower to decode)"));
    options.addOption(Option("dfpwm", "d", "Use DFPWM compression on audio"));
    options.addOption(Option("mute", "m", "Remove audio from output"));
    options.addOption(Option("width", "W", "Resize the image to the specified width", false, "size", true).validator(new IntValidator(1, 65535)));
    options.addOption(Option("height", "H", "Resize the image to the specified height", false, "size", true).validator(new IntValidator(1, 65535)));
    options.addOption(Option("monitor-size", "M", "Split the image into multiple parts for large monitors", false, "WxH[@S]", false).validator(new RegExpValidator("^[0-9]+x[0-9]+(?:@[0-5](?:\\.5)?)?$")));
    options.addOption(Option("trim-borders", "", "For multi-monitor images, skip pixels that would be hidden underneath monitor borders, keeping the image size consistent"));
    options.addOption(Option("disable-opencl", "", "Disable OpenCL computation; force CPU-only"));
    options.addOption(Option("help", "h", "Show this help"));
    OptionProcessor argparse(options);
    argparse.setUnixStyle(true);

    try {
        for (int i = 1; i < argc; i++) {
            std::string option, arg;
            if (argparse.process(argv[i], option, arg)) {
                if (option == "input") input = arg;
                else if (option == "subtitle") subtitle = arg;
                else if (option == "format") format = arg;
                else if (option == "output") output = arg;
                else if (option == "lua") mode = OutputType::Lua;
                else if (option == "nfp") mode = OutputType::NFP;
                else if (option == "raw") mode = OutputType::Raw;
                else if (option == "32vid") mode = OutputType::Vid32;
#ifndef NO_NET
                else if (option == "http") {mode = OutputType::HTTP; port = std::stoi(arg);}
                else if (option == "websocket") {mode = OutputType::WebSocket; port = std::stoi(arg);}
                else if (option == "websocket-client") {mode = OutputType::WebSocket; output = arg; port = 0;}
#endif
                else if (option == "blit-image") mode = OutputType::BlitImage;
                else if (option == "streamed") streamed = true;
                else if (option == "default-palette") useDefaultPalette = true;
                else if (option == "palette") {
                    for (size_t i = 0, pos = 0, end = arg.find_first_of(','); i < 16; i++, pos = end+1, end = arg.find_first_of(',', end+1)) {
                        std::string c = arg.substr(pos, end - pos);
                        if (!c.empty() && c != "X") {
                            const char * s = c.c_str();
                            if (*s == '#') s++;
                            long color = strtol(s, NULL, 16);
                            customPalette[i] = {color & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF};
                            customPaletteMask |= 1 << i;
                            customPaletteCount--;
                        }
                    }
                }
                else if (option == "threshold") noDither = true;
                else if (option == "ordered") ordered = true;
                else if (option == "lab-color") useLab = true;
                else if (option == "octree") useOctree = true;
                else if (option == "kmeans") useKmeans = true;
                else if (option == "compression") {
                    if (arg == "none") compression = VID32_FLAG_VIDEO_COMPRESSION_NONE;
                    else if (arg == "ans") compression = VID32_FLAG_VIDEO_COMPRESSION_ANS;
                    else if (arg == "deflate") compression = VID32_FLAG_VIDEO_COMPRESSION_DEFLATE;
                    else if (arg == "custom") compression = VID32_FLAG_VIDEO_COMPRESSION_CUSTOM;
                }
                else if (option == "binary") binary = true;
                else if (option == "dfpwm") useDFPWM = true;
                else if (option == "mute") mute = true;
                else if (option == "width") width = std::stoi(arg);
                else if (option == "height") height = std::stoi(arg);
                else if (option == "monitor-size") {
                    if (!arg.empty()) {
                        monitorArrayWidth = std::stoi(arg);
                        monitorArrayHeight = std::stoi(arg.substr(arg.find_first_of('x') + 1));
                        size_t pos = arg.find_first_of('@');
                        if (pos != std::string::npos) monitorScale = std::stod(arg.substr(pos + 1)) * 2;
                        monitorWidth = round((64*monitorArrayWidth - 20) / (6 * (monitorScale / 2.0))) * 2;
                        monitorHeight = round((64*monitorArrayHeight - 20) / (9 * (monitorScale / 2.0))) * 3;
                    } else {monitorArrayWidth = 8; monitorArrayHeight = 6; monitorWidth = 328; monitorHeight = 243; monitorScale = 1;}
                }
                else if (option == "trim-borders") trimBorders = true;
                else if (option == "disable-opencl") disableOpenCL = true;
                else if (option == "help") throw HelpException();
            }
        }
        argparse.checkRequired();
        if (!(mode == OutputType::HTTP || mode == OutputType::WebSocket) && output == "") throw MissingOptionException("Required option not specified: output");
        if (monitorWidth && mode != OutputType::Default && mode != OutputType::Lua && mode != OutputType::BlitImage && !(mode == OutputType::Vid32 && !separateStreams)) throw InvalidArgumentException("Monitor splitting is only supported on Lua, BIMG, and 32vid outputs.");
    } catch (const OptionException &e) {
        if (e.className() != "HelpException") std::cerr << e.displayText() << "\n";
        HelpFormatter help(options);
#ifdef SIZE_IOCTL
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        help.setWidth(w.ws_col);
#endif
#ifdef SIZE_WIN
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
        help.setWidth(csbi.srWindow.Right - csbi.srWindow.Left + 1);
#endif
        help.setUnixStyle(true);
        help.setUsage("[options] -i <input> [-o <output> | -s <port> | -w <port> | -u <url>]");
        help.setCommand(argv[0]);
        help.setHeader("sanjuuni converts images and videos into a format that can be displayed in ComputerCraft.");
        help.setFooter("sanjuuni is licensed under the GPL license. Get the source at https://github.com/MCJack123/sanjuuni.");
        help.format(e.className() == "HelpException" ? std::cout : std::cerr);
        return e.className() != "HelpException";
    }

    if (useDFPWM) {
#if (LIBAVCODEC_VERSION_MAJOR > 59 || (LIBAVCODEC_VERSION_MAJOR == 59 && LIBAVCODEC_VERSION_MINOR >= 22)) && \
    (LIBAVFORMAT_VERSION_MAJOR > 59 || (LIBAVFORMAT_VERSION_MAJOR == 59 && LIBAVFORMAT_VERSION_MINOR >= 18))
#define HAS_DFPWM 1
        if (avcodec_version() < AV_VERSION_INT(59, 22, 0) || avformat_version() < AV_VERSION_INT(59, 18, 0)) {
#else
#pragma warning ("DFPWM support not detected, -d will be disabled.")
#endif
            std::cerr << "DFPWM output requires FFmpeg 5.1 or later\nlavc " << avcodec_version() << ", lavf " << avformat_version() << "\n";
            return 2;
#ifdef HAS_DFPWM
        }
#endif
    }

    AVFormatContext * format_ctx = NULL;
    AVCodecContext * video_codec_ctx = NULL, * audio_codec_ctx = NULL, * dfpwm_codec_ctx = NULL;
#if LIBAVCODEC_VERSION_MAJOR >= 59
    const AVInputFormat * wanted_format = NULL;
#else
    AVInputFormat * wanted_format = NULL;
#endif
    const AVCodec * video_codec = NULL, * audio_codec = NULL, * dfpwm_codec = NULL;
    SwsContext * resize_ctx = NULL;
    SwrContext * resample_ctx = NULL;
    const AVFilter * asetnsamples_filter = NULL, * src_filter = NULL, * sink_filter = NULL;
    AVFilterContext * asetnsamples_ctx = NULL, * src_ctx = NULL, * sink_ctx = NULL;
    AVFilterGraph * filter_graph = NULL;
    int error, video_stream = -1, audio_stream = -1;
    // Open video file
    avdevice_register_all();
    if (!format.empty()) {
        wanted_format = av_find_input_format(format.c_str());
        if (wanted_format == NULL) std::cerr << "Warning: Could not find desired input format '" << format << "', automatically detecting.\n";
    }
    if ((error = avformat_open_input(&format_ctx, input.c_str(), wanted_format, NULL)) < 0) {
        std::cerr << "Could not open input file: " << avErrorString(error) << "\n";
        return error;
    }
    if ((error = avformat_find_stream_info(format_ctx, NULL)) < 0) {
        std::cerr << "Could not read stream info: " << avErrorString(error) << "\n";
        avformat_close_input(&format_ctx);
        return error;
    }
    // Search for video (and audio?) stream indexes
    for (int i = 0; i < format_ctx->nb_streams && (video_stream < 0 || audio_stream < 0); i++) {
        if (video_stream < 0 && format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) video_stream = i;
        else if (audio_stream < 0 && format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) audio_stream = i;
    }
    if (video_stream < 0) {
        std::cerr << "Could not find any video streams\n";
        avformat_close_input(&format_ctx);
        return 2;
    }
    // Open the video decoder
    if (!(video_codec = avcodec_find_decoder(format_ctx->streams[video_stream]->codecpar->codec_id))) {
        std::cerr << "Could not find video codec\n";
        avformat_close_input(&format_ctx);
        return 2;
    }
    if (!(video_codec_ctx = avcodec_alloc_context3(video_codec))) {
        std::cerr << "Could not allocate video codec context\n";
        avformat_close_input(&format_ctx);
        return 2;
    }
    if ((error = avcodec_parameters_to_context(video_codec_ctx, format_ctx->streams[video_stream]->codecpar)) < 0) {
        std::cerr << "Could not initialize video codec parameters: " << avErrorString(error) << "\n";
        avcodec_free_context(&video_codec_ctx);
        avformat_close_input(&format_ctx);
        return error;
    }
    if ((error = avcodec_open2(video_codec_ctx, video_codec, NULL)) < 0) {
        std::cerr << "Could not open video codec: " << avErrorString(error) << "\n";
        avcodec_free_context(&video_codec_ctx);
        avformat_close_input(&format_ctx);
        return error;
    }
    if (mode == OutputType::Default) mode = OutputType::Lua;
    if (mode == OutputType::Vid32 && !separateStreams) {
        if (!(filter_graph = avfilter_graph_alloc())) {
            std::cerr << "Could not allocate filter graph\n";
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return 2;
        }
        if (!(asetnsamples_filter = avfilter_get_by_name("asetnsamples"))) {
            std::cerr << "Could not find audio filter 'asetnsamples'\n";
            avfilter_graph_free(&filter_graph);
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return 2;
        }
        if (!(src_filter = avfilter_get_by_name("abuffer"))) {
            std::cerr << "Could not find audio filter 'abuffer'\n";
            avfilter_graph_free(&filter_graph);
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return 2;
        }
        if (!(sink_filter = avfilter_get_by_name("abuffersink"))) {
            std::cerr << "Could not find audio filter 'abuffersink'\n";
            avfilter_graph_free(&filter_graph);
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return 2;
        }
        if (!(asetnsamples_ctx = avfilter_graph_alloc_filter(filter_graph, asetnsamples_filter, NULL))) {
            std::cerr << "Could not add audio filter 'asetnsamples'\n";
            avfilter_graph_free(&filter_graph);
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return 2;
        }
        if ((error = avfilter_init_str(asetnsamples_ctx, "n=24000")) < 0) {
            std::cerr << "Could not initialize audio filter 'asetnsamples'\n";
            avfilter_free(asetnsamples_ctx);
            avfilter_graph_free(&filter_graph);
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return 2;
        }
        if (!(src_ctx = avfilter_graph_alloc_filter(filter_graph, src_filter, NULL))) {
            std::cerr << "Could not add audio filter 'abuffer'\n";
            avfilter_free(asetnsamples_ctx);
            avfilter_graph_free(&filter_graph);
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return 2;
        }
        if ((error = avfilter_init_str(src_ctx, "sample_rate=48000:sample_fmt=u8:channel_layout=mono")) < 0) {
            std::cerr << "Could not initialize audio filter 'abuffer'\n";
            avfilter_free(src_ctx);
            avfilter_free(asetnsamples_ctx);
            avfilter_graph_free(&filter_graph);
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return 2;
        }
        if (!(sink_ctx = avfilter_graph_alloc_filter(filter_graph, sink_filter, NULL))) {
            std::cerr << "Could not add audio filter 'abuffersink'\n";
            avfilter_free(src_ctx);
            avfilter_free(asetnsamples_ctx);
            avfilter_graph_free(&filter_graph);
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return 2;
        }
        if ((error = avfilter_init_str(sink_ctx, NULL)) < 0) {
            std::cerr << "Could not initialize audio filter 'abuffersink'\n";
            avfilter_free(sink_ctx);
            avfilter_free(src_ctx);
            avfilter_free(asetnsamples_ctx);
            avfilter_graph_free(&filter_graph);
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return 2;
        }
        if ((error = avfilter_link(src_ctx, 0, asetnsamples_ctx, 0)) < 0) {
            std::cerr << "Could not link audio filter\n";
            avfilter_free(sink_ctx);
            avfilter_free(src_ctx);
            avfilter_free(asetnsamples_ctx);
            avfilter_graph_free(&filter_graph);
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return 2;
        }
        if ((error = avfilter_link(asetnsamples_ctx, 0, sink_ctx, 0)) < 0) {
            std::cerr << "Could not link audio filter\n";
            avfilter_free(sink_ctx);
            avfilter_free(src_ctx);
            avfilter_free(asetnsamples_ctx);
            avfilter_graph_free(&filter_graph);
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return 2;
        }
        if ((error = avfilter_graph_config(filter_graph, NULL)) < 0) {
            std::cerr << "Could not configure audio filter\n";
            avfilter_free(sink_ctx);
            avfilter_free(src_ctx);
            avfilter_free(asetnsamples_ctx);
            avfilter_graph_free(&filter_graph);
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return 2;
        }
    }
    // Open the audio decoder if present
    if (audio_stream >= 0) {
        if (!(audio_codec = avcodec_find_decoder(format_ctx->streams[audio_stream]->codecpar->codec_id))) {
            std::cerr << "Could not find audio codec\n";
            if (sink_ctx) avfilter_free(sink_ctx);
            if (src_ctx) avfilter_free(src_ctx);
            if (asetnsamples_ctx) avfilter_free(asetnsamples_ctx);
            if (filter_graph) avfilter_graph_free(&filter_graph);
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return 2;
        }
        if (!(audio_codec_ctx = avcodec_alloc_context3(audio_codec))) {
            std::cerr << "Could not allocate audio codec context\n";
            if (sink_ctx) avfilter_free(sink_ctx);
            if (src_ctx) avfilter_free(src_ctx);
            if (asetnsamples_ctx) avfilter_free(asetnsamples_ctx);
            if (filter_graph) avfilter_graph_free(&filter_graph);
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return 2;
        }
        if ((error = avcodec_parameters_to_context(audio_codec_ctx, format_ctx->streams[audio_stream]->codecpar)) < 0) {
            std::cerr << "Could not initialize audio codec parameters: " << avErrorString(error) << "\n";
            if (sink_ctx) avfilter_free(sink_ctx);
            if (src_ctx) avfilter_free(src_ctx);
            if (asetnsamples_ctx) avfilter_free(asetnsamples_ctx);
            if (filter_graph) avfilter_graph_free(&filter_graph);
            avcodec_free_context(&audio_codec_ctx);
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return error;
        }
        if ((error = avcodec_open2(audio_codec_ctx, audio_codec, NULL)) < 0) {
            std::cerr << "Could not open audio codec: " << avErrorString(error) << "\n";
            if (sink_ctx) avfilter_free(sink_ctx);
            if (src_ctx) avfilter_free(src_ctx);
            if (asetnsamples_ctx) avfilter_free(asetnsamples_ctx);
            if (filter_graph) avfilter_graph_free(&filter_graph);
            avcodec_free_context(&audio_codec_ctx);
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return error;
        }
    }
    // Open DFPWM encoder if required
#ifdef HAS_DFPWM
    if (useDFPWM) {
        if (!(dfpwm_codec = avcodec_find_encoder(AV_CODEC_ID_DFPWM))) {
            std::cerr << "Could not find DFPWM codec\n";
            if (sink_ctx) avfilter_free(sink_ctx);
            if (src_ctx) avfilter_free(src_ctx);
            if (asetnsamples_ctx) avfilter_free(asetnsamples_ctx);
            if (filter_graph) avfilter_graph_free(&filter_graph);
            avcodec_free_context(&audio_codec_ctx);
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return error;
        }
        if (!(dfpwm_codec_ctx = avcodec_alloc_context3(dfpwm_codec))) {
            std::cerr << "Could not allocate DFPWM codec context\n";
            if (sink_ctx) avfilter_free(sink_ctx);
            if (src_ctx) avfilter_free(src_ctx);
            if (asetnsamples_ctx) avfilter_free(asetnsamples_ctx);
            if (filter_graph) avfilter_graph_free(&filter_graph);
            avcodec_free_context(&audio_codec_ctx);
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return error;
        }
        dfpwm_codec_ctx->sample_fmt = AV_SAMPLE_FMT_U8;
        dfpwm_codec_ctx->sample_rate = 48000;
        dfpwm_codec_ctx->ch_layout = AV_CHANNEL_LAYOUT_MONO;
        if ((error = avcodec_open2(dfpwm_codec_ctx, dfpwm_codec, NULL)) < 0) {
            std::cerr << "Could not open DFPWM codec: " << avErrorString(error) << "\n";
            if (sink_ctx) avfilter_free(sink_ctx);
            if (src_ctx) avfilter_free(src_ctx);
            if (asetnsamples_ctx) avfilter_free(asetnsamples_ctx);
            if (filter_graph) avfilter_graph_free(&filter_graph);
            avcodec_free_context(&dfpwm_codec_ctx);
            avcodec_free_context(&audio_codec_ctx);
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return error;
        }
    }
#endif
    // Initialize packets/frames
    AVPacket * packet = av_packet_alloc(), * dfpwm_packet = useDFPWM ? av_packet_alloc() : NULL;
    AVFrame * frame = av_frame_alloc();

    std::ofstream outfile;
    if (output != "-" && output != "" && mode != OutputType::WebSocket) {
        outfile.open(output, std::ios::out | std::ios::binary);
        if (!outfile.good()) {
            std::cerr << "Could not open output file!\n";
            av_frame_free(&frame);
            av_packet_free(&packet);
            avcodec_free_context(&video_codec_ctx);
            if (sink_ctx) avfilter_free(sink_ctx);
            if (src_ctx) avfilter_free(src_ctx);
            if (asetnsamples_ctx) avfilter_free(asetnsamples_ctx);
            if (filter_graph) avfilter_graph_free(&filter_graph);
            if (audio_codec_ctx) avcodec_free_context(&audio_codec_ctx);
            avformat_close_input(&format_ctx);
            return 1;
        }
    }
    std::ostream& outstream = (output == "-" || output == "") ? std::cout : outfile;
#ifndef NO_NET
    HTTPServer * srv = NULL;
    WebSocket* ws = NULL;
#endif
#ifdef USE_SDL
    SDL_Window * win = NULL;
#endif
    std::string videoStream;
    std::vector<Vid32SubtitleEvent*> vid32subs;
    std::stringstream vid32stream;
    double fps = 0;
    int nframe = 0, nframe_vid32 = 0;
    auto start = system_clock::now();
    auto lastUpdate = system_clock::now() - seconds(1);
    bool first = true;
    int64_t totalDuration = 0;
#ifndef NO_NET
    if (mode == OutputType::HTTP) {
        srv = new HTTPServer(new HTTPListener::Factory(&fps), port);
        srv->start();
        signal(SIGINT, sighandler);
    } else if (mode == OutputType::WebSocket) {
        if (port == 0) {
            Poco::URI uri;
            try {
                uri = Poco::URI(output);
            } catch (Poco::SyntaxException &e) {
                std::cerr << "Failed to parse URL: " << e.displayText() << "\n";
                goto cleanup;
            }
            if (uri.getHost() == "localhost") uri.setHost("127.0.0.1");
            HTTPClientSession * cs;
            if (uri.getScheme() == "ws") cs = new HTTPClientSession(uri.getHost(), uri.getPort());
            else if (uri.getScheme() == "wss") {
                Context::Ptr ctx = new Context(Context::CLIENT_USE, "", Context::VERIFY_NONE, 9, true, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
#if POCO_VERSION >= 0x010A0000
                ctx->disableProtocols(Context::PROTO_TLSV1_3);
#endif
                cs = new HTTPSClientSession(uri.getHost(), uri.getPort(), ctx);
            } else {
                std::cerr << "Invalid scheme (this should never happen)\n";
                goto cleanup;
            }
            size_t pos = output.find('/', output.find(uri.getHost()));
            size_t hash = pos != std::string::npos ? output.find('#', pos) : std::string::npos;
            std::string path = urlEncode(pos != std::string::npos ? output.substr(pos, hash - pos) : "/");
            HTTPRequest request(HTTPRequest::HTTP_GET, path, HTTPMessage::HTTP_1_1);
            request.add("User-Agent", "sanjuuni/1.0");
            request.add("Accept-Charset", "UTF-8");
            HTTPResponse response;
            try {
                ws = new WebSocket(*cs, request, response);
            } catch (Poco::Exception &e) {
                std::cerr << "Failed to open WebSocket: " << e.displayText() << "\n";
                goto cleanup;
            } catch (std::exception &e) {
                std::cerr << "Failed to open WebSocket: " << e.what() << "\n";
                goto cleanup;
            }
            std::thread(serveWebSocket, ws, &fps).detach();
        } else {
            srv = new HTTPServer(new WebSocketServer::Factory(&fps), port);
            srv->start();
            signal(SIGINT, sighandler);
        }
    }
#endif

#ifdef HAS_OPENCL
    if (!disableOpenCL) {
        try {
            device = new OpenCL::Device(OpenCL::select_device_with_most_flops());
            /*Mat testImage(2, 2, device);
            testImage.at(0, 0) = {255, 0, 0};
            testImage.at(0, 1) = {255, 255, 0};
            testImage.at(1, 0) = {0, 255, 0};
            testImage.at(1, 1) = {0, 0, 255};
            Mat expected = ditherImage_ordered(testImage, {{255, 192, 128}, {0, 64, 96}}, NULL);
            Mat actual = ditherImage_ordered(testImage, {{255, 192, 128}, {0, 64, 96}}, device);
            expected.download();
            actual.download();
            printf("%d %d %d => %d %d %d / %d %d %d\n",
                testImage.at(0, 0).x, testImage.at(0, 0).y, testImage.at(0, 0).z,
                expected.at(0, 0).x, expected.at(0, 0).y, expected.at(0, 0).z,
                actual.at(0, 0).x, actual.at(0, 0).y, actual.at(0, 0).z);
            printf("%d %d %d => %d %d %d / %d %d %d\n",
                testImage.at(0, 1).x, testImage.at(0, 1).y, testImage.at(0, 1).z,
                expected.at(0, 1).x, expected.at(0, 1).y, expected.at(0, 1).z,
                actual.at(0, 1).x, actual.at(0, 1).y, actual.at(0, 1).z);
            printf("%d %d %d => %d %d %d / %d %d %d\n",
                testImage.at(1, 0).x, testImage.at(1, 0).y, testImage.at(1, 0).z,
                expected.at(1, 0).x, expected.at(1, 0).y, expected.at(1, 0).z,
                actual.at(1, 0).x, actual.at(1, 0).y, actual.at(1, 0).z);
            printf("%d %d %d => %d %d %d / %d %d %d\n",
                testImage.at(1, 1).x, testImage.at(1, 1).y, testImage.at(1, 1).z,
                expected.at(1, 1).x, expected.at(1, 1).y, expected.at(1, 1).z,
                actual.at(1, 1).x, actual.at(1, 1).y, actual.at(1, 1).z);*/
        } catch (std::exception &e) {
            std::cerr << "Warning: Could not open OpenCL device: " << e.what() << ". Falling back to CPU computation.\n";
            device = NULL;
        }
    }
#endif

#ifdef USE_SDL
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    SDL_Init(SDL_INIT_VIDEO);
#endif

    totalFrames = format_ctx->streams[video_stream]->nb_frames;
    while (av_read_frame(format_ctx, packet) >= 0) {
        if (packet->stream_index == video_stream) {
            avcodec_send_packet(video_codec_ctx, packet);
            fps = av_q2d(video_codec_ctx->framerate);
            /*if (fps < 1 && format_ctx->streams[video_stream]->nb_frames > 1) {
                std::cerr << "Variable framerate files are not supported.\n";
                av_packet_unref(packet);
                goto cleanup;
            }*/
            if (first) {
                if (!subtitle.empty()) subtitles = parseASSSubtitles(subtitle, fps);
                if (mode == OutputType::Raw) outstream << "32Vid 1.1\n" << fps << "\n";
                else if (mode == OutputType::BlitImage) outstream << (binary ? "{" : "{\n");
                first = false;
            }
            while ((error = avcodec_receive_frame(video_codec_ctx, frame)) == 0) {
                auto now = system_clock::now();
                if (now - lastUpdate > milliseconds(250)) {
                    auto t = now - start;
#ifdef STATUS_FUNCTION
                    STATUS_FUNCTION(nframe++, format_ctx->streams[video_stream]->nb_frames, duration_cast<milliseconds>(t), nframe > 0 ? duration_cast<milliseconds>((t * totalFrames / nframe) - t) : milliseconds(0), t >= seconds(1) ? floor((double)nframe / duration_cast<seconds>(t).count()) : 0);
#else
                    std::cerr << "\rframe " << nframe++ << "/" << format_ctx->streams[video_stream]->nb_frames << " (elapsed " << t << ", remaining " << ((t * totalFrames / nframe) - t) << ", " << floor((double)nframe / duration_cast<seconds>(t).count()) << " fps)";
                    std::cerr.flush();
#endif
                    lastUpdate = now;
                } else nframe++;
                totalDuration += frame->duration;
                if (resize_ctx == NULL) {
                    if (width != -1 || height != -1) {
                        width = width == -1 ? height * ((double)frame->width / (double)frame->height) : width;
                        height = height == -1 ? width * ((double)frame->height / (double)frame->width) : height;
                    } else {
                        width = frame->width;
                        height = frame->height;
                    }
                    if (monitorWidth && width <= monitorWidth && height <= monitorHeight) {
                        monitorWidth = 0;
                        monitorHeight = 0;
                    } else if (monitorWidth && mode == OutputType::Lua) {
                        outstream << "local width,height=" << ceil((double)width / (double)monitorWidth) << "," << ceil((double)height / (double)monitorHeight) << ";" << multiMonitorLua;
                    }
                    resize_ctx = sws_getContext(frame->width, frame->height, (AVPixelFormat)frame->format, width, height, AV_PIX_FMT_BGR24, SWS_BICUBIC, NULL, NULL, NULL);
                    if (mode == OutputType::Vid32 && !separateStreams) {
                        Vid32Chunk combinedChunk;
                        Vid32Header header;
                        combinedChunk.nframes = totalFrames;
                        combinedChunk.type = (uint8_t)Vid32Chunk::Type::Combined;
                        memcpy(header.magic, "32VD", 4);
                        header.width = width / 2;
                        header.height = height / 3;
                        header.fps = floor(fps + 0.5);
                        header.nstreams = 1;
                        header.flags = compression | VID32_FLAG_VIDEO_5BIT_CODES;
                        if (useDFPWM) header.flags |= VID32_FLAG_AUDIO_COMPRESSION_DFPWM;
                        if (monitorWidth) {
                            header.flags |= VID32_FLAG_VIDEO_MULTIMONITOR |
                                VID32_FLAG_VIDEO_MULTIMONITOR_WIDTH(width / monitorWidth) |
                                VID32_FLAG_VIDEO_MULTIMONITOR_HEIGHT(height / monitorHeight);
                        }
                        outstream.write((char*)&header, 12);
                        outstream.write((char*)&combinedChunk, 9);
                    }
                }
                Mat rs(width, height+1, device);
                uint8_t * data = (uint8_t*)rs.vec.data();
                int stride[3] = {width * 3, width * 3, width * 3};
                uint8_t * ptrs[3] = {data, data + 1, data + 2};
                sws_scale(resize_ctx, frame->data, frame->linesize, 0, frame->height, ptrs, stride);
                rs.remove_last_line();
                if (monitorWidth) {
                    for (int y = 0, my = 1; y < height; my++, y += (trimBorders ? monitorArrayHeight * 128 / monitorScale / 3 : monitorHeight)) {
                        for (int x = 0, mx = 1; x < width; mx++, x += (trimBorders ? monitorArrayWidth * 128 / monitorScale / 3 : monitorWidth)) {
                            int mw = min(width - x, monitorWidth), mh = min(height - y, monitorHeight);
                            Mat crop(mw, mh, device);
                            for (int line = 0; line < mh; line++) {
                                memcpy(crop.vec.data() + line * mw, rs.vec.data() + (y + line) * width + x, mw * sizeof(uchar3));
                            }
                            uchar *characters, *colors;
                            std::vector<Vec3b> palette;
                            size_t w, h;
                            convertImage(crop, &characters, &colors, palette, w, h, nframe);
                            if (mode == OutputType::Lua) outstream << "do local m,i,p=peripheral.wrap(monitors[" << my << "][" << mx << "])," << makeTable(characters, colors, palette, w / 2, h / 3, true) << "m.clear()m.setTextScale(" << (monitorScale / 2.0) << ")for i=0,#p do m.setPaletteColor(2^i,table.unpack(p[i]))end for y,r in ipairs(i)do m.setCursorPos(1,y)m.blit(table.unpack(r))end end\n";
                            else if (mode == OutputType::BlitImage) outstream << makeTable(characters, colors, palette, w / 2, h / 3, binary, true, binary) << (binary ? "," : ",\n");
                            else if (mode == OutputType::Vid32) {
                                std::string data;
                                if (compression == VID32_FLAG_VIDEO_COMPRESSION_CUSTOM) data = make32vid_cmp(characters, colors, palette, w / 2, h / 3);
                                else if (compression == VID32_FLAG_VIDEO_COMPRESSION_ANS) data = make32vid_ans(characters, colors, palette, w / 2, h / 3);
                                else data = make32vid(characters, colors, palette, w / 2, h / 3);
                                if (compression == VID32_FLAG_VIDEO_COMPRESSION_DEFLATE) {
                                    unsigned long size = compressBound(videoStream.size());
                                    uint8_t * buf = new uint8_t[size];
                                    error = compress2(buf, &size, (const uint8_t*)videoStream.c_str(), videoStream.size(), compression);
                                    if (error != Z_OK) {
                                        std::cerr << "Could not compress video!\n";
                                        delete[] buf;
                                        goto cleanup;
                                    }
                                    data = std::string((const char*)buf + 2, size - 6);
                                    delete[] buf;
                                }
                                uint32_t size = data.size();
                                vid32stream.write((const char*)&size, 4);
                                vid32stream.put((char)Vid32Chunk::Type::MultiMonitorVideo | ((mx - 1) << 3) | (my - 1));
                                uint16_t tmp = w / 2;
                                vid32stream.write((char*)&tmp, 2);
                                tmp = h / 3;
                                vid32stream.write((char*)&tmp, 2);
                                vid32stream.write(data.c_str(), data.size());
                                nframe_vid32++;
                            }
                            outstream.flush();
                            delete[] characters;
                            delete[] colors;
                        }
                    }
                    // TODO: subtitles?
                } else {
                    uchar *characters, *colors;
                    std::vector<Vec3b> palette;
                    size_t w, h;
                    convertImage(rs, &characters, &colors, palette, w, h, nframe);
                    switch (mode) {
                    case OutputType::Lua: {
                        outstream << makeLuaFile(characters, colors, palette, w / 2, h / 3) << "sleep(" << (frame->duration * av_q2d(format_ctx->streams[video_stream]->time_base)) << ")\n";
                        outstream.flush();
                        break;
                    } case OutputType::NFP: {
                        outstream << makeNFP(characters, colors, palette, w / 2, h / 3);
                        outstream.flush();
                        break;
                    } case OutputType::Raw: {
                        outstream << makeRawImage(characters, colors, palette, w / 2, h / 3);
                        outstream.flush();
                        break;
                    } case OutputType::BlitImage: {
                        outstream << makeTable(characters, colors, palette, w / 2, h / 3, binary, true, binary) << (binary ? "," : ",\n");
                        outstream.flush();
                        break;
                    } case OutputType::Vid32: {
                        if (separateStreams) {
                            if (compression == VID32_FLAG_VIDEO_COMPRESSION_CUSTOM) videoStream += make32vid_cmp(characters, colors, palette, w / 2, h / 3);
                            else if (compression == VID32_FLAG_VIDEO_COMPRESSION_ANS) videoStream += make32vid_ans(characters, colors, palette, w / 2, h / 3);
                            else videoStream += make32vid(characters, colors, palette, w / 2, h / 3);
                            renderSubtitles(subtitles, nframe, NULL, NULL, palette, w, h, &vid32subs);
                        } else {
                            std::string data;
                            if (compression == VID32_FLAG_VIDEO_COMPRESSION_CUSTOM) data = make32vid_cmp(characters, colors, palette, w / 2, h / 3);
                            else if (compression == VID32_FLAG_VIDEO_COMPRESSION_ANS) data = make32vid_ans(characters, colors, palette, w / 2, h / 3);
                            else data = make32vid(characters, colors, palette, w / 2, h / 3);
                            if (compression == VID32_FLAG_VIDEO_COMPRESSION_DEFLATE) {
                                unsigned long size = compressBound(videoStream.size());
                                uint8_t * buf = new uint8_t[size];
                                error = compress2(buf, &size, (const uint8_t*)videoStream.c_str(), videoStream.size(), compression);
                                if (error != Z_OK) {
                                    std::cerr << "Could not compress video!\n";
                                    delete[] buf;
                                    goto cleanup;
                                }
                                data = std::string((const char*)buf + 2, size - 6);
                                delete[] buf;
                            }
                            uint32_t size = data.size();
                            vid32stream.write((const char*)&size, 4);
                            vid32stream.put((char)Vid32Chunk::Type::Video);
                            vid32stream.write(data.c_str(), data.size());
                            nframe_vid32++;
                            std::vector<Vid32SubtitleEvent*> subs;
                            renderSubtitles(subtitles, nframe, NULL, NULL, palette, w, h, &subs);
                            for (Vid32SubtitleEvent * sub : subs) {
                                size = sizeof(Vid32SubtitleEvent) + sub->size;
                                vid32stream.write((const char*)&size, 4);
                                vid32stream.put((char)Vid32Chunk::Type::Subtitle);
                                vid32stream.write((char*)sub, size);
                                free(sub);
                                nframe_vid32++;
                            }
                        }
                        break;
                    } case OutputType::HTTP: case OutputType::WebSocket: {
                        frameStorage.push_back("return " + makeTable(characters, colors, palette, w / 2, h / 3, true));
                        break;
                    }
                    }
#ifdef USE_SDL
                    if (!win) win = SDL_CreateWindow("Image", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN);
                    SDL_Surface * surf = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_BGRA32);
                    for (int i = 0; i < out.vec.size(); i++) ((uint32_t*)surf->pixels)[i] = 0xFF000000 | (out.vec[i].z << 16) | (out.vec[i].y << 8) | out.vec[i].x;
                    SDL_BlitSurface(surf, NULL, SDL_GetWindowSurface(win), NULL);
                    SDL_FreeSurface(surf);
                    SDL_UpdateWindowSurface(win);
#endif
                    delete[] characters;
                    delete[] colors;
                }
                if (streamed) {
                    std::unique_lock<std::mutex> lock(streamedLock);
                    streamedNotify.notify_all();
                    streamedNotify.wait(lock);
                }
            }
            if (error != AVERROR_EOF && error != AVERROR(EAGAIN)) {
                std::cerr << "Failed to grab video frame: " << avErrorString(error) << "\n";
            }
        } else if (packet->stream_index == audio_stream && mode != OutputType::Lua && mode != OutputType::Raw && mode != OutputType::BlitImage && mode != OutputType::NFP && !mute) {
            avcodec_send_packet(audio_codec_ctx, packet);
            while ((error = avcodec_receive_frame(audio_codec_ctx, frame)) == 0) {
                AVFrame * newframe = av_frame_alloc();
                newframe->ch_layout = AV_CHANNEL_LAYOUT_MONO;
                if (!resample_ctx) {
                    if ((error = swr_alloc_set_opts2(&resample_ctx, &newframe->ch_layout, AV_SAMPLE_FMT_U8, 48000, &frame->ch_layout, (AVSampleFormat)frame->format, frame->sample_rate, 0, NULL)) < 0) {
                        std::cerr << "Failed to initialize resampler: " << avErrorString(error) << "\n";
                        av_frame_free(&newframe);
                        continue;
                    }
                }
                newframe->format = AV_SAMPLE_FMT_U8;
                newframe->sample_rate = 48000;
                if ((error = swr_convert_frame(resample_ctx, newframe, frame)) < 0) {
                    std::cerr << "Failed to convert audio: " << avErrorString(error) << "\n";
                    av_frame_free(&newframe);
                    continue;
                }
                if (filter_graph) {
                    if ((error = av_buffersrc_add_frame(src_ctx, newframe)) < 0) {
                        std::cerr << "Could not push frame to filter: " << avErrorString(error) << "\n";
                        av_frame_free(&newframe);
                        continue;
                    }
                    av_frame_free(&newframe);
                    AVFrame * newframe2 = av_frame_alloc();
                    if ((error = av_buffersink_get_frame(sink_ctx, newframe2)) < 0) {
                        //std::cerr << "Could not pull frame from filter: " << avErrorString(error) << "\n";
                        av_frame_free(&newframe2);
                        continue;
                    }
                    newframe = newframe2;
                }
                if (useDFPWM) {
                    if ((error = avcodec_send_frame(dfpwm_codec_ctx, newframe)) < 0) {
                        std::cerr << "Could not write DFPWM frame: " << avErrorString(error) << "\n";
                        av_frame_free(&newframe);
                        continue;
                    }
                    while (error >= 0) {
                        error = avcodec_receive_packet(dfpwm_codec_ctx, dfpwm_packet);
                        if (error == AVERROR(EAGAIN) || error == AVERROR_EOF) break;
                        else if (error < 0) {
                            std::cerr << "Could not read DFPWM frame: " << avErrorString(error) << "\n";
                            break;
                        }
                        if (audioStorageSize + dfpwm_packet->size > 0) {
                            unsigned offset = audioStorageSize < 0 ? -audioStorageSize : 0;
                            audioStorage = (uint8_t*)realloc(audioStorage, audioStorageSize + dfpwm_packet->size);
                            memcpy(audioStorage + audioStorageSize + offset, dfpwm_packet->data + offset, dfpwm_packet->size - offset);
                        }
                        audioStorageSize += dfpwm_packet->size;
                        av_packet_unref(dfpwm_packet);
                    }
                } else {
                    if (audioStorageSize + newframe->nb_samples > 0) {
                        unsigned offset = audioStorageSize < 0 ? -audioStorageSize : 0;
                        audioStorage = (uint8_t*)realloc(audioStorage, audioStorageSize + newframe->nb_samples);
                        memcpy(audioStorage + audioStorageSize + offset, newframe->data[0] + offset, newframe->nb_samples - offset);
                    }
                    audioStorageSize += newframe->nb_samples;
                }
                if (mode == OutputType::Vid32 && !separateStreams) {
                    uint32_t size = audioStorageSize;
                    outstream.write((const char*)&size, 4);
                    outstream.put((char)Vid32Chunk::Type::Audio);
                    outstream.write((const char*)audioStorage, size);
                    nframe_vid32++;
                    free(audioStorage);
                    audioStorage = NULL;
                    audioStorageSize = 0;
                    std::string vdata = vid32stream.str();
                    outstream.write(vdata.c_str(), vdata.size());
                    vid32stream = std::stringstream();
                }
                av_frame_free(&newframe);
            }
            if (error != AVERROR_EOF && error != AVERROR(EAGAIN)) {
                std::cerr << "Failed to grab audio frame: " << avErrorString(error) << "\n";
            }
        }
        av_packet_unref(packet);
#ifdef STATUS_FUNCTION
        if (externalStop) break;
#endif
    }
    if (fps < 1) {
        fps = nframe / (totalDuration * av_q2d(format_ctx->streams[video_stream]->time_base));
        if (mode == OutputType::Vid32 && !separateStreams) {
            auto pos = outstream.tellp();
            outstream.seekp(8, std::ios::beg);
            outstream.put(floor(fps + 0.5));
            outstream.seekp(pos, std::ios::beg);
        }
    }
    if (mode == OutputType::Vid32 && separateStreams) {
        Vid32Chunk videoChunk, audioChunk;
        Vid32Header header;
        videoChunk.nframes = nframe;
        videoChunk.type = (uint8_t)Vid32Chunk::Type::Video;
        audioChunk.size = audioStorageSize;
        audioChunk.nframes = audioStorageSize;
        audioChunk.type = (uint8_t)Vid32Chunk::Type::Audio;
        memcpy(header.magic, "32VD", 4);
        header.width = width / 2;
        header.height = height / 3;
        header.fps = floor(fps + 0.5);
        header.nstreams = (vid32subs.empty() ? 0 : 1) + (audioStorage ? 1 : 0) + 1;
        header.flags = compression | VID32_FLAG_VIDEO_5BIT_CODES;
        if (useDFPWM) header.flags |= VID32_FLAG_AUDIO_COMPRESSION_DFPWM;

        outfile.write((char*)&header, 12);
        if (compression == VID32_FLAG_VIDEO_COMPRESSION_DEFLATE) {
            unsigned long size = compressBound(videoStream.size());
            uint8_t * buf = new uint8_t[size];
            error = compress2(buf, &size, (const uint8_t*)videoStream.c_str(), videoStream.size(), compression);
            if (error != Z_OK) {
                std::cerr << "Could not compress video!\n";
                delete[] buf;
                goto cleanup;
            }
            videoChunk.size = size;
            outfile.write((char*)&videoChunk, 9);
            outfile.write((char*)buf + 2, size - 6);
            delete[] buf;
        } else {
            videoChunk.size = videoStream.size();
            outfile.write((char*)&videoChunk, 9);
            outfile.write(videoStream.c_str(), videoStream.size());
        }
        if (audioStorage) {
            if (useDFPWM) audioChunk.nframes *= 8;
            outfile.write((char*)&audioChunk, 9);
            outfile.write((char*)audioStorage, audioStorageSize);
        }
        if (!vid32subs.empty()) {
            Vid32Chunk subtitleChunk;
            subtitleChunk.nframes = vid32subs.size();
            subtitleChunk.size = 0;
            for (int i = 0; i < vid32subs.size(); i++) subtitleChunk.size += sizeof(Vid32SubtitleEvent) + vid32subs[i]->size;
            subtitleChunk.type = (uint8_t)Vid32Chunk::Type::Subtitle;
            outfile.write((char*)&subtitleChunk, 9);
            for (int i = 0; i < vid32subs.size(); i++) {
                outfile.write((char*)vid32subs[i], sizeof(Vid32SubtitleEvent) + vid32subs[i]->size);
                free(vid32subs[i]);
            }
            vid32subs.clear();
        }
    } else if (mode == OutputType::Vid32 && !separateStreams) {
        std::string vdata = vid32stream.str();
        outstream.write(vdata.c_str(), vdata.size());
        vid32stream = std::stringstream();
        Vid32Chunk chunk;
        chunk.size = (size_t)outstream.tellp() - 21;
        chunk.nframes = nframe_vid32;
        chunk.type = (uint8_t)Vid32Chunk::Type::Combined;
        outstream.seekp(12, std::ios::beg);
        outstream.write((const char*)&chunk, 9);
    } else if (mode == OutputType::BlitImage) {
        char timestr[26];
        time_t now = time(0);
        struct tm * time = gmtime(&now);
        strftime(timestr, 26, "%FT%T%z", time);
        if (monitorWidth) {
            if (binary) outfile << "multiMonitor={width=" << ceil((double)width / (double)monitorWidth) << ",height=" << ceil((double)height / (double)monitorHeight) << ",scale=" << (monitorScale / 2.0) << "},";
            else outfile << "multiMonitor = {\n    width = " << ceil((double)width / (double)monitorWidth) << ",\n    height = " << ceil((double)height / (double)monitorHeight) << ",\n    scale = " << (monitorScale / 2.0) << "\n},\n";
        }
        if (binary) outfile << "creator='sanjuuni',version='1.0.0',secondsPerFrame=" << (1.0 / fps) << ",animation=" << (nframe > 1 ? "true" : "false") << ",date='" << timestr << "',title='" << input << "'}";
        else outfile << "creator = 'sanjuuni',\nversion = '1.0.0',\nsecondsPerFrame = " << (1.0 / fps) << ",\nanimation = " << (nframe > 1 ? "true" : "false") << ",\ndate = '" << timestr << "',\ntitle = '" << input << "'\n}\n";
    } else if (mode == OutputType::Lua) {
        if (nframe == 1) outfile << "read()\n";
        outfile << "for i = 0, 15 do term.setPaletteColor(2^i, term.nativePaletteColor(2^i)) end\nterm.setBackgroundColor(colors.black)\nterm.setTextColor(colors.white)\nterm.setCursorPos(1, 1)\nterm.clear()\n";
    }
cleanup:
    auto t = system_clock::now() - start;
#ifdef STATUS_FUNCTION
    STATUS_FUNCTION(nframe, nframe, duration_cast<milliseconds>(t), milliseconds(0), t >= seconds(1) ? floor((double)nframe / duration_cast<seconds>(t).count()) : 0);
#else
    std::cerr << "\rframe " << nframe << "/" << nframe << " (elapsed " << t << ", remaining 00:00, " << floor((double)nframe / duration_cast<seconds>(t).count()) << " fps)\n";
#endif
#ifdef HAS_OPENCL
    if (device != NULL) delete device;
#endif
    if (outfile.is_open()) outfile.close();
    if (resize_ctx) sws_freeContext(resize_ctx);
    if (resample_ctx) swr_free(&resample_ctx);
    av_frame_free(&frame);
    av_packet_free(&packet);
    if (dfpwm_packet) av_packet_free(&dfpwm_packet);
    if (sink_ctx) avfilter_free(sink_ctx);
    if (src_ctx) avfilter_free(src_ctx);
    if (asetnsamples_ctx) avfilter_free(asetnsamples_ctx);
    if (filter_graph) avfilter_graph_free(&filter_graph);
    avcodec_free_context(&video_codec_ctx);
    if (audio_codec_ctx) avcodec_free_context(&audio_codec_ctx);
    if (dfpwm_codec_ctx) avcodec_free_context(&dfpwm_codec_ctx);
    avformat_close_input(&format_ctx);
#ifndef NO_NET
    if (ws) {
#ifdef STATUS_FUNCTION
        if (!externalStop)
#endif
        if (!streamed) {
            std::cout << "Serving on port " << port << "\n";
            std::unique_lock<std::mutex> lock(exitLock);
            exitNotify.wait(lock);
        }
        ws->shutdown();
        delete ws;
    } else if (srv) {
#ifdef STATUS_FUNCTION
        if (!externalStop)
#endif
        if (!streamed) {
            std::cout << "Serving on port " << port << "\n";
            std::unique_lock<std::mutex> lock(exitLock);
            exitNotify.wait(lock);
        }
        srv->stop();
        delete srv;
    }
#endif
    if (audioStorage) free(audioStorage);
    audioStorage = NULL;
    audioStorageSize = totalFrames = 0;
    frameStorage.clear();
#ifdef USE_SDL
    while (true) {
        SDL_Event e;
        SDL_WaitEvent(&e);
        if (e.type == SDL_QUIT) break;
    }
    SDL_DestroyWindow(win);
    SDL_Quit();
#endif
    return 0;
}
