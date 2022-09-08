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
using namespace Poco::Net;

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

WorkQueue work;
static std::vector<std::string> frameStorage;
static uint8_t * audioStorage = NULL;
static long audioStorageSize = 0, totalFrames = 0;
static std::mutex exitLock, streamedLock;
static std::condition_variable exitNotify, streamedNotify;
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
static const std::string playLua = "'local function b(c)local d,e=http.get('http://'..a..c,nil,true)if not d then error(e)end;local f=d.readAll()d.close()return f end;local g=textutils.unserializeJSON(b('/info'))local h,i={},{}local j=peripheral.find'speaker'term.clear()local k=2;parallel.waitForAll(function()for l=0,g.length-1 do h[l]=b('/video/'..l)if k>0 then k=k-1 end end end,function()for l=0,g.length/g.fps do i[l]=b('/audio/'..l)if k>0 then k=k-1 end end end,function()while k>0 do os.pullEvent()end;local m=os.epoch'utc'for l=0,g.length-1 do while not h[l]do os.pullEvent()end;local n=h[l]h[l]=nil;local o,p=assert(load(n,'=frame','t',{}))()for q=0,#p do term.setPaletteColor(2^q,table.unpack(p[q]))end;for s,t in ipairs(o)do term.setCursorPos(1,s)term.blit(table.unpack(t))end;while os.epoch'utc'<m+(l+1)/g.fps*1000 do sleep(1/g.fps)end end end,function()if not j or not j.playAudio then return end;while k>0 do os.pullEvent()end;local u=0;while u<g.length/g.fps do while not i[u]do os.pullEvent()end;local v=i[u]i[u]=nil;v={v:byte(1,-1)}for q=1,#v do v[q]=v[q]-128 end;u=u+1;if not j.playAudio(v)or _HOST:find('v2.6.4')then repeat local w,x=os.pullEvent('speaker_audio_empty')until x==peripheral.getName(j)end end end)for q=0,15 do term.setPaletteColor(2^q,term.nativePaletteColor(2^q))end;term.setBackgroundColor(colors.black)term.setTextColor(colors.white)term.setCursorPos(1,1)term.clear()";

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
            response.send().write(file.c_str(), file.size());
        } else if (path == "/info") {
            std::string file = "{\n    \"length\": " + std::to_string(frameStorage.size()) + ",\n    \"fps\": " + std::to_string(*fps) + "\n}";
            response.setStatusAndReason(HTTPResponse::HTTP_OK);
            response.setContentType("application/json");
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
            response.send().write((char*)(audioStorage + frame * size), frame == audioStorageSize / size ? audioStorageSize % size : size);
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
                std::string data = std::to_string(totalFrames);
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
        for (space = data.size() - 1; isspace(data[space]); space--);
        if (space < data.size() - 1) data = data.substr(0, space + 1);
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

int main(int argc, const char * argv[]) {
    OptionSet options;
    options.addOption(Option("input", "i", "Input image or video", true, "file", true));
    options.addOption(Option("subtitle", "S", "ASS-formatted subtitle file to add to the video", false, "file", true));
    options.addOption(Option("output", "o", "Output file path", false, "path", true));
    options.addOption(Option("lua", "l", "Output a Lua script file (default for images; only does one frame)"));
    options.addOption(Option("nfp", "n", "Output an NFP format image for use in paint (changes proportions!)"));
    options.addOption(Option("raw", "r", "Output a rawmode-based image/video file (default for videos)"));
    options.addOption(Option("blit-image", "b", "Output a blit image (BIMG) format image/animation file"));
    options.addOption(Option("32vid", "3", "Output a 32vid format binary video file with compression + audio"));
    options.addOption(Option("http", "s", "Serve an HTTP server that has each frame split up + a player program", false, "port", true).validator(new IntValidator(1, 65535)));
    options.addOption(Option("websocket", "w", "Serve a WebSocket that sends the image/video with audio", false, "port", true).validator(new IntValidator(1, 65535)));
    options.addOption(Option("websocket-client", "u", "Connect to a WebSocket server to send image/video with audio", false, "url", true).validator(new RegExpValidator("^wss?://(?:[A-Za-z0-9!#$%&'*+\\-\\/=?^_`{|}~]+(?:\\.[A-Za-z0-9!#$%&'*+\\-\\/=?^_`{|}~]+)*|\\[[\x21-\x5A\x5E-\x7E]*\\])(?::\\d+)?(?:/[^/]+)*$")));
    options.addOption(Option("streamed", "T", "For servers, encode data on-the-fly instead of doing it ahead of time (saves memory at the cost of speed, and only one client can connect)"));
    options.addOption(Option("default-palette", "p", "Use the default CC palette instead of generating an optimized one"));
    options.addOption(Option("threshold", "t", "Use thresholding instead of dithering"));
    options.addOption(Option("octree", "8", "Use octree for higher quality color conversion (slower)"));
    options.addOption(Option("kmeans", "k", "Use k-means for highest quality color conversion (slowest)"));
    options.addOption(Option("compression", "c", "Compression type for 32vid videos; available modes: none|lzw|deflate|custom", false, "mode", true).validator(new RegExpValidator("^(none|lzw|deflate|custom)$")));
    options.addOption(Option("compression-level", "L", "Compression level for 32vid videos when using DEFLATE", false, "1-9", true).validator(new IntValidator(1, 9)));
    options.addOption(Option("dfpwm", "d", "Use DFPWM compression on audio"));
    options.addOption(Option("mute", "m", "Remove audio from output"));
    options.addOption(Option("width", "W", "Resize the image to the specified width", false, "size", true).validator(new IntValidator(1, 65535)));
    options.addOption(Option("height", "H", "Resize the image to the specified height", false, "size", true).validator(new IntValidator(1, 65535)));
    options.addOption(Option("help", "h", "Show this help"));
    OptionProcessor argparse(options);
    argparse.setUnixStyle(true);

    std::string input, output, subtitle;
    bool useDefaultPalette = false, noDither = false, useOctree = false, useKmeans = false, mute = false;
    OutputType mode = OutputType::Default;
    int compression = VID32_FLAG_VIDEO_COMPRESSION_CUSTOM;
    int port = 80, width = -1, height = -1, zlibCompression = 5;
    try {
        for (int i = 1; i < argc; i++) {
            std::string option, arg;
            if (argparse.process(argv[i], option, arg)) {
                if (option == "input") input = arg;
                else if (option == "subtitle") subtitle = arg;
                else if (option == "output") output = arg;
                else if (option == "lua") mode = OutputType::Lua;
                else if (option == "nfp") mode = OutputType::NFP;
                else if (option == "raw") mode = OutputType::Raw;
                else if (option == "32vid") mode = OutputType::Vid32;
                else if (option == "http") {mode = OutputType::HTTP; port = std::stoi(arg);}
                else if (option == "websocket") {mode = OutputType::WebSocket; port = std::stoi(arg);}
                else if (option == "websocket-client") {mode = OutputType::WebSocket; output = arg; port = 0;}
                else if (option == "blit-image") mode = OutputType::BlitImage;
                else if (option == "streamed") streamed = true;
                else if (option == "default-palette") useDefaultPalette = true;
                else if (option == "threshold") noDither = true;
                else if (option == "octree") useOctree = true;
                else if (option == "kmeans") useKmeans = true;
                else if (option == "compression") {
                    if (arg == "none") compression = VID32_FLAG_VIDEO_COMPRESSION_NONE;
                    else if (arg == "lzw") compression = VID32_FLAG_VIDEO_COMPRESSION_LZW;
                    else if (arg == "deflate") compression = VID32_FLAG_VIDEO_COMPRESSION_DEFLATE;
                    else if (arg == "custom") compression = VID32_FLAG_VIDEO_COMPRESSION_CUSTOM;
                } else if (option == "compression-level") zlibCompression = std::stoi(arg);
                else if (option == "dfpwm") useDFPWM = true;
                else if (option == "mute") mute = true;
                else if (option == "width") width = std::stoi(arg);
                else if (option == "height") height = std::stoi(arg);
                else if (option == "help") throw HelpException();
            }
        }
        argparse.checkRequired();
        if (!(mode == OutputType::HTTP || mode == OutputType::WebSocket) && output == "") throw MissingOptionException("Required option not specified: output");
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
        return 0;
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
    const AVCodec * video_codec = NULL, * audio_codec = NULL, * dfpwm_codec = NULL;
    SwsContext * resize_ctx = NULL;
    SwrContext * resample_ctx = NULL;
    int error, video_stream = -1, audio_stream = -1;
    std::unordered_multimap<int, ASSSubtitleEvent> subtitles;
    // Open video file
    if ((error = avformat_open_input(&format_ctx, input.c_str(), NULL, NULL)) < 0) {
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
    if (mode == OutputType::Default) mode = format_ctx->streams[video_stream]->nb_frames > 0 ? OutputType::Raw : OutputType::Lua;
    // Open the audio decoder if present
    if (audio_stream >= 0) {
        if (!(audio_codec = avcodec_find_decoder(format_ctx->streams[audio_stream]->codecpar->codec_id))) {
            std::cerr << "Could not find audio codec\n";
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return 2;
        }
        if (!(audio_codec_ctx = avcodec_alloc_context3(audio_codec))) {
            std::cerr << "Could not allocate audio codec context\n";
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return 2;
        }
        if ((error = avcodec_parameters_to_context(audio_codec_ctx, format_ctx->streams[audio_stream]->codecpar)) < 0) {
            std::cerr << "Could not initialize audio codec parameters: " << avErrorString(error) << "\n";
            avcodec_free_context(&audio_codec_ctx);
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return error;
        }
        if ((error = avcodec_open2(audio_codec_ctx, audio_codec, NULL)) < 0) {
            std::cerr << "Could not open audio codec: " << avErrorString(error) << "\n";
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
            avcodec_free_context(&audio_codec_ctx);
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return error;
        }
        if (!(dfpwm_codec_ctx = avcodec_alloc_context3(dfpwm_codec))) {
            std::cerr << "Could not allocate DFPWM codec context\n";
            avcodec_free_context(&audio_codec_ctx);
            avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&format_ctx);
            return error;
        }
        dfpwm_codec_ctx->sample_fmt = AV_SAMPLE_FMT_U8;
        dfpwm_codec_ctx->sample_rate = 48000;
        dfpwm_codec_ctx->channels = 1;
        dfpwm_codec_ctx->channel_layout = AV_CH_LAYOUT_MONO;
        if ((error = avcodec_open2(dfpwm_codec_ctx, dfpwm_codec, NULL)) < 0) {
            std::cerr << "Could not open DFPWM codec: " << avErrorString(error) << "\n";
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
        outfile.open(output);
        if (!outfile.good()) {
            std::cerr << "Could not open output file!\n";
            av_frame_free(&frame);
            av_packet_free(&packet);
            avcodec_free_context(&video_codec_ctx);
            if (audio_codec_ctx) avcodec_free_context(&audio_codec_ctx);
            avformat_close_input(&format_ctx);
            return 1;
        }
    }
    std::ostream& outstream = (output == "-" || output == "") ? std::cout : outfile;
    HTTPServer * srv = NULL;
    WebSocket* ws = NULL;
    std::string videoStream;
    std::vector<Vid32SubtitleEvent*> vid32subs;
    double fps = 0;
    int nframe = 0;
    auto start = system_clock::now();
    auto lastUpdate = system_clock::now() - seconds(1);
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

#ifdef USE_SDL
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window * win = NULL;
#endif

    totalFrames = format_ctx->streams[video_stream]->nb_frames;
    while (av_read_frame(format_ctx, packet) >= 0) {
        if (packet->stream_index == video_stream) {
            avcodec_send_packet(video_codec_ctx, packet);
            fps = (double)video_codec_ctx->framerate.num / (double)video_codec_ctx->framerate.den;
            if (nframe == 0) {
                if (!subtitle.empty()) subtitles = parseASSSubtitles(subtitle, fps);
                if (mode == OutputType::Raw) outstream << "32Vid 1.1\n" << fps << "\n";
                else if (mode == OutputType::BlitImage) outstream << "{\n";
            }
            while ((error = avcodec_receive_frame(video_codec_ctx, frame)) == 0) {
                auto now = system_clock::now();
                if (now - lastUpdate > milliseconds(250)) {
                    auto t = now - start;
                    std::cerr << "\rframe " << nframe++ << "/" << format_ctx->streams[video_stream]->nb_frames << " (elapsed " << t << ", remaining " << ((t * totalFrames / nframe) - t) << ", " << floor((double)nframe / duration_cast<seconds>(t).count()) << " fps)";
                    std::cerr.flush();
                    lastUpdate = now;
                } else nframe++;
                Mat out;
                if (resize_ctx == NULL) {
                    if (width != -1 || height != -1) {
                        width = width == -1 ? height * ((double)frame->width / (double)frame->height) : width;
                        height = height == -1 ? width * ((double)frame->height / (double)frame->width) : height;
                    } else {
                        width = frame->width;
                        height = frame->height;
                    }
                    resize_ctx = sws_getContext(frame->width, frame->height, (AVPixelFormat)frame->format, width, height, AV_PIX_FMT_BGR24, SWS_BICUBIC, NULL, NULL, NULL);
                }
                Mat rs(width, height);
                uint8_t * data = new uint8_t[width * height * 4];
                int stride[3] = {width * 3, width * 3, width * 3};
                uint8_t * ptrs[3] = {data, data + 1, data + 2};
                sws_scale(resize_ctx, frame->data, frame->linesize, 0, frame->height, ptrs, stride);
                for (int y = 0; y < height; y++)
                    for (int x = 0; x < width; x++)
                        rs.at(y, x) = {data[y*width*3+x*3], data[y*width*3+x*3+1], data[y*width*3+x*3+2]};
                delete[] data;
                std::vector<Vec3b> palette;
                if (useDefaultPalette) palette = defaultPalette;
                else if (useOctree) palette = reducePalette_octree(rs, 16);
                else if (useKmeans) palette = reducePalette_kMeans(rs, 16);
                else palette = reducePalette_medianCut(rs, 16);
                if (noDither) out = thresholdImage(rs, palette);
                else out = ditherImage(rs, palette);
                Mat1b pimg = rgbToPaletteImage(out, palette);
                uchar *characters, *colors;
                makeCCImage(pimg, palette, &characters, &colors);
                if (!subtitle.empty() && mode != OutputType::Vid32) renderSubtitles(subtitles, nframe, characters, colors, palette, pimg.width, pimg.height);
                switch (mode) {
                case OutputType::Lua: {
                    outstream << makeLuaFile(characters, colors, palette, pimg.width / 2, pimg.height / 3);
                    outstream.flush();
                    break;
                } case OutputType::NFP: {
                    outstream << makeNFP(characters, colors, palette, pimg.width / 2, pimg.height / 3);
                    outstream.flush();
                    break;
                } case OutputType::Raw: {
                    outstream << makeRawImage(characters, colors, palette, pimg.width / 2, pimg.height / 3);
                    outstream.flush();
                    break;
                } case OutputType::BlitImage: {
                    outstream << makeTable(characters, colors, palette, pimg.width / 2, pimg.height / 3, false, true) << ",\n";
                    outstream.flush();
                    break;
                } case OutputType::Vid32: {
                    if (compression == VID32_FLAG_VIDEO_COMPRESSION_CUSTOM) videoStream += make32vid_cmp(characters, colors, palette, pimg.width / 2, pimg.height / 3);
                    else videoStream += make32vid(characters, colors, palette, pimg.width / 2, pimg.height / 3);
                    renderSubtitles(subtitles, nframe, NULL, NULL, palette, pimg.width, pimg.height, &vid32subs);
                    break;
                } case OutputType::HTTP: case OutputType::WebSocket: {
                    frameStorage.push_back("return " + makeTable(characters, colors, palette, pimg.width / 2, pimg.height / 3, true));
                    break;
                }
                }
#ifdef USE_SDL
                if (!win) win = SDL_CreateWindow("Image", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN);
                SDL_Surface * surf = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_BGRA32);
                for (int i = 0; i < out.vec.size(); i++) ((uint32_t*)surf->pixels)[i] = 0xFF000000 | (out.vec[i][2] << 16) | (out.vec[i][1] << 8) | out.vec[i][0];
                SDL_BlitSurface(surf, NULL, SDL_GetWindowSurface(win), NULL);
                SDL_FreeSurface(surf);
                SDL_UpdateWindowSurface(win);
#endif
                delete[] characters;
                delete[] colors;
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
                if (!resample_ctx) resample_ctx = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_MONO, AV_SAMPLE_FMT_U8, 48000, frame->channel_layout, (AVSampleFormat)frame->format, frame->sample_rate, 0, NULL);
                AVFrame * newframe = av_frame_alloc();
                newframe->channel_layout = AV_CH_LAYOUT_MONO;
                newframe->format = AV_SAMPLE_FMT_U8;
                newframe->sample_rate = 48000;
                if ((error = swr_convert_frame(resample_ctx, newframe, frame)) < 0) {
                    std::cerr << "Failed to convert audio: " << avErrorString(error) << "\n";
                    continue;
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
                av_frame_free(&newframe);
            }
            if (error != AVERROR_EOF && error != AVERROR(EAGAIN)) {
                std::cerr << "Failed to grab audio frame: " << avErrorString(error) << "\n";
            }
        }
        av_packet_unref(packet);
    }
    if (mode == OutputType::Vid32) {
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
        header.nstreams = !vid32subs.empty() ? 3 : 2;
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
        } else if (compression == VID32_FLAG_VIDEO_COMPRESSION_LZW) {
            // TODO
            std::cerr << "LZW not implemented yet\n";
            goto cleanup;
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
    } else if (mode == OutputType::BlitImage) {
        char timestr[26];
        time_t now = time(0);
        struct tm * time = gmtime(&now);
        strftime(timestr, 26, "%FT%T%z", time);
        outfile << "creator = 'sanjuuni',\nversion = '1.0.0',\nsecondsPerFrame = " << (1.0 / fps) << ",\nanimation = " << (nframe > 1 ? "true" : "false") << ",\ndate = '" << timestr << "',\ntitle = '" << input << "'\n}\n";
    }
cleanup:
    std::cerr << "\rframe " << nframe << "/" << format_ctx->streams[video_stream]->nb_frames << "\n";
    if (outfile.is_open()) outfile.close();
    if (resize_ctx) sws_freeContext(resize_ctx);
    if (resample_ctx) swr_free(&resample_ctx);
    av_frame_free(&frame);
    av_packet_free(&packet);
    if (dfpwm_packet) av_packet_free(&dfpwm_packet);
    avcodec_free_context(&video_codec_ctx);
    if (audio_codec_ctx) avcodec_free_context(&audio_codec_ctx);
    if (dfpwm_codec_ctx) avcodec_free_context(&dfpwm_codec_ctx);
    avformat_close_input(&format_ctx);
    if (ws) {
        if (!streamed) {
            std::cout << "Serving on port " << port << "\n";
            std::unique_lock<std::mutex> lock(exitLock);
            exitNotify.wait(lock);
        }
        ws->shutdown();
        delete ws;
    } else if (srv) {
        if (!streamed) {
            std::cout << "Serving on port " << port << "\n";
            std::unique_lock<std::mutex> lock(exitLock);
            exitNotify.wait(lock);
        }
        srv->stop();
        delete srv;
    }
    if (audioStorage) free(audioStorage);
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
