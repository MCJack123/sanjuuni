/*
 * 32vid-player.cpp
 * Plays 32vid formatted media on the desktop using SDL3.
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

#include <iostream>
#include <fstream>
#include <memory>
#include <cassert>
#include <SDL3/SDL.h>
#include "sanjuuni.hpp"

struct Vid32Frame {
    uint32_t size;
    uint8_t type;
    uint8_t data[];
};

class Vid32Demuxer {
    std::istream& file;
    unsigned nframe = 0;
    unsigned totalFrames;
    Vid32Header h;
public:
    Vid32Demuxer(std::istream& f): file(f) {
        if (!file.read((char*)&h, 12).good()) {
            throw std::runtime_error("Failed to read file");
        }
        if (*(uint32_t*)h.magic != SDL_FOURCC('3', '2', 'V', 'D')) {
            throw std::invalid_argument("Not a 32Vid file");
        }
        if (h.nstreams != 1) {
            throw std::invalid_argument("Separate stream files not supported by this tool");
        }
        if ((h.flags & 3) != 1) {
            throw std::invalid_argument("This tool only supports ANS-compressed files");
        }
        Vid32Chunk c;
        if (!file.read((char*)&c, 9).good()) {
            throw std::runtime_error("Failed to read file");
        }
        if (c.type != 0x0C) {
            throw std::invalid_argument("Stream type not supported by this tool");
        }
        totalFrames = c.nframes;
    }

    unsigned GetWidth() const {
        return h.width;
    }

    unsigned GetHeight() const {
        return h.height;
    }

    unsigned GetFPS() const {
        return h.fps;
    }

    bool GetDFPWM() const {
        return h.flags & VID32_FLAG_AUDIO_COMPRESSION_DFPWM;
    }

    unsigned GetNextFrameIndex() const {
        return nframe;
    }

    Vid32Frame* NextFrame() {
        if (nframe++ > totalFrames) {
            return nullptr;
        }
        uint32_t size;
        file.read((char*)&size, 4);
        uint8_t type = file.get();
        if (file.eof()) return nullptr;
        if (!file.good()) {
            throw std::runtime_error("Failed to read file");
        }
        Vid32Frame* frame = (Vid32Frame*)malloc(size + 5);
        frame->size = size;
        frame->type = type;
        if (!file.read((char*)frame->data, size).good()) {
            throw std::runtime_error("Failed to read file");
        }
        return frame;
    }
};

struct ANSDecEntry {
    uint32_t X;
    uint32_t n;
    uint8_t s;
};

static uint32_t log2i(unsigned int n) {
    if (n == 0) return 0;
    uint32_t i = 0;
    while (n) {i++; n >>= 1;}
    return i - 1;
}

class Vid32Decoder {
    ANSDecEntry* decodingTable;
    uint8_t* buf;
    uint32_t X;
    uint32_t partial = 0;
    uint8_t bits = 0;
    const uint8_t R;
    const bool isColor;

    uint32_t readbits(int n) {
        if (!n) return 0;
        while (bits < n) {
            bits += 8;
            partial = (partial << 8) | *buf++;
        }
        uint32_t retval = (partial >> (bits - n) & ((1 << n) - 1));
        bits -= n;
        return retval;
    }

public:
    Vid32Decoder(uint8_t* data, bool c): isColor(c), R(data[0]) {
        uint32_t L = 1 << R;
        uint32_t Lm = L - 1;
        uint32_t Ls[32];
        const int nLs = c ? 24 : 32;
        for (int i = 0; i < (nLs >> 1); i++) {
            Ls[i<<1] = data[i+1] >> 4;
            Ls[(i<<1)+1] = data[i+1] & 0x0F;
        }
        buf = data + (c ? 13 : 17);
        if (R == 0) {
            decodingTable = new ANSDecEntry[1];
            decodingTable[0] = {0, 1, *buf++};
            X = 0xFFFFFFFFUL;
            return;
        }
        for (int i = 0; i < nLs; i++) if (Ls[i]) Ls[i] = 1 << (Ls[i] - 1);
        uint32_t x = 0;
        int step = (L >> 1) + (L >> 3) + 3;
        uint32_t next[32];
        uint8_t* symbol = new uint8_t[L];
        memset(symbol, 0xFF, L);
        decodingTable = new ANSDecEntry[L];
        for (int i = 0; i < nLs; i++) {
            next[i] = Ls[i];
            for (int j = 0; j < Ls[i]; j++) {
                while (symbol[x] != 0xFF) x = (x + 1) & Lm;
                symbol[x] = i;
                x = (x + step) & Lm;
            }
        }
        for (x = 0; x < L; x++) {
            uint8_t s = symbol[x];
            ANSDecEntry t = {0, R - log2i(next[s]), s};
            t.X = (next[s]++ << t.n) - L;
            assert(t.X < L);
            decodingTable[x] = t;
        }
        X = readbits(R);
        delete[] symbol;
    }

    ~Vid32Decoder() {
        delete[] decodingTable;
    }

    uint8_t* read(int nsym) {
        uint8_t* retval = new uint8_t[nsym];
        if (X == 0xFFFFFFFFUL) {
            for (int i = 0; i < nsym; i++) retval[i] = decodingTable[0].s;
            return retval;
        }
        int i = 0, last = 0;
        while (i < nsym) {
            const ANSDecEntry& t = decodingTable[X];
            if (isColor && t.s >= 16) {
                uint32_t l = 1 << (t.s - 15);
                for (int n = 0; n < l && i+n < nsym; n++) retval[i+n] = last;
                i += l;
            } else {
                retval[i++] = last = t.s;
            }
            X = t.X + readbits(t.n);
        }
        return retval;
    }

    uint8_t* endPointer() const {
        return buf;
    }
};

typedef struct {
    int fq, q, s, lt;
} DFPWMState;

// DFPWM codec from https://github.com/ChenThread/dfpwm/blob/master/1a/
// Licensed in the public domain

static void au_decompress(DFPWMState *state, int fs, int len,
                          uint8_t *outbuf, const uint8_t *inbuf)
{
    unsigned d;
    for (int i = 0; i < len; i++) {
        // get bits
        d = *(inbuf++);
        for (int j = 0; j < 8; j++) {
            int nq, lq, st, ns, ov;
            // set target
            int t = ((d&1) ? 127 : -128);
            d >>= 1;

            // adjust charge
            nq = state->q + ((state->s * (t-state->q) + 512)>>10);
            if(nq == state->q && nq != t)
                nq += (t == 127 ? 1 : -1);
            lq = state->q;
            state->q = nq;

            // adjust strength
            st = (t != state->lt ? 0 : 1023);
            ns = state->s;
            if(ns != st)
                ns += (st != 0 ? 1 : -1);
            if(ns < 8) ns = 8;
            state->s = ns;

            // FILTER: perform antijerk
            ov = (t != state->lt ? (nq+lq+1)>>1 : nq);

            // FILTER: perform LPF
            state->fq += ((fs*(ov-state->fq) + 0x80)>>8);
            ov = state->fq;

            // output sample
            *(outbuf++) = ov + 128;

            state->lt = t;
        }
    }
}

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <file.32v>\n";
        return 1;
    }
    std::ifstream in(argv[1]);
    if (!in.is_open()) {
        std::cerr << "Could not open file\n";
        return 2;
    }
    Vid32Demuxer demux(in);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS);
    SDL_Window* win = SDL_CreateWindow("32vid", demux.GetWidth() * 12, demux.GetHeight() * 18, 0);
    SDL_Surface* surf = SDL_GetWindowSurface(win);
    const SDL_PixelFormatDetails* pixdetail = SDL_GetPixelFormatDetails(surf->format);
    SDL_AudioSpec spec = {SDL_AUDIO_U8, 1, 48000};
    SDL_AudioStream* audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    SDL_ResumeAudioStreamDevice(audio);
    DFPWMState dfpwm = {0, 0, 0, -128};
    int vidframe = 0;
    SDL_Time videoStart;
    SDL_GetCurrentTime(&videoStart);
    while (true) {
        Vid32Frame* frame = demux.NextFrame();
        if (frame == nullptr) break;
        if (frame->type == 0) {
            vidframe++;
            Vid32Decoder sdec(frame->data, false);
            uint8_t* screen = sdec.read(demux.GetWidth() * demux.GetHeight());
            Vid32Decoder cdec(sdec.endPointer(), true);
            uint8_t* bgcolors = cdec.read(demux.GetWidth() * demux.GetHeight());
            uint8_t* fgcolors = cdec.read(demux.GetWidth() * demux.GetHeight());
            uint32_t palette[16];
            for (int i = 0; i < 16; i++) {
                palette[i] = SDL_MapRGB(pixdetail, NULL, cdec.endPointer()[i * 3], cdec.endPointer()[i * 3 + 1], cdec.endPointer()[i * 3 + 2]);
            }
            for (int y = 0; y < demux.GetHeight(); y++) {
                for (int x = 0; x < demux.GetWidth(); x++) {
                    uint8_t ch = screen[y*demux.GetWidth()+x];
                    uint32_t fg = palette[fgcolors[y*demux.GetWidth()+x]], bg = palette[bgcolors[y*demux.GetWidth()+x]];
                    SDL_Rect r = {x * 12 + 0 * 6, y * 18 + 0 * 6, 6, 6};
                    SDL_FillSurfaceRect(surf, &r, ch & 0x01 ? fg : bg);
                    r = {x * 12 + 1 * 6, y * 18 + 0 * 6, 6, 6};
                    SDL_FillSurfaceRect(surf, &r, ch & 0x02 ? fg : bg);
                    r = {x * 12 + 0 * 6, y * 18 + 1 * 6, 6, 6};
                    SDL_FillSurfaceRect(surf, &r, ch & 0x04 ? fg : bg);
                    r = {x * 12 + 1 * 6, y * 18 + 1 * 6, 6, 6};
                    SDL_FillSurfaceRect(surf, &r, ch & 0x08 ? fg : bg);
                    r = {x * 12 + 0 * 6, y * 18 + 2 * 6, 6, 6};
                    SDL_FillSurfaceRect(surf, &r, ch & 0x10 ? fg : bg);
                    r = {x * 12 + 1 * 6, y * 18 + 2 * 6, 6, 6};
                    SDL_FillSurfaceRect(surf, &r, bg);
                }
            }
            SDL_UpdateWindowSurface(win);
            delete[] screen;
            delete[] fgcolors;
            delete[] bgcolors;
            SDL_Event ev;
            SDL_Time now;
            while (SDL_GetCurrentTime(&now), SDL_WaitEventTimeout(&ev, ((videoStart + (unsigned long long)vidframe * 1000000000ULL / demux.GetFPS()) - now) / 1000000)) {
                if (ev.type == SDL_EVENT_QUIT) {
                    free(frame);
                    SDL_DestroyAudioStream(audio);
                    SDL_DestroyWindow(win);
                    SDL_Quit();
                    return 0;
                }
            }
        } else if (frame->type == 1) {
            if (demux.GetDFPWM()) {
                uint8_t* dec = new uint8_t[frame->size * 8];
                au_decompress(&dfpwm, 140, frame->size, dec, frame->data);
                SDL_PutAudioStreamData(audio, dec, frame->size * 8);
                delete[] dec;
            } else {
                SDL_PutAudioStreamData(audio, frame->data, frame->size);
            }
        }
        free(frame);
    }
    SDL_DestroyAudioStream(audio);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
