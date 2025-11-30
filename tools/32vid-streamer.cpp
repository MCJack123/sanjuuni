/*
 * 32vid-streamer.cpp
 * Streams 32vid files on a sanjuuni-compatible WebSocket server.
 * Copyright (C) 2025 JackMacWindows
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

#include <csignal>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/WebSocket.h>
#include "sanjuuni.hpp"

using namespace std::chrono;
using namespace Poco::Net;

WorkQueue work;
std::mutex exitLock;
std::condition_variable exitNotify;
static std::vector<std::string> frameStorage;
static uint8_t * audioStorage = NULL;
static long audioStorageSize = 0, totalFrames = 0;
static std::vector<Vid32SubtitleEvent> subtitles;
static std::mutex streamedLock;
static std::condition_variable streamedNotify;
static bool streamed = false;
static bool useDFPWM = false;

struct bitlen_t {
    uint8_t s;
    uint8_t l;
    uint8_t c;
};

struct tree_node {
    tree_node * left = NULL;
    tree_node * right = NULL;
    size_t weight = 0;
    uint8_t data = 0;
    ~tree_node() {
        if (left) delete left;
        if (right) delete right;
    }
    bool operator<(const tree_node& rhs) {return weight >= rhs.weight;}
    void clear() {
        if (left) delete left;
        if (right) delete right;
        left = right = NULL;
    }
};

static int log2i(unsigned int n) {
    if (n == 0) return 0;
    int i = 0;
    while (n) {i++; n >>= 1;}
    return i - 1;
}

static void serveWebSocket(WebSocket * ws, double * fps) {
    char buf[256];
    int n, flags;
    do {
        flags = 0;
        try {n = ws->receiveFrame(buf, 256, flags);}
        catch (Poco::TimeoutException &e) {continue;}
        if (n > 0) {
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

void destroyCodeTree(tree_node * node) {
    if (node->left) destroyCodeTree(node->left);
    if (node->right) destroyCodeTree(node->right);
    delete node;
}

struct ANSEntry {
    uint32_t X;
    uint8_t s;
    uint8_t n;
};

template<bool isColor>
struct ANSDecoder {
    std::vector<ANSEntry> decodingTable;
    uint8_t R;
    uint32_t X;
    std::istream& in;
    uint32_t partial;
    uint8_t bits;

    ANSDecoder(std::istream& inf): in(inf) {
        R = in.get();
        uint32_t L = 1 << R;
        uint32_t Ls[isColor ? 24 : 32];
        for (int i = 0; i < isColor ? 24 : 32; i += 2) {
            uint8_t b = in.get();
            Ls[i] = b >> 4;
            Ls[i+1] = b & 15;
        }
        if (R == 0) {
            X = in.get();
            R = 0;
            return;
        }
        uint32_t a = 0;
        for (int i = 0; i < sizeof(Ls) / 4; i++) Ls[i] = Ls[i] == 0 ? 0 : 1 << (Ls[i]-1), a += Ls[i];
        decodingTable = std::vector<ANSEntry>(L);
        uint32_t x = 0;
        uint32_t step = L >> 1 + L >> 3 + 3;
        uint32_t * next = new uint32_t[sizeof(Ls) / 4];
        uint8_t * symbol = new uint8_t[L];
        memset(symbol, 0xFF, L);
        for (int i = 0; i < sizeof(Ls) / 4; i++) {
            next[i] = Ls[i];
            for (int j = 0; j < Ls[i]; j++) {
                while (symbol[x] != 0xFF) x = (x + 1) % L;
                symbol[x] = i;
                x = (x + step) % L;
            }
        }
        for (x = 0; x < L; x++) {
            uint8_t s = symbol[x];
            ANSEntry t = {0, s, (uint8_t)(R - log2i(next[s]))};
            t.X = (next[s] << t.n) - L;
            decodingTable[x] = t;
            next[s]++;
        }
        X = readbits(R);
        delete[] next;
        delete[] symbol;
    }
    uint32_t readbits(uint8_t n) {
        if (n == 0xFF) n = bits % 8;
        if (n == 0) return 0;
        while (bits < n) {
            bits += 8;
            partial = (partial << 8) | in.get();
        }
        uint32_t retval = ((partial >> (bits-n)) & ((1 << n)-1));
        bits -= n;
        return retval;
    }
    std::vector<uint8_t> read(int nsym) {
        std::vector<uint8_t> retval(nsym);
        if (R == 0) {
            for (int i = 0; i < nsym; i++) retval[i] = X;
            return retval;
        }
        uint32_t i = 0;
        uint8_t last = 0;
        while (i < nsym) {
            const ANSEntry& t = decodingTable[X];
            if (isColor && t.s >= 16) {
                uint32_t l = 1 << (t.s - 15);
                for (int n = 0; n < l; n++) retval[i+n] = last;
                i += l;
            } else retval[i++] = last = t.s;
            X = t.X + readbits(t.n);
        }
        return retval;
    }
};

void readFrame(std::istream& in, std::vector<uint8_t>& chars, std::vector<uint8_t>& colors, std::vector<Vec3b>& palette, const Vid32Header& h) {
    switch (h.flags & 3) {
        case VID32_FLAG_VIDEO_COMPRESSION_DEFLATE:
        case VID32_FLAG_VIDEO_COMPRESSION_NONE: {
            uint64_t tmp = 0;
            int avail = 0;
            for (int y = 0; y < h.height; y++) {
                for (int x = 0; x < h.width; x++) {
                    if (!avail) {
                        tmp = (uint64_t)in.get() << 32 | (uint64_t)in.get() << 24 | (uint64_t)in.get() << 16 | (uint64_t)in.get() << 8 | (uint64_t)in.get();
                        avail = 8;
                    }
                    chars[y*h.width+x] = ((tmp >> (--avail * 5)) & 0x1F) | 0x80;
                }
            }
            in.read((char*)&colors[0], h.width * h.height);
            for (int k = 0; k < 16; k++) palette[k] = {(uchar)in.get(), (uchar)in.get(), (uchar)in.get()};
            break;
        } case VID32_FLAG_VIDEO_COMPRESSION_CUSTOM: {
            // MARK: Huffman tree reconstruction
            // read bit lengths
            std::vector<bitlen_t> bitlen(32);
            tree_node codetree;
            int solidchar = 0, runlen = 0;
            int tmp, tmppos;
            if (h.flags & VID32_FLAG_VIDEO_5BIT_CODES) {
                for (uint8_t j = 0; j < 32; j+=2) {
                    uint8_t n = in.get();
                    bitlen[j] = {j, (uint8_t)(n >> 4), 0};
                    bitlen[j+1] = {(uint8_t)(j+1), (uint8_t)(n & 0x0F), 0};
                }
            } else {
                std::cerr << "Error: Incompatible custom compression format\n";
                exit(2);
            }
            bitlen.erase(std::remove_if(bitlen.begin(), bitlen.end(), [](const bitlen_t& p) -> bool {return p.l == 0;}), bitlen.end());
            if (bitlen.empty()) {
                // screen is solid character
                solidchar = in.get() | 0x80;
            } else {
                // reconstruct codes from bit lengths
                std::sort(bitlen.begin(), bitlen.end(), [](const bitlen_t& a, const bitlen_t& b) -> bool {
                    if (a.l == b.l) return a.s < b.s; else return a.l < b.l;});
                bitlen[0].c = 0;
                for (int j = 1; j < bitlen.size(); j++) bitlen[j].c = (bitlen[j-1].c + 1) << (bitlen[j].l - bitlen[j-1].l);
                // create tree from codes
                for (int j = 0; j < bitlen.size(); j++) {
                    uint8_t c = bitlen[j].c;
                    tree_node * node = &codetree;
                    for (int k = bitlen[j].l - 1; k >= 1; k--) {
                        if (c & (1 << k)) {
                            if (node->right == NULL) node->right = new tree_node;
                            node = node->right;
                        } else {
                            if (node->left == NULL) node->left = new tree_node;
                            node = node->left;
                        }
                        //if type(node) == "number" then error(("Invalid tree state: position %X, frame %d, #bitlen = %d, current entry = %d"):format(pos, i, #bitlen, j)) end
                    }
                    if (c & 1) {
                        node->right = new tree_node;
                        node->right->data = bitlen[j].s;
                    } else {
                        node->left = new tree_node;
                        node->left->data = bitlen[j].s;
                    }
                }
                // read first byte
                tmp = in.get(); tmppos = 7;
            }
            for (int y = 0; y < h.height; y++) {
                for (int x = 0; x < h.width; x++) {
                    if (solidchar) {
                        chars[y*h.width+x] = solidchar;
                        continue;
                    }
                    // MARK: Huffman decoding
                    tree_node * node = &codetree;
                    while (true) {
                        uint8_t n = tmp & tmppos--;
                        if (tmppos < 0) {tmp = in.get(); tmppos = 7;}
                        //if type(node) ~= "table" then error(("Invalid tree state: position %X, frame %d"):format(pos+file.seek()-size-1, i)) end
                        tree_node * next = n ? node->right : node->left;
                        if (next->left == NULL && next->right == NULL) {
                            uint8_t c = next->data;
                            chars[y*h.width+x] = c | 0x80;
                            break;
                        } else node = next;
                    }
                }
            }
            if (tmppos == 7) in.putback(tmp);
            codetree.clear();
            // MARK: Huffman tree reconstruction
            // read bit lengths
            bitlen.clear();
            bitlen.resize(24);
            for (int j = 0; j < 12; j++) {
                uint8_t b = in.get();
                bitlen[j*2] = {(uint8_t)(j*2), (uint8_t)(b >> 4), 0};
                bitlen[j*2+1] = {(uint8_t)(j*2+1), (uint8_t)(b & 0x0F), 0};
            }
            bitlen.erase(std::remove_if(bitlen.begin(), bitlen.end(), [](const bitlen_t& p) -> bool {return p.l == 0;}), bitlen.end());
            if (bitlen.empty()) {
                // screen is solid color
                solidchar = in.get() | 0x80;
            } else {
                // reconstruct codes from bit lengths
                std::sort(bitlen.begin(), bitlen.end(), [](const bitlen_t& a, const bitlen_t& b) -> bool {
                    if (a.l == b.l) return a.s < b.s; else return a.l < b.l;});
                bitlen[0].c = 0;
                for (int j = 1; j < bitlen.size(); j++) bitlen[j].c = (bitlen[j-1].c + 1) << (bitlen[j].l - bitlen[j-1].l);
                // create tree from codes
                for (int j = 0; j < bitlen.size(); j++) {
                    uint8_t c = bitlen[j].c;
                    tree_node * node = &codetree;
                    for (int k = bitlen[j].l - 1; k >= 1; k--) {
                        if (c & (1 << k)) {
                            if (node->right == NULL) node->right = new tree_node;
                            node = node->right;
                        } else {
                            if (node->left == NULL) node->left = new tree_node;
                            node = node->left;
                        }
                        //if type(node) == "number" then error(("Invalid tree state: position %X, frame %d, #bitlen = %d, current entry = %d"):format(pos, i, #bitlen, j)) end
                    }
                    if (c & 1) {
                        node->right = new tree_node;
                        node->right->data = bitlen[j].s;
                    } else {
                        node->left = new tree_node;
                        node->left->data = bitlen[j].s;
                    }
                }
                // read first byte
                tmp = in.get(); tmppos = 7;
            }
            for (int y = 0; y < h.height; y++) {
                for (int x = 0; x < h.width; x++) {
                    if (runlen) {
                        uint8_t c = solidchar;
                        runlen = runlen - 1;
                        colors[y*h.width+x] = c << 4;
                        continue;
                    }
                    if (solidchar & 0x80) {
                        colors[y*h.width+x] = solidchar << 4;
                        continue;
                    }
                    // MARK: Huffman decoding
                    tree_node * node = &codetree;
                    while (true) {
                        uint8_t n = tmp & tmppos--;
                        if (tmppos < 0) {tmp = in.get(); tmppos = 7;}
                        //if type(node) ~= "table" then error(("Invalid tree state: position %X, frame %d"):format(pos+file.seek()-size-1, i)) end
                        tree_node * next = n ? node->right : node->left;
                        if (next->left == NULL && next->right == NULL) {
                            uint8_t c = next->data;
                            if (c > 15) {
                                runlen = (1 << (c-15))-1;
                                colors[y*h.width+x] = solidchar << 4;
                                break;
                            }
                            else solidchar = c;
                            colors[y*h.width+x] = c << 4;
                            break;
                        } else node = next;
                    }
                }
            }
            for (int y = 0; y < h.height; y++) {
                for (int x = 0; x < h.width; x++) {
                    if (runlen) {
                        uint8_t c = solidchar;
                        runlen = runlen - 1;
                        colors[y*h.width+x] |= c;
                        continue;
                    }
                    if (solidchar & 0x80) {
                        colors[y*h.width+x] |= solidchar;
                        continue;
                    }
                    // MARK: Huffman decoding
                    tree_node * node = &codetree;
                    while (true) {
                        uint8_t n = tmp & tmppos--;
                        if (tmppos < 0) {tmp = in.get(); tmppos = 7;}
                        //if type(node) ~= "table" then error(("Invalid tree state: position %X, frame %d"):format(pos+file.seek()-size-1, i)) end
                        tree_node * next = n ? node->right : node->left;
                        if (next->left == NULL && next->right == NULL) {
                            uint8_t c = next->data;
                            if (c > 15) {
                                runlen = (1 << (c-15))-1;
                                colors[y*h.width+x] |= solidchar;
                                break;
                            }
                            else solidchar = c;
                            colors[y*h.width+x] |= c;
                            break;
                        } else node = next;
                    }
                }
            }
            for (int k = 0; k < 16; k++) palette[k] = {(uchar)in.get(), (uchar)in.get(), (uchar)in.get()};
            break;
        } case VID32_FLAG_VIDEO_COMPRESSION_ANS: {
            ANSDecoder<false> chardec(in);
            chars = chardec.read(h.width * h.height);
            ANSDecoder<true> colordec(in);
            std::vector<uint8_t> bg = colordec.read(h.width * h.height);
            std::vector<uint8_t> fg = colordec.read(h.width * h.height);
            for (int i = 0; i < h.width * h.height; i++) colors[i] = (fg[i] << 4) | bg[i];
            for (int k = 0; k < 16; k++) palette[k] = {(uchar)in.get(), (uchar)in.get(), (uchar)in.get()};
            break;
        }
    }
}

static void sighandler(int signal) {
    std::unique_lock<std::mutex> lock(exitLock);
    exitNotify.notify_all();
}

int main(int argc, const char * argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <file.32v> <port>\n";
        return 1;
    }
    std::ifstream in(argv[1]);
    if (!in.is_open()) {
        std::cerr << "Error: Could not open input file " << argv[0] << "\n";
        return 2;
    }
    Vid32Header h;
    in.read((char*)&h, 12);
    if (strncmp(h.magic, "32VD", 4) != 0) {
        std::cerr << "Error: Not a 32vid file\n";
        return 2;
    }
    for (int i = 0; i < h.nstreams; i++) {
        Vid32Chunk c;
        in.read((char*)&c, 9);
        switch (c.type) {
            case 0: {
                for (int j = 0; j < c.nframes; j++) {
                    std::vector<uint8_t> chars(h.width * h.height), colors(h.width * h.height);
                    std::vector<Vec3b> palette(16);
                    readFrame(in, chars, colors, palette, h);
                    frameStorage.push_back(makeTable(&chars[0], &colors[0], palette, h.width, h.height, true, false, true));
                }
            } case 1: {
                if ((h.flags & 0x0C) == VID32_FLAG_AUDIO_COMPRESSION_NONE) {
                    audioStorageSize = c.size;
                    audioStorage = new uint8_t[audioStorageSize];
                    in.read((char*)audioStorage, audioStorageSize);
                } else if ((h.flags & 0x0C) == VID32_FLAG_AUDIO_COMPRESSION_DFPWM) {

                } else {
                    std::cerr << "Unsupported audio compression format\n";
                    return 2;
                }
            } case 8: {
                Vid32SubtitleEvent sub;
                in.read((char*)&sub, sizeof(sub));
                subtitles.push_back(sub);
            } case 12: {

            }
            case 3: case 4: case 5: case 6: case 7: case 9: case 10: case 11:
                in.seekg(c.size, std::ios::cur);
                break;
            default:
                std::cerr << "Error: Unknown chunk type (" << c.type << ")\n";
                return 2;
        }
    }
    int port = atoi(argv[2]);
    double fps = h.fps;
    HTTPServer * srv = new HTTPServer(new WebSocketServer::Factory(&fps), port);
    srv->start();
    signal(SIGINT, sighandler);
    std::cout << "Serving on port " << port << "\n";
    std::unique_lock<std::mutex> lock(exitLock);
    exitNotify.wait(lock);
    srv->stop();
    delete srv;
    return 0;
}