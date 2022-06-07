/*
 * sanjuuni: Converts images and videos into a format that can be displayed in ComputerCraft.
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

#define CL_HPP_TARGET_OPENCL_VERSION 200
#define CL_HPP_ENABLE_EXCEPTIONS
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <zlib.h>
}
//#include <CL/opencl.hpp>
#include <Poco/Base64Encoder.h>
#include <Poco/Checksum.h>
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
#include <vector>
#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <mutex>
#include <array>
#include <queue>
#include <functional>
#include <atomic>
#include <condition_variable>
#include <unordered_map>
#include <csignal>
#ifdef USE_SDL
#include <SDL2/SDL.h>
#endif

// OpenCL polyfills
#define __global
#define __local
#define __constant constexpr
#define __private
#define __kernel

typedef uint8_t uchar;
typedef uint16_t ushort;
typedef struct {
    uchar x;
    uchar y;
} uchar2;
typedef struct {
    uchar x;
    uchar y;
    uchar z;
} uchar3;
typedef struct {
    float x;
    float y;
    float z;
} float3;
static float distance(float3 p0, float3 p1) {return sqrt((p1.x - p0.x)*(p1.x - p0.x) + (p1.y - p0.y)*(p1.y - p0.y) + (p1.z - p0.z)*(p1.z - p0.z));}
float3 operator-(const float3& a, const float3& b) {return {a.x - b.x, a.y - b.y, a.z - b.z};}
float3& operator+=(float3& a, const float3& b) {a.x += b.x; a.y += b.y; a.z += b.z; return a;}
float3 operator*(const float3& v, float s) {return {v.x * s, v.y * s, v.z * s};}
int operator==(const float3& a, const float3& b) {return a.x == b.x && a.y == b.y && a.z == b.z;}

using namespace std::chrono;
//using namespace cl;
using namespace Poco::Util;
using namespace Poco::Net;

struct Vec3d : public std::array<double, 3> {
    Vec3d() = default;
    template<typename T> Vec3d(const std::array<T, 3>& a) {(*this)[0] = a[0]; (*this)[1] = a[1]; (*this)[2] = a[2];}
    template<typename T> Vec3d(std::initializer_list<T> il) {(*this)[0] = *(il.begin()); (*this)[1] = *(il.begin()+1); (*this)[2] = *(il.begin()+2);}
    Vec3d operator+(const Vec3d& b) {return {(*this)[0] + b[0], (*this)[1] + b[1], (*this)[2] + b[2]};}
    Vec3d operator-(const Vec3d& b) {return {(*this)[0] - b[0], (*this)[1] - b[1], (*this)[2] - b[2]};}
    Vec3d operator*(double b) {return {(double)(*this)[0] * b, (double)(*this)[1] * b, (double)(*this)[2] * b};}
    Vec3d operator/(double b) {return {(*this)[0] / b, (*this)[1] / b, (*this)[2] / b};}
    Vec3d& operator+=(const Vec3d& a) {(*this)[0] += a[0]; (*this)[1] += a[1]; (*this)[2] += a[2]; return *this;}
};
struct Vec3b : public std::array<uint8_t, 3> {
    Vec3b() = default;
    template<typename T> Vec3b(const std::array<T, 3>& a) {(*this)[0] = a[0]; (*this)[1] = a[1]; (*this)[2] = a[2];}
    template<typename T> Vec3b(std::initializer_list<T> il) {(*this)[0] = *(il.begin()); (*this)[1] = *(il.begin()+1); (*this)[2] = *(il.begin()+2);}
};
template<typename T>
class vector2d {
public:
    unsigned width;
    unsigned height;
    std::vector<T> vec;
    class row {
        std::vector<T> * vec;
        unsigned ypos;
        unsigned size;
    public:
        row(std::vector<T> *v, unsigned y, unsigned s): vec(v), ypos(y), size(s) {}
        T& operator[](unsigned idx) { 
            if (idx >= size) throw std::out_of_range("Vector2D index out of range");
            return (*vec)[ypos+idx];
        }
        row& operator=(std::vector<T> v) {std::copy(v.begin(), v.begin() + (v.size() > size ? v.size() : size), vec->begin() + ypos); return *this;}
        row& operator=(row v) {std::copy(v.vec->begin() + v.ypos, v.vec->begin() + v.ypos + v.size, vec->begin() + ypos); return *this;}
    };
    class const_row {
        const std::vector<T> * vec;
        unsigned ypos;
        unsigned size;
    public:
        const_row(const std::vector<T> *v, unsigned y, unsigned s): vec(v), ypos(y), size(s) {}
        const T& operator[](unsigned idx) const { 
            if (idx >= size) throw std::out_of_range("Vector2D index out of range");
            return (*vec)[ypos+idx];
        }
        row& operator=(std::vector<T> v) {std::copy(v.begin(), v.begin() + (v.size() > size ? v.size() : size), vec->begin() + ypos); return *this;}
        row& operator=(row v) {std::copy(v.vec->begin() + v.ypos, v.vec->begin() + v.ypos + v.size, vec->begin() + ypos); return *this;}
    };
    vector2d(): width(0), height(0), vec() {}
    vector2d(unsigned w, unsigned h, T v = T()): width(w), height(h), vec((size_t)w*h, v) {}
    row operator[](unsigned idx) {
        if (idx >= height) throw std::out_of_range("Vector2D index out of range");
        return row(&vec, idx * width, width);
    }
    const const_row operator[](unsigned idx) const {
        if (idx >= height) throw std::out_of_range("Vector2D index out of range");
        return const_row(&vec, idx * width, width);
    }
    T& at(unsigned y, unsigned x) {
        if (y >= height || x >= width) throw std::out_of_range("Vector2D index out of range");
        return vec[y*width+x];
    }
    const T& at(unsigned y, unsigned x) const {
        if (y >= height || x >= width) throw std::out_of_range("Vector2D index out of range");
        return vec[y*width+x];
    }
};
class WorkQueue {
    std::vector<std::thread> threads;
    std::queue<std::pair<std::function<void(void*)>, void*>> work;
    std::mutex notify_lock, finish_lock;
    std::condition_variable notify, finish;
    bool exiting = false;
    std::atomic_int finish_count;
    int expected_finish_count = 0;
    void worker() {
        while (!exiting) {
            std::unique_lock<std::mutex> lock(notify_lock);
            if (work.empty()) notify.wait(lock);
            if (!work.empty()) {
                std::pair<std::function<void(void*)>, void*> fn = work.front();
                work.pop();
                lock.unlock();
                fn.first(fn.second);
                std::unique_lock<std::mutex> lock(finish_lock);
                finish_count++;
                finish.notify_all();
            }
        }
    }
public:
    WorkQueue(): WorkQueue(std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 8) {}
    WorkQueue(int nthreads) {
        finish_count = 0;
        for (int i = 0; i < nthreads; i++) threads.push_back(std::thread(std::bind(&WorkQueue::worker, this)));
    }
    ~WorkQueue() {
        exiting = true;
        notify.notify_all();
        for (int i = 0; i < threads.size(); i++) threads[i].join();
    }
    void push(const std::function<void()>& fn) {
        std::unique_lock<std::mutex> lock(notify_lock);
        work.push(std::make_pair((std::function<void(void*)>)[fn](void*){fn();}, (void*)NULL));
        expected_finish_count++;
        notify.notify_one();
    }
    void push(const std::function<void(void*)>& fn, void* arg = NULL) {
        std::unique_lock<std::mutex> lock(notify_lock);
        work.push(std::make_pair(fn, arg));
        expected_finish_count++;
        notify.notify_one();
    }
    void wait() {
        while (true) {
            std::unique_lock<std::mutex> lock(finish_lock);
            if (work.empty() && finish_count >= expected_finish_count) break;
            finish.wait(lock);
        }
        finish_count = 0;
        expected_finish_count = 0;
    }
};
typedef vector2d<uint8_t> Mat1b;
typedef vector2d<Vec3b> Mat;
#undef min
#undef max
template<typename T> T min(T a, T b) {return a < b ? a : b;}
template<typename T> T max(T a, T b) {return a > b ? a : b;}

//cl::Platform CLPlatform;
bool isOpenCLInitialized = false;
static std::vector<std::string> frameStorage;
static uint8_t * audioStorage = NULL;
static long audioStorageSize = 0, totalFrames = 0;
static std::mutex exitLock, streamedLock;
static std::condition_variable exitNotify, streamedNotify;
static WorkQueue work;
static bool streamed = false;
static const char * hexstr = "0123456789abcdef";
static const std::vector<Vec3b> defaultPalette = {
    {0xf0, 0xf0, 0xf0},
    {0xf2, 0xb2, 0x33},
    {0xe5, 0x7f, 0xd8},
    {0x99, 0xb2, 0xf2},
    {0xde, 0xde, 0x6c},
    {0x7f, 0xcc, 0x19},
    {0xf2, 0xb2, 0xcc},
    {0x4c, 0x4c, 0x4c},
    {0x99, 0x99, 0x99},
    {0x4c, 0x99, 0xb2},
    {0xb2, 0x66, 0xe5},
    {0x33, 0x66, 0xcc},
    {0x7f, 0x66, 0x4c},
    {0x57, 0xa6, 0x4e},
    {0xcc, 0x4c, 0x4c},
    {0x11, 0x11, 0x11}
};
static const std::string playLua = "'local function b(c)local d,e=http.get('http://'..a..c,nil,true)if not d then error(e)end;local f=d.readAll()d.close()return f end;local g=textutils.unserializeJSON(b('/info'))local h,i={},{}local j=peripheral.find'speaker'term.clear()local k=2;parallel.waitForAll(function()for l=0,g.length-1 do h[l]=b('/video/'..l)if k>0 then k=k-1 end end end,function()for l=0,g.length/g.fps do i[l]=b('/audio/'..l)if k>0 then k=k-1 end end end,function()while k>0 do os.pullEvent()end;local m=os.epoch'utc'for l=0,g.length-1 do while not h[l]do os.pullEvent()end;local n=h[l]h[l]=nil;local o,p=assert(load(n,'=frame','t',{}))()for q,r in ipairs(p)do term.setPaletteColor(2^(q-1),table.unpack(r))end;for s,t in ipairs(o)do term.setCursorPos(1,s)term.blit(table.unpack(t))end;while os.epoch'utc'<m+(l+1)/g.fps*1000 do sleep(1/g.fps)end end end,function()if not j or not j.playAudio then return end;while k>0 do os.pullEvent()end;local u=0;while u<g.length/g.fps do while not i[u]do os.pullEvent()end;local v=i[u]i[u]=nil;v={v:byte(1,-1)}for q=1,#v do v[q]=v[q]-128 end;u=u+1;if not j.playAudio(v)or _HOST:find('v2.6.4')then repeat local w,x=os.pullEvent('speaker_audio_empty')until x==peripheral.getName(j)end end end)for q=0,15 do term.setPaletteColor(2^q,term.nativePaletteColor(2^q))end;term.setBackgroundColor(colors.black)term.setTextColor(colors.white)term.setCursorPos(1,1)term.clear()";

// BEGIN OPENCL-COMPATIBLE CODE

/* OpenCL-compatible C to be able to copy straight to OpenCL */
/* can't use external functions besides what CLC provides */

uchar getComponent(__private uchar3 c, __private int cmp) {
    switch (cmp) {
        case 0: return c.x;
        case 1: return c.y;
        case 2: return c.z;
        default: return 0;
    }
}

/* unrolled Floyd-Steinberg dithering */
void ditherCCImage(__private float3 * img, __private float3 a, __private float3 b, __private uchar ac, __private uchar bc, __private uchar * out) {
    __private float3 err;
    /* 0, 0 */
    if (distance(img[0], a) < distance(img[0], b)) {
        err = img[0] - a;
        img[0] = a;
    } else {
        err = img[0] - b;
        img[0] = b;
    }
    img[1] += err * (7.0/16.0);
    img[2] += err * (5.0/16.0);
    img[3] += err * (1.0/16.0);
    /* 1, 0 */
    if (distance(img[1], a) < distance(img[1], b)) {
        err = img[1] - a;
        img[1] = a;
    } else {
        err = img[1] - b;
        img[1] = b;
    }
    img[2] += err * (3.0/16.0);
    img[3] += err * (5.0/16.0);
    /* 0, 1 */
    if (distance(img[2], a) < distance(img[2], b)) {
        err = img[2] - a;
        img[2] = a;
    } else {
        err = img[2] - b;
        img[2] = b;
    }
    img[3] += err * (7.0/16.0);
    img[4] += err * (5.0/16.0);
    img[5] += err * (1.0/16.0);
    /* 1, 1 */
    if (distance(img[3], a) < distance(img[3], b)) {
        err = img[3] - a;
        img[3] = a;
    } else {
        err = img[3] - b;
        img[3] = b;
    }
    img[4] += err * (3.0/16.0);
    img[5] += err * (5.0/16.0);
    /* 0, 2 */
    if (distance(img[4], a) < distance(img[4], b)) {
        err = img[4] - a;
        img[4] = a;
    } else {
        err = img[4] - b;
        img[4] = b;
    }
    img[5] += err * (7.0/16.0);
    /* 1, 2 */
    if (distance(img[5], a) < distance(img[5], b)) {
        err = img[5] - a;
        img[5] = a;
    } else {
        err = img[5] - b;
        img[5] = b;
    }
    /* generate image */
    if (img[0] == a) out[0] = ac; else out[0] = bc;
    if (img[1] == a) out[1] = ac; else out[1] = bc;
    if (img[2] == a) out[2] = ac; else out[2] = bc;
    if (img[3] == a) out[3] = ac; else out[3] = bc;
    if (img[4] == a) out[4] = ac; else out[4] = bc;
    if (img[5] == a) out[5] = ac; else out[5] = bc;
}

/* character: high byte = bg << 4 | fg, low byte = char */
__kernel void toCCPixel(__global uchar * colors, __global uchar * character, __global uchar * color, __global uchar3 * palette) {
    __private uchar used_colors[6];
    __private int n_used_colors = 0;
    __private int i, j, tmp, maxComponent;
    __private uchar ch = 128, fg, bg;
    __private bool found = false;
    __private uchar2 red, green, blue;
    __private float color_distances[3];
    __private uchar color_map[16], b[3];
    __private int sums[3];
    __private float3 palf[4];
    __private float3 dither_in[6];
    __private uchar dither_out[6];
    for (i = 0; i < 6; i++) {
        for (j = 0; j < n_used_colors; j++) {
            if (used_colors[j] == colors[i]) {
                found = true;
                break;
            }
        }
        if (!found) used_colors[n_used_colors++] = colors[i];
    }
    switch (n_used_colors) {
    case 1:
        *character = ' ';
        *color = used_colors[0] << 4;
        break;
    case 2:
        fg = used_colors[1]; bg = used_colors[0];
        for (i = 0; i < 5; i++) if (colors[i] == fg) ch = ch | (1 << i);
        if (colors[5] == fg) {ch = (~ch & 0x1F) | 128; fg = bg; bg = used_colors[1];}
        *character = ch;
        *color = fg | (bg << 4);
        break;
    case 3:
        sums[0] = palette[used_colors[0]].x + palette[used_colors[0]].y + palette[used_colors[0]].z;
        sums[1] = palette[used_colors[1]].x + palette[used_colors[1]].y + palette[used_colors[1]].z;
        sums[2] = palette[used_colors[2]].x + palette[used_colors[2]].y + palette[used_colors[2]].z;
        if (sums[0] > sums[1]) {
            tmp = sums[1];
            sums[1] = sums[0];
            sums[0] = tmp;
            tmp = used_colors[1];
            used_colors[1] = used_colors[0];
            used_colors[0] = tmp;
        }
        if (sums[0] > sums[2]) {
            tmp = sums[2];
            sums[2] = sums[0];
            sums[0] = tmp;
            tmp = used_colors[2];
            used_colors[2] = used_colors[0];
            used_colors[0] = tmp;
        }
        if (sums[1] > sums[2]) {
            tmp = sums[2];
            sums[2] = sums[1];
            sums[1] = tmp;
            tmp = used_colors[2];
            used_colors[2] = used_colors[1];
            used_colors[1] = tmp;
        }
        palf[0].x = palette[used_colors[0]].x; palf[0].y = palette[used_colors[0]].y; palf[0].z = palette[used_colors[0]].z;
        palf[1].x = palette[used_colors[1]].x; palf[1].y = palette[used_colors[1]].y; palf[1].z = palette[used_colors[1]].z;
        palf[2].x = palette[used_colors[2]].x; palf[2].y = palette[used_colors[2]].y; palf[2].z = palette[used_colors[2]].z;
        color_distances[0] = distance(palf[1], palf[0]);
        color_distances[1] = distance(palf[2], palf[1]);
        color_distances[2] = distance(palf[0], palf[2]);
        if (color_distances[0] - color_distances[1] > 10) {
            color_map[used_colors[0]] = used_colors[0];
            color_map[used_colors[1]] = used_colors[2];
            color_map[used_colors[2]] = used_colors[2];
            fg = used_colors[2]; bg = used_colors[0];
        } else if (color_distances[1] - color_distances[0] > 10) {
            color_map[used_colors[0]] = used_colors[0];
            color_map[used_colors[1]] = used_colors[0];
            color_map[used_colors[2]] = used_colors[2];
            fg = used_colors[2]; bg = used_colors[0];
        } else {
            if ((palette[used_colors[0]].x + palette[used_colors[0]].y + palette[used_colors[0]].z) < 32) {
                color_map[used_colors[0]] = used_colors[1];
                color_map[used_colors[1]] = used_colors[1];
                color_map[used_colors[2]] = used_colors[2];
                fg = used_colors[1]; bg = used_colors[2];
            } else if ((palette[used_colors[2]].x + palette[used_colors[2]].y + palette[used_colors[2]].z) >= 224) {
                color_map[used_colors[0]] = used_colors[1];
                color_map[used_colors[1]] = used_colors[2];
                color_map[used_colors[2]] = used_colors[2];
                fg = used_colors[1]; bg = used_colors[2];
            } else { /* Fallback if the algorithm fails */
                color_map[used_colors[0]] = used_colors[1];
                color_map[used_colors[1]] = used_colors[2];
                color_map[used_colors[2]] = used_colors[2];
                fg = used_colors[1]; bg = used_colors[2];
            }
        }
        for (i = 0; i < 5; i++) if (color_map[colors[i]] == fg) ch = ch | (1 << i);
        if (color_map[colors[5]] == fg) {ch = (~ch & 0x1F) | 128; fg = bg; bg = used_colors[1];}
        *character = ch;
        *color = fg | (bg << 4);
        break;
    case 4:
        /* find the colors that are used twice (there are exactly one or two) */
        for (i = 0; i < 4; i++) color_map[used_colors[i]] = 0;
        fg = 0xFF; bg = 0xFF;
        for (i = 0; i < 6; i++) {
            if (++color_map[colors[i]] == 2) {
                if (fg == 0xFF) fg = colors[i];
                else bg = colors[i];
            }
        }
        color_map[fg] = fg;
        /* if there's only one reused color, take the middle color of the other three for the background */
        if (bg == 0xFF) {
            tmp = 0;
            for (i = 0; i < 4; i++) if (used_colors[i] != fg) b[tmp++] = used_colors[i];
            sums[0] = palette[b[0]].x + palette[b[0]].y + palette[b[0]].z;
            sums[1] = palette[b[1]].x + palette[b[1]].y + palette[b[1]].z;
            sums[2] = palette[b[2]].x + palette[b[2]].y + palette[b[2]].z;
            if (sums[0] > sums[1]) {
                tmp = sums[1];
                sums[1] = sums[0];
                sums[0] = tmp;
                tmp = b[1];
                b[1] = b[0];
                b[0] = tmp;
            }
            if (sums[0] > sums[2]) {
                tmp = sums[2];
                sums[2] = sums[0];
                sums[0] = tmp;
                tmp = b[2];
                b[2] = b[0];
                b[0] = tmp;
            }
            if (sums[1] > sums[2]) {
                tmp = sums[2];
                sums[2] = sums[1];
                sums[1] = tmp;
                tmp = b[2];
                b[2] = b[1];
                b[1] = tmp;
            }
            bg = b[1];
        }
        color_map[bg] = bg;
        /* map other colors to nearest match */
        b[0] = b[1] = 0xFF;
        for (i = 0; i < 4; i++) {
            if (used_colors[i] != fg && used_colors[i] != bg) {
                if (!found) {
                    found = true;
                    b[0] = used_colors[i];
                    palf[0].x = palette[used_colors[i]].x; palf[0].y = palette[used_colors[i]].y; palf[0].z = palette[used_colors[i]].z;
                } else {
                    b[1] = used_colors[i];
                    palf[1].x = palette[used_colors[i]].x; palf[1].y = palette[used_colors[i]].y; palf[1].z = palette[used_colors[i]].z;
                }
            }
        }
        palf[2].x = palette[fg].x; palf[2].y = palette[fg].y; palf[2].z = palette[fg].z;
        palf[3].x = palette[bg].x; palf[3].y = palette[bg].y; palf[3].z = palette[bg].z;
        if (distance(palf[0], palf[2]) < distance(palf[0], palf[3])) color_map[b[0]] = fg;
        else color_map[b[0]] = bg;
        if (distance(palf[1], palf[2]) < distance(palf[1], palf[3])) color_map[b[1]] = fg;
        else color_map[b[1]] = bg;
        for (i = 0; i < 5; i++) if (color_map[colors[i]] == fg) ch = ch | (1 << i);
        if (color_map[colors[5]] == fg) {ch = (~ch & 0x1F) | 128; tmp = fg; fg = bg; bg = tmp;}
        *character = ch;
        *color = fg | (bg << 4);
        break;
    default:
        /* Fall back on median cut */
        red.x = green.x = blue.x = 255;
        for (i = 0; i < n_used_colors; i++) {
            if (palette[used_colors[i]].x < red.x) red.x = palette[used_colors[i]].x;
            if (palette[used_colors[i]].x > red.x) red.y = palette[used_colors[i]].x;
            if (palette[used_colors[i]].y < green.x) green.x = palette[used_colors[i]].y;
            if (palette[used_colors[i]].y > green.x) green.y = palette[used_colors[i]].y;
            if (palette[used_colors[i]].z < blue.x) blue.x = palette[used_colors[i]].z;
            if (palette[used_colors[i]].z > blue.x) blue.y = palette[used_colors[i]].z;
        }
        sums[0] = red.y - red.x; sums[1] = green.y - green.x; sums[2] = blue.y - blue.x;
        if (sums[0] > sums[1] and sums[0] > sums[2]) maxComponent = 0;
        else if (sums[1] > sums[2] and sums[1] > sums[0]) maxComponent = 1;
        else maxComponent = 2;
        /* insertion sort */
        for (i = 0; i < 6; i++) dither_out[i] = colors[i];
        for (i = 0; i < 6; i++) {
            tmp = dither_out[i];
            fg = getComponent(palette[tmp], maxComponent);
            for (j = i; j > 0 && getComponent(palette[dither_out[j-1]], maxComponent) > fg; j--)
                dither_out[j] = dither_out[j-1];
            dither_out[j] = tmp;
        }
        for (i = 0; i < 6; i++) {
            if (i < 3) color_map[i] = dither_out[i];
            else b[i - 3] = dither_out[i];
        }
        dither_in[0].x = palette[colors[0]].x; dither_in[0].y = palette[colors[0]].y; dither_in[0].z = palette[colors[0]].z;
        dither_in[1].x = palette[colors[1]].x; dither_in[1].y = palette[colors[1]].y; dither_in[1].z = palette[colors[1]].z;
        dither_in[2].x = palette[colors[2]].x; dither_in[2].y = palette[colors[2]].y; dither_in[2].z = palette[colors[2]].z;
        dither_in[3].x = palette[colors[3]].x; dither_in[3].y = palette[colors[3]].y; dither_in[3].z = palette[colors[3]].z;
        dither_in[4].x = palette[colors[4]].x; dither_in[4].y = palette[colors[4]].y; dither_in[4].z = palette[colors[4]].z;
        dither_in[5].x = palette[colors[5]].x; dither_in[5].y = palette[colors[5]].y; dither_in[5].z = palette[colors[5]].z;
        palf[0].x = palette[color_map[2]].x; palf[0].y = palette[color_map[2]].y; palf[0].z = palette[color_map[2]].z;
        palf[1].x = palette[b[2]].x; palf[1].y = palette[b[2]].y; palf[1].z = palette[b[2]].z;
        ditherCCImage(dither_in, palf[0], palf[1], color_map[2], b[2], dither_out);
        fg = color_map[2]; bg = b[2];
        for (i = 0; i < 5; i++) if (dither_out[i] == fg) ch = ch | (1 << i);
        if (dither_out[5] == fg) {ch = (~ch & 0x1F) | 128; fg = bg; bg = color_map[2];}
        *character = ch;
        *color = fg | (bg << 4);
        break;
    }
}

// END OPENCL-COMPATIBLE CODE

void initCL() {
    if (isOpenCLInitialized) return;
    isOpenCLInitialized = true;
    /*try {
        // Filter for a 2.0 or newer platform and set it as the default
        std::vector<cl::Platform> platforms;
        cl::Platform::get(&platforms);
        cl::Platform plat;
        for (auto &p : platforms) {
            std::string platver = p.getInfo<CL_PLATFORM_VERSION>();
            if (platver.find("OpenCL 2.") != std::string::npos ||
                platver.find("OpenCL 3.") != std::string::npos) {
                // Note: an OpenCL 3.x platform may not support all required features!
                plat = p;
            }
        }
        if (plat() == 0) {
            std::cerr << "No OpenCL 2.0 or newer platform found. Falling back on CPU computation.\n";
            return;
        }
    
        CLPlatform = cl::Platform::setDefault(plat);
        if (CLPlatform != plat) {
            std::cerr << "Error setting default platform. Falling back on CPU computation.\n";
            return;
        }
    } catch (cl::Error& e) {
        std::cerr << "OpenCL error: " << e.what() << " (" << e.err() << "). Falling back on CPU computation.\n";
    }*/
}

static void medianCut(std::vector<Vec3b>& pal, int num, int lastComponent, std::vector<Vec3b>::iterator res) {
    if (num == 1) {
        Vec3d sum = {0, 0, 0};
        for (const Vec3b& v : pal) sum += v;
        *res = Vec3b(sum / (double)pal.size());
    } else {
        uint8_t red[2] = {255, 0}, green[2] = {255, 0}, blue[2] = {255, 0};
        for (const Vec3b& v : pal) {
            red[0] = min(v[0], red[0]); red[1] = max(v[0], red[1]);
            green[0] = min(v[1], green[0]); green[1] = max(v[1], green[1]);
            blue[0] = min(v[2], blue[0]); blue[1] = max(v[2], blue[1]);
        }
        Vec3b ranges = {(uint8_t)(red[1] - red[0]), (uint8_t)(green[1] - green[0]), (uint8_t)(blue[1] - blue[0])};
        int maxComponent;
        if (ranges[0] > ranges[1] and ranges[0] > ranges[2]) maxComponent = 0;
        else if (ranges[1] > ranges[2] and ranges[1] > ranges[0]) maxComponent = 1;
        else maxComponent = 2;
        if (maxComponent == lastComponent) {
            if (abs(ranges[maxComponent] - ranges[(maxComponent+1)%3]) < 8 && abs(ranges[maxComponent] - ranges[(maxComponent+2)%3]) < 8)
                maxComponent = ranges[(maxComponent+1)%3] > ranges[(maxComponent+2)%3] ? (maxComponent + 1) % 3 : (maxComponent + 2) % 3;
            else if (abs(ranges[maxComponent] - ranges[(maxComponent+1)%3]) < 8) maxComponent = (maxComponent + 1) % 3;
            else if (abs(ranges[maxComponent] - ranges[(maxComponent+2)%3]) < 8) maxComponent = (maxComponent + 2) % 3;
        }
        std::sort(pal.begin(), pal.end(), [maxComponent](const Vec3b& a, const Vec3b& b)->bool {return a[maxComponent] < b[maxComponent];});
        work.push([res, num, maxComponent](void* v){medianCut(*(std::vector<Vec3b>*)v, num / 2, maxComponent, res); delete (std::vector<Vec3b>*)v;}, new std::vector<Vec3b>(pal.begin(), pal.begin() + pal.size() / 2));
        work.push([res, num, maxComponent](void* v){medianCut(*(std::vector<Vec3b>*)v, num / 2, maxComponent, res + (num / 2)); delete (std::vector<Vec3b>*)v;}, new std::vector<Vec3b>(pal.begin() + pal.size() / 2, pal.end()));
    }
}

// BEGIN OCTREE QUANTIZATION CODE

/*
Octree quantization

Copyright (c) 2006 Michal Molhanec

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented;
     you must not claim that you wrote the original software.
     If you use this software in a product, an acknowledgment
     in the product documentation would be appreciated but
     is not required.

  2. Altered source versions must be plainly marked as such,
     and must not be misrepresented as being the original software.

  3. This notice may not be removed or altered from any
     source distribution.
*/

/*

octree.c -- Octree quantization implementation.
Modified by JackMacWindows for sanjuuni.

*/

#define BITS_USED 8

struct octree_node {
    uint32_t r, g, b;
    uint32_t counter;
    int leaf;
    int leaf_parent;
    struct octree_node* subnodes[8];
    int palette_entry;
    struct octree_node* prev;
    struct octree_node* next;
    struct octree_node* parent;
};

struct octree_tree {
    struct octree_node* root;
    uint32_t number_of_leaves;
    struct octree_node* leaves_parents;
};

static struct octree_node* octree_create_node(struct octree_node* parent) {
    struct octree_node* n = (struct octree_node*) calloc(1, sizeof(struct octree_node));
    if (n) {
        n->parent = parent;
    }
    return n;
}

/* 0 on error */
static int octree_insert_pixel(struct octree_tree* tree, int r, int g, int b) {
    int mask;
    int r_bit, g_bit, b_bit;
    int index;
    int i;
    struct octree_node* node = tree->root;
    for (i = BITS_USED; i >= 0; i--) {
        mask =  1 << i;
        r_bit = (r & mask) >> i;
        g_bit = (g & mask) >> i;
        b_bit = (b & mask) >> i;
        index = (r_bit << 2) + (g_bit << 1) + b_bit;
        if (!node->subnodes[index]) {
            node->subnodes[index] = octree_create_node(node);
            if (!node->subnodes[index]) {
                return 0;
            }
        }
        node = node->subnodes[index];
    }
    if (node->counter == 0) {
        tree->number_of_leaves++;
        node->leaf = 1;
        if (!node->parent->leaf_parent) {
            node->parent->leaf_parent = 1;
            if (tree->leaves_parents) {
                tree->leaves_parents->prev = node->parent;
            }
            node->parent->next = tree->leaves_parents;
            tree->leaves_parents = node->parent;
        }
    }
    node->counter++;
    node->r += r;
    node->g += g;
    node->b += b;
    return 1;
}

static uint32_t octree_calc_counters(struct octree_node* node) {
    int i;
    if (node->leaf) {
        return node->counter;
    }
    for (i = 0; i < 8; i++) {
        if (node->subnodes[i]) {
            node->counter += octree_calc_counters(node->subnodes[i]);
        }
    }
    return node->counter;
}

static struct octree_node* octree_find_smallest(struct octree_tree* tree, uint32_t* last_min) {
    struct octree_node* min = tree->leaves_parents;
    struct octree_node* n = tree->leaves_parents->next;

    while (n != 0) {
        if (min->counter == *last_min) {
            return min;
        }
        if (n->counter < min->counter) {
            min = n;
        }
        n = n->next;
    }
    *last_min = min->counter;
    return min;
}

static void octree_reduce(struct octree_tree* tree) {
    struct octree_node* n;
    uint32_t min = 1;
    int i;
    if (tree->number_of_leaves <= 16) {
        return;
    }
    octree_calc_counters(tree->root);
    while (tree->number_of_leaves > 16) {
        n = octree_find_smallest(tree, &min);
        for (i = 0; i < 8; i++) {
            if (n->subnodes[i]) {
                n->r += n->subnodes[i]->r;
                n->g += n->subnodes[i]->g;
                n->b += n->subnodes[i]->b;
                free(n->subnodes[i]);
                n->subnodes[i] = 0;
                tree->number_of_leaves--;
            }
        }
        tree->number_of_leaves++;
        n->leaf = 1;
        if (!n->parent->leaf_parent) {
            n->parent->leaf_parent = 1;
            n->parent->next = n->next;
            n->parent->prev = n->prev;
            if (n->prev) {
                n->prev->next = n->parent;
            } else {
                tree->leaves_parents = n->parent;
            }
            if (n->next) {
                n->next->prev = n->parent;
            }
        } else {
            if (n->prev) {
                n->prev->next = n->next;
            } else {
                tree->leaves_parents = n->next;
            }
            if (n->next) {
                n->next->prev = n->prev;
            }
        }
    }
}

static void octree_fill_palette(std::vector<Vec3b>& pal, int* index, struct octree_tree* tree) {
    int i;
    struct octree_node* n = tree->leaves_parents;
    while (n) {
        for (i = 0; i < 8; i++) {
            if (n->subnodes[i] && n->subnodes[i]->leaf) {
                pal[*index][0] = n->subnodes[i]->r / n->subnodes[i]->counter;
                pal[*index][1] = n->subnodes[i]->g / n->subnodes[i]->counter;
                pal[*index][2] = n->subnodes[i]->b / n->subnodes[i]->counter;
                n->subnodes[i]->palette_entry = (*index)++;
            }
        }
        n = n->next;
    }
}

static int octree_get_index(struct octree_node* node, int r, int g, int b, int i) {
    int mask, index;
    int r_bit, g_bit, b_bit;
restart:
    if (node->leaf) {
        return node->palette_entry;
    }
    mask =  1 << i;
    r_bit = (r & mask) >> i;
    g_bit = (g & mask) >> i;
    b_bit = (b & mask) >> i;
    index = (r_bit << 2) + (g_bit << 1) + b_bit;
    i--;
    node = node->subnodes[index];
    goto restart;
}

static void octree_free_node(struct octree_node* node) {
    int i;
    for (i = 0; i < 8; i++) {
        if (node->subnodes[i]) {
            octree_free_node(node->subnodes[i]);
        }
    }
    free(node);
}

std::vector<Vec3b> octree_quantize(const Mat& bmp, octree_tree * tree) {
    int x, y;
    int i;
    int bpp;
    int r, g, b;
    std::vector<Vec3b> pal(16);

    tree->number_of_leaves = 0;
    tree->root = octree_create_node(0);
    tree->leaves_parents = 0;
    if (!tree->root) {
        return {};
    }

    for (y = 0; y < bmp.height; y++) {
        for (x = 0; x < bmp.width; x++) {
            if (!octree_insert_pixel(tree, bmp[y][x][0], bmp[y][x][1], bmp[y][x][2])) {
                octree_free_node(tree->root);
                return {};
            }
        }
    }
    
    octree_reduce(tree);
    if (tree->number_of_leaves == 16) {
        i = 0;
    } else {
        i = 1;
        /* If there is space, left color with index 0 black */
        pal[0][0] = pal[0][1] = pal[0][2] = 0;
    }
    octree_fill_palette(pal, &i, tree);
    while (i < 16) {
        pal[i][0] = pal[i][0] = pal[i][0] = 0;
        i++;
    }

    return pal;
}

// END OCTREE QUANTIZATION CODE

std::vector<Vec3b> reducePalette(const Mat& image, int numColors, octree_tree * octree) {
    if (octree) {
        return octree_quantize(image, octree); // TODO: use the tree search
    } else {
        int e = 0;
        if (frexp(numColors, &e) > 0.5) throw std::invalid_argument("color count must be a power of 2");
        std::vector<Vec3b> pal;
        for (int y = 0; y < image.height; y++)
            for (int x = 0; x < image.width; x++)
                pal.push_back(Vec3b(image.at(y, x)));
        std::vector<Vec3b> uniq(pal);
        uniq.erase(std::unique(uniq.begin(), uniq.end()), uniq.end());
        if (numColors >= uniq.size()) return uniq;
        std::vector<Vec3b> newpal(numColors);
        medianCut(pal, numColors, -1, newpal.begin());
        work.wait();
        // Move the darkest color to 15, and the lightest to 0
        // This fixes some background issues & makes subtitles a bit simpler
        std::vector<Vec3b>::iterator darkest = newpal.begin(), lightest = newpal.begin();
        for (auto it = newpal.begin(); it != newpal.end(); it++) {
            if ((int)(*it)[0] + (int)(*it)[1] + (int)(*it)[2] < (int)(*darkest)[0] + (int)(*darkest)[1] + (int)(*darkest)[2]) darkest = it;
            if ((int)(*it)[0] + (int)(*it)[1] + (int)(*it)[2] > (int)(*lightest)[0] + (int)(*lightest)[1] + (int)(*lightest)[2]) lightest = it;
        }
        Vec3b d = *darkest, l = *lightest;
        if (darkest == lightest) newpal.erase(darkest);
        else if (darkest > lightest) {
            newpal.erase(darkest);
            newpal.erase(lightest);
        } else {
            newpal.erase(lightest);
            newpal.erase(darkest);
        }
        newpal.insert(newpal.begin(), l);
        newpal.push_back(d);
        return newpal;
    }
}

struct kmeans_state {
    int i, numColors;
    std::vector<std::pair<Vec3d, std::vector<const Vec3d*>>> *colors, *newColors;
    std::vector<std::mutex*>* locks;
    bool * changed;
};

static void kMeans_bucket(void* s) {
    kmeans_state * state = (kmeans_state*)s;
    std::vector<std::vector<const Vec3d*>> vec(state->numColors);
    for (const Vec3d* color : (*state->colors)[state->i].second) {
        int nearest = 0;
        double dist = sqrt(((double)(*state->newColors)[0].first[0] - (*color)[0])*((double)(*state->newColors)[0].first[0] - (*color)[0]) +
                           ((double)(*state->newColors)[0].first[1] - (*color)[1])*((double)(*state->newColors)[0].first[1] - (*color)[1]) +
                           ((double)(*state->newColors)[0].first[2] - (*color)[2])*((double)(*state->newColors)[0].first[2] - (*color)[2]));
        for (int j = 1; j < state->numColors; j++) {
            double d = sqrt(((double)(*state->newColors)[j].first[0] - (*color)[0])*((double)(*state->newColors)[j].first[0] - (*color)[0]) +
                            ((double)(*state->newColors)[j].first[1] - (*color)[1])*((double)(*state->newColors)[j].first[1] - (*color)[1]) +
                            ((double)(*state->newColors)[j].first[2] - (*color)[2])*((double)(*state->newColors)[j].first[2] - (*color)[2]));
            if (d < dist) {
                nearest = j;
                dist = d;
            }
        }
        vec[nearest].push_back(color);
    }
    for (int j = 0; j < state->numColors; j++) {
        (*state->locks)[j]->lock();
        (*state->newColors)[j].second.insert((*state->newColors)[j].second.end(), vec[j].begin(), vec[j].end());
        (*state->locks)[j]->unlock();
    }
}

static void kMeans_recenter(void* s) {
    kmeans_state * state = (kmeans_state*)s;
    Vec3d sum {0, 0, 0};
    int size = 0;
    for (const Vec3d* c : (*state->newColors)[state->i].second) {
        sum += *c;
        size++;
    }
    if (size == 0) return;
    sum = sum / size;
    if (Vec3b(sum) != Vec3b((*state->colors)[state->i].first)) *state->changed = true;
    (*state->newColors)[state->i].first = sum;
    (*state->newColors)[state->i].second.clear();
}

std::vector<Vec3b> kMeans(const Mat& image, int numColors) {
    Vec3d * originalColors = new Vec3d[image.width * image.height];
    std::vector<std::pair<Vec3d, std::vector<const Vec3d*>>> colorsA, colorsB;
    std::vector<std::pair<Vec3d, std::vector<const Vec3d*>>> *colors = &colorsA, *newColors = &colorsB;
    std::vector<kmeans_state> states(numColors);
    std::vector<std::mutex*> locks;
    bool changed = true;
    // get initial centroids
    // TODO: optimize
    std::vector<Vec3b> med = reducePalette(image, numColors, NULL);
    for (int i = 0; i < numColors; i++) colors->push_back(std::make_pair(med[i], std::vector<const Vec3d*>()));
    // first loop
    // place all colors in nearest bucket
    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            Vec3d c = Vec3d(image[y][x]);
            int nearest = 0;
            Vec3d v = (*colors)[0].first - c;
            double dist = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
            for (int i = 1; i < numColors; i++) {
                v = (*colors)[i].first - c;
                double d = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
                if (d < dist) {
                    nearest = i;
                    dist = d;
                }
            }
            originalColors[y*image.width+x] = c;
            (*colors)[nearest].second.push_back(&originalColors[y*image.width+x]);
        }
    }
    // generate new centroids
    for (int i = 0; i < numColors; i++) {
        states[i].i = i;
        states[i].numColors = numColors;
        states[i].locks = &locks;
        states[i].changed = &changed;
        Vec3d sum;
        int size = 0;
        for (const Vec3d* c : (*colors)[i].second) {
            sum += *c;
            size++;
        }
        sum = size == 0 ? sum : sum / size;
        newColors->push_back(std::make_pair(sum, std::vector<const Vec3d*>()));
    }
    // loop
    for (int i = 0; i < numColors; i++) locks.push_back(new std::mutex());
    for (int loop = 0; loop < 100 && changed; loop++) {
        changed = false;
        // place all colors in nearest new bucket
        for (int i = 0; i < numColors; i++) {
            states[i].colors = colors;
            states[i].newColors = newColors;
            work.push(kMeans_bucket, &states[i]);
        }
        work.wait();
        auto tmp = colors;
        colors = newColors;
        newColors = tmp;
        // generate new centroids
        for (int i = 0; i < numColors; i++) {
            states[i].colors = colors;
            states[i].newColors = newColors;
            work.push(kMeans_recenter, &states[i]);
        }
        work.wait();
    }
    // make final palette
    std::vector<Vec3b> retval;
    for (int i = 0; i < numColors; i++) {
        retval.push_back(Vec3b((*newColors)[i].first));
        delete locks[i];
    }
    delete[] originalColors;
    return retval;
}

static Vec3b nearestColor(const std::vector<Vec3b>& palette, const Vec3d& color, int* _n = NULL) {
    double n, dist = 1e100;
    for (int i = 0; i < palette.size(); i++) {
        double d = sqrt(((double)palette[i][0] - color[0])*((double)palette[i][0] - color[0]) +
                        ((double)palette[i][1] - color[1])*((double)palette[i][1] - color[1]) +
                        ((double)palette[i][2] - color[2])*((double)palette[i][2] - color[2]));
        if (d < dist) {n = i; dist = d;}
    }
    if (_n) *_n = n;
    return palette[n];
}

struct threshold_state {
    int i;
    const Mat * image;
    Mat * output;
    const std::vector<Vec3b> * palette;
};

static void thresholdImage_worker(void* s) {
    threshold_state * state = (threshold_state*)s;
    for (int x = 0; x < state->image->width; x++) state->output->at(state->i, x) = nearestColor(*state->palette, state->image->at(state->i, x));
    delete state;
}

Mat thresholdImage(const Mat& image, const std::vector<Vec3b>& palette) {
    Mat output(image.width, image.height);
    initCL();
    if (/*CLPlatform() &&*/ false) {
        // Use OpenCL for computation
    } else {
        // Use normal CPU for computation
        for (int i = 0; i < image.height; i++)
            work.push(thresholdImage_worker, new threshold_state {i, &image, &output, &palette});
        work.wait();
    }
    return output;
}

Mat ditherImage(const Mat& image, const std::vector<Vec3b>& palette) {
    vector2d<Vec3d> errmap(image.width, image.height);
    Mat retval(image.width, image.height);
    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            Vec3d c = Vec3d(image.at(y, x)) + errmap.at(y, x);
            Vec3b newpixel = nearestColor(palette, c);
            retval.at(y, x) = newpixel;
            Vec3d err = c - Vec3d(newpixel);
            if (x < image.width - 1) errmap.at(y, x + 1) += err * (7.0/16.0);
            if (y < image.height - 1) {
                if (x > 1) errmap.at(y + 1, x - 1) += err * (3.0/16.0);
                errmap.at(y + 1, x) += err * (5.0/16.0);
                if (x < image.width - 1) errmap.at(y + 1, x + 1) += err * (1.0/16.0);
            }
        }
    }
    return retval;
}

Mat1b rgbToPaletteImage(const Mat& image, const std::vector<Vec3b>& palette) {
    Mat1b output(image.width, image.height);
    for (int y = 0; y < image.height; y++)
        for (int x = 0; x < image.width; x++)
            output.at(y, x) = std::find(palette.begin(), palette.end(), image.at(y, x)) - palette.begin();
    return output;
}

struct makeCCImage_state {
    int i;
    uchar* colors;
    uchar** chars;
    uchar** cols;
    uchar3 * pal;
};

static void makeCCImage_worker(void* s) {
    makeCCImage_state * state = (makeCCImage_state*)s;
    toCCPixel(state->colors + (state->i * 6), *state->chars + state->i, *state->cols + state->i, state->pal);
    delete state;
}

void makeCCImage(const Mat1b& input, const std::vector<Vec3b>& palette, uchar** chars, uchar** cols) {
    // Create the input and output data
    int width = input.width - input.width % 2, height = input.height - input.height % 3;
    uchar * colors = new uchar[height * width];
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x+=2) {
            if (input[y][x] > 15) throw std::runtime_error("Too many colors (1)");
            if (input[y][x+1] > 15) throw std::runtime_error("Too many colors (2)");
            colors[(y-y%3)*width + x*3 + (y%3)*2] = input[y][x];
            colors[(y-y%3)*width + x*3 + (y%3)*2 + 1] = input[y][x+1];
        }
    }
    *chars = new uchar[(height / 3) * (width / 2)];
    *cols = new uchar[(height / 3) * (width / 2)];
    uchar3 * pal = new uchar3[16];
    for (int i = 0; i < palette.size(); i++) pal[i] = {palette[i][0], palette[i][1], palette[i][2]};
    initCL();
    if (/*CLPlatform() &&*/ false) {
        // Use OpenCL for computation
    } else {
        // Use normal CPU for computation
        for (int i = 0; i < (height / 3) * (width / 2); i++)
            work.push(makeCCImage_worker, new makeCCImage_state {i, colors, chars, cols, pal});
        work.wait();
    }
    delete[] pal;
    delete[] colors;
}

std::string makeTable(const uchar * characters, const uchar * colors, const std::vector<Vec3b>& palette, int width, int height, bool compact = false, bool embedPalette = false) {
    std::stringstream retval;
    retval << (compact ? "{" : "{\n");
    for (int y = 0; y < height; y++) {
        std::string text, fg, bg;
        for (int x = 0; x < width; x++) {
            uchar c = characters[y*width+x], cc = colors[y*width+x];
            if (c >= 32 && c < 127 && c != '"' && c != '\\') text += c;
            else text += "\\" + std::to_string(c);
            fg += hexstr[cc & 0xf];
            bg += hexstr[cc >> 4];
        }
        if (compact) retval << "{\"" << text << "\",\"" << fg << "\",\"" << bg << "\"},";
        else retval << "    {\n        \"" << text << "\",\n        \"" << fg << "\",\n        \"" << bg << "\"\n    },\n";
    }
    retval << (embedPalette ? "    palette = {\n" : (compact ? "},{" : "}, {\n"));
    for (const Vec3b& c : palette) {
        if (compact) retval << "{" << std::to_string(c[2] / 255.0) << "," << std::to_string(c[1] / 255.0) << "," << std::to_string(c[0] / 255.0) << "},";
        else retval << "    {" << std::to_string(c[2] / 255.0) << ", " << std::to_string(c[1] / 255.0) << ", " << std::to_string(c[0] / 255.0) << "},\n";
    }
    retval << (embedPalette ? "    }\n}" : "}");
    return retval.str();
}

std::string makeRawImage(const uchar * screen, const uchar * colors, const std::vector<Vec3b>& palette, int width, int height) {
    std::stringstream output;
    for (int i = 0; i < 4; i++) output.put(0);
    output.write((char*)&width, 2);
    output.write((char*)&height, 2);
    for (int i = 0; i < 8; i++) output.put(0);
    unsigned char c = screen[0];
    unsigned char n = 0;
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x++) {
            if (screen[y*width+x] != c || n == 255) {
                output.put(c);
                output.put(n);
                c = screen[y*width+x];
                n = 0;
            }
            n++;
        }
    }
    if (n > 0) {
        output.put(c);
        output.put(n);
    }
    c = colors[0];
    n = 0;
    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x++) {
            if (colors[y*width+x] != c || n == 255) {
                output.put(c);
                output.put(n);
                c = colors[y*width+x];
                n = 0;
            }
            n++;
        }
    }
    if (n > 0) {
        output.put(c);
        output.put(n);
    }
    for (int i = 0; i < 16; i++) {
        output.put(palette[i][2]);
        output.put(palette[i][1]);
        output.put(palette[i][0]);
    }
    std::string orig = output.str();
    std::stringstream ss;
    Poco::Base64Encoder enc(ss);
    enc.write(orig.c_str(), orig.size());
    enc.close();
    std::string str = ss.str();
    str.erase(std::remove_if(str.begin(), str.end(), [](char c)->bool {return c == '\n' || c == '\r'; }), str.end());
    Poco::Checksum chk;
    chk.update(orig);
    const uint32_t sum = chk.checksum();
    char tmpdata[21];
    if (str.length() > 65535) {
        snprintf(tmpdata, 21, "%012zX%08x", str.length(), sum);
        return "!CPD" + std::string(tmpdata, 12) + str + std::string(tmpdata + 12, 8) + "\n";
    } else {
        snprintf(tmpdata, 13, "%04X%08x", (unsigned)str.length(), sum);
        return "!CPC" + std::string(tmpdata, 4) + str + std::string(tmpdata + 4, 8) + "\n";
    }
}

std::string make32vid_5bit(const uchar * characters, const uchar * colors, const std::vector<Vec3b>& palette, int width, int height) {
    std::string screen, col, pal;
    uint64_t next5bit = 0;
    uchar next5bit_pos = 0;
    for (int i = 0; i < width * height; i++) {
        next5bit = (next5bit << 5) | (characters[i] & 0x1F);
        col += colors[i];
        if (++next5bit_pos == 8) {
            screen += (char)((next5bit >> 32) & 0xFF);
            screen += (char)((next5bit >> 24) & 0xFF);
            screen += (char)((next5bit >> 16) & 0xFF);
            screen += (char)((next5bit >> 8) & 0xFF);
            screen += (char)(next5bit & 0xFF);
            next5bit = next5bit_pos = 0;
        }
    }
    if (next5bit_pos) {
        next5bit <<= (8 - next5bit_pos) * 5;
        screen += (char)((next5bit >> 32) & 0xFF);
        screen += (char)((next5bit >> 24) & 0xFF);
        screen += (char)((next5bit >> 16) & 0xFF);
        screen += (char)((next5bit >> 8) & 0xFF);
        screen += (char)(next5bit & 0xFF);
        next5bit = next5bit_pos = 0;
    }
    for (int i = 0; i < 16; i++) {
        if (i < palette.size()) {
            pal += palette[i][2];
            pal += palette[i][1];
            pal += palette[i][0];
        } else pal += std::string((size_t)3, (char)0);
    }
    return screen + col + pal;
}

std::string make32vid_6bit(const uchar * characters, const uchar * colors, const std::vector<Vec3b>& palette, int width, int height) {
    std::string screen, col, pal;
    uchar lastColor = 0;
    uint32_t next6bit = 0;
    uchar next6bit_pos = 0;
    for (int i = 0; i < width * height; i++) {
        uchar tok = characters[i] & 0x1F;
        uchar c = colors[i];
        if (((c >> 4) | (c << 4)) == lastColor) {
            c = lastColor;
            tok |= 0x20;
        }
        lastColor = c;
        col += c;
        next6bit = (next6bit << 6) | tok;
        if (++next6bit_pos == 4) {
            screen += (char)((next6bit >> 16) & 0xFF);
            screen += (char)((next6bit >> 8) & 0xFF);
            screen += (char)(next6bit & 0xFF);
            next6bit = next6bit_pos = 0;
        }
    }
    if (next6bit_pos) {
        next6bit <<= (4 - next6bit_pos) * 6;
        screen += (char)((next6bit >> 16) & 0xFF);
        screen += (char)((next6bit >> 8) & 0xFF);
        screen += (char)(next6bit & 0xFF);
    }
    for (int i = 0; i < 16; i++) {
        if (i < palette.size()) {
            pal += palette[i][2];
            pal += palette[i][1];
            pal += palette[i][0];
        } else pal += std::string((size_t)3, (char)0);
    }
    return screen + col + pal;
}

std::string makeLuaFile(const uchar * characters, const uchar * colors, const std::vector<Vec3b>& palette, int width, int height) {
    return "local image, palette = " + makeTable(characters, colors, palette, width, height) + "\n\nterm.clear()\nfor i, v in ipairs(palette) do term.setPaletteColor(2^(i-1), table.unpack(v)) end\nfor y, r in ipairs(image) do\n    term.setCursorPos(1, y)\n    term.blit(table.unpack(r))\nend\nread()\nfor i = 0, 15 do term.setPaletteColor(2^i, term.nativePaletteColor(2^i)) end\nterm.setBackgroundColor(colors.black)\nterm.setTextColor(colors.white)\nterm.setCursorPos(1, 1)\nterm.clear()\n";
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
            try {
                frame = std::stoi(path.substr(7));
            } catch (std::exception &e) {
                response.setStatusAndReason(HTTPResponse::HTTP_BAD_REQUEST);
                response.setContentType("text/plain");
                response.send().write("Invalid path", 12);
                return;
            }
            if (frame < 0 || frame > audioStorageSize / 48000) {
                response.setStatusAndReason(HTTPResponse::HTTP_NOT_FOUND);
                response.setContentType("text/plain");
                response.send().write("404 Not Found", 13);
                return;
            }
            response.setStatusAndReason(HTTPResponse::HTTP_OK);
            response.setContentType("application/octet-stream");
            response.send().write((char*)(audioStorage + frame * 48000), frame == audioStorageSize / 48000 ? audioStorageSize % 48000 : 48000);
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

static void serveWebSocket(WebSocket& ws, double * fps) {
    char buf[256];
    int n, flags;
    do {
        flags = 0;
        try {n = ws.receiveFrame(buf, 256, flags);}
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
                if (frame >= frameStorage.size() || frame < 0) ws.sendFrame("!", 1, WebSocket::FRAME_TEXT);
                else for (size_t i = 0; i < frameStorage[frame].size(); i += 65535)
                    ws.sendFrame(frameStorage[frame].c_str() + i, min(frameStorage[frame].size() - i, (size_t)65535), WebSocket::FRAME_BINARY);
                if (streamed) frameStorage[frame] = "";
            } else if (buf[0] == 'a') {
                int offset = std::stoi(std::string(buf + 1, n - 1));
                if (streamed) {
                    if (audioStorageSize < 48000) {
                        audioStorage = (uint8_t*)realloc(audioStorage, 48000);
                        memset(audioStorage + max(audioStorageSize, 0L), 0, 48000 - max(audioStorageSize, 0L));
                    }
                    if (audioStorageSize > -48000) ws.sendFrame(audioStorageSize < 0 ? audioStorage - audioStorageSize : audioStorage, 48000, WebSocket::FRAME_BINARY);
                    if (audioStorageSize > 48000) memmove(audioStorage, audioStorage + 48000, audioStorageSize - 48000);
                    audioStorageSize -= 48000;
                }
                else if (offset >= audioStorageSize || offset < 0) ws.sendFrame("!", 1, WebSocket::FRAME_TEXT);
                else ws.sendFrame(audioStorage + offset, offset + 48000 > audioStorageSize ? audioStorageSize - offset : 48000, WebSocket::FRAME_BINARY);
            } else if (buf[0] == 'n') {
                std::string data = std::to_string(streamed ? totalFrames : frameStorage.size());
                ws.sendFrame(data.c_str(), data.size(), WebSocket::FRAME_TEXT);
            } else if (buf[0] == 'f') {
                std::string data = std::to_string(*fps);
                ws.sendFrame(data.c_str(), data.size(), WebSocket::FRAME_TEXT);
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
            serveWebSocket(ws, fps);
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

static void sighandler(int signal) {
    std::unique_lock<std::mutex> lock(exitLock);
    exitNotify.notify_all();
}

static std::string avErrorString(int err) {
    char errstr[256];
    av_strerror(err, errstr, 256);
    return std::string(errstr);
}

class HelpException: public OptionException {
public:
    virtual const char * className() const noexcept {return "HelpException";}
    virtual const char * name() const noexcept {return "Help";}
};

#define VID32_FLAG_VIDEO_COMPRESSION_NONE     0x0000
#define VID32_FLAG_VIDEO_COMPRESSION_LZW      0x0001
#define VID32_FLAG_VIDEO_COMPRESSION_DEFLATE  0x0002
#define VID32_FLAG_AUDIO_COMPRESSION_NONE     0x0000
#define VID32_FLAG_AUDIO_COMPRESSION_DFPWM    0x0004
#define VID32_FLAG_VIDEO_5BIT_CODES           0x0010

struct Vid32Header {
    char magic[4];
    uint16_t width;
    uint16_t height;
    uint8_t fps;
    uint8_t nstreams;
    uint16_t flags;
};

struct Vid32Chunk {
    uint32_t size;
    uint32_t nframes;
    uint8_t type;
    uint8_t data[];
    enum class Type {
        Video = 0,
        Audio,
        AudioLeft,
        AudioRight,
        AudioCh3,
        AudioCh4,
        AudioCh5,
        AudioCh6,
        Subtitle,
        Subtitle2,
        Subtitle3,
        Subtitle4
    };
};

struct Vid32SubtitleEvent {
    uint32_t start;
    uint32_t length;
    uint16_t x;
    uint16_t y;
    uint8_t colors;
    uint8_t flags;
    uint16_t size;
    char text[];
};

struct ASSSubtitleEvent {
    unsigned width;
    unsigned height;
    uint8_t alignment;
    int marginLeft;
    int marginRight;
    int marginVertical;
    Vec3b color;
    std::string text;
};

enum class OutputType {
    Default,
    Lua,
    Raw,
    Vid32,
    HTTP,
    WebSocket,
    BlitImage
};

static double parseTime(const std::string& str) {
    return (str[0] - '0') * 3600 + (str[2] - '0') * 600 + (str[3] - '0') * 60 + (str[5] - '0') * 10 + (str[6] - '0') * 1 + (str[8] - '0') * 0.1 + (str[9] - '0') * 0.01;
}

static Vec3b parseColor(const std::string& str) {
    uint32_t color;
    if (str.substr(0, 2) == "&H") color = std::stoul(str.substr(2), NULL, 16);
    else color = std::stoul(str);
    return {color & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF};
}

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

void renderSubtitles(const std::unordered_multimap<int, ASSSubtitleEvent>& subtitles, int nframe, uchar * characters, uchar * colors, const std::vector<Vec3b>& palette, int width, int height) {
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
            int start = (startY / 3) * (width / 2) + (startX / 2);
            for (int x = 0; x < lines[i].size(); x++) {
                characters[start+x] = lines[i][x];
                colors[start+x] = 0xF0 | color;
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
    options.addOption(Option("raw", "r", "Output a rawmode-based image/video file (default for videos)"));
    options.addOption(Option("blit-image", "b", "Output a blit image (BIMG) format image/animation file"));
    options.addOption(Option("32vid", "3", "Output a 32vid format binary video file with compression + audio"));
    options.addOption(Option("http", "s", "Serve an HTTP server that has each frame split up + a player program", false, "port", true).validator(new IntValidator(1, 65535)));
    options.addOption(Option("websocket", "w", "Serve a WebSocket that sends the image/video with audio", false, "port", true).validator(new IntValidator(1, 65535)));
    options.addOption(Option("websocket-client", "", "Connect to a WebSocket server to send image/video with audio", false, "url", true).validator(new RegExpValidator("^wss?://(?:[A-Za-z0-9!#$%&'*+\\-\\/=?^_`{|}~]+(?:\\.[A-Za-z0-9!#$%&'*+\\-\\/=?^_`{|}~]+)*|\\[[\x21-\x5A\x5E-\x7E]*\])(?::\\d+)?(?:/[^/]+)*$")));
    options.addOption(Option("streamed", "S", "For servers, encode data on-the-fly instead of doing it ahead of time (saves memory at the cost of speed and only one client)"));
    options.addOption(Option("default-palette", "p", "Use the default CC palette instead of generating an optimized one"));
    options.addOption(Option("threshold", "t", "Use thresholding instead of dithering"));
    options.addOption(Option("octree", "8", "Use octree for higher quality color conversion (slower)"));
    options.addOption(Option("kmeans", "k", "Use k-means for highest quality color conversion (slowest)"));
    options.addOption(Option("5bit", "5", "Use 5-bit codes in 32vid (internal testing)"));
    options.addOption(Option("compression", "c", "Compression type for 32vid videos", false, "none|lzw|deflate", true).validator(new RegExpValidator("^(none|lzw|deflate)$")));
    options.addOption(Option("compression-level", "L", "Compression level for 32vid videos when using DEFLATE", false, "1-9", true).validator(new IntValidator(1, 9)));
    options.addOption(Option("width", "W", "Resize the image to the specified width", false, "size", true).validator(new IntValidator(1, 65535)));
    options.addOption(Option("height", "H", "Resize the image to the specified height", false, "size", true).validator(new IntValidator(1, 65535)));
    options.addOption(Option("help", "h", "Show this help"));
    OptionProcessor argparse(options);
    argparse.setUnixStyle(true);

    std::string input, output, subtitle;
    bool useDefaultPalette = false, noDither = false, useOctree = false, use5bit = false, useKmeans = false;
    OutputType mode = OutputType::Default;
    int compression = VID32_FLAG_VIDEO_COMPRESSION_DEFLATE;
    int port = 80, width = -1, height = -1, zlibCompression = 5;
    try {
        for (int i = 1; i < argc; i++) {
            std::string option, arg;
            if (argparse.process(argv[i], option, arg)) {
                if (option == "input") input = arg;
                else if (option == "subtitle") subtitle = arg;
                else if (option == "output") output = arg;
                else if (option == "lua") mode = OutputType::Lua;
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
                else if (option == "5bit") use5bit = true;
                else if (option == "compression") {
                    if (arg == "none") compression = VID32_FLAG_VIDEO_COMPRESSION_NONE;
                    else if (arg == "lzw") compression = VID32_FLAG_VIDEO_COMPRESSION_LZW;
                    else if (arg == "deflate") compression = VID32_FLAG_VIDEO_COMPRESSION_DEFLATE;
                } else if (option == "compression-level") zlibCompression = std::stoi(arg);
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
        help.setUnixStyle(true);
        help.setUsage("[options] -i <input> [-o <output> | -s <port> | -w <port>]");
        help.setCommand(argv[0]);
        help.setHeader("sanjuuni converts images and videos into a format that can be displayed in ComputerCraft.");
        help.setFooter("sanjuuni is licensed under the GPL license. Get the source at https://github.com/MCJack123/sanjuuni.");
        help.format(e.className() == "HelpException" ? std::cout : std::cerr);
        return 0;
    }

    AVFormatContext * format_ctx = NULL;
    AVCodecContext * video_codec_ctx = NULL, * audio_codec_ctx = NULL;
    const AVCodec * video_codec = NULL, * audio_codec = NULL;
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
    // Initialize packets/frames
    AVPacket * packet = av_packet_alloc();
    AVFrame * frame = av_frame_alloc();

    std::ofstream outfile;
    if (output != "-" && output != "") {
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
    double fps = 0;
    int nframe = 0;
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
                Context::Ptr ctx = new Context(Context::CLIENT_USE, "", Context::VERIFY_RELAXED, 9, true, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
                addSystemCertificates(ctx);
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
                std::cerr << "\rframe " << nframe++ << "/" << format_ctx->streams[video_stream]->nb_frames;
                std::cerr.flush();
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
                octree_tree tree;
                if (useDefaultPalette) palette = defaultPalette;
                else if (useKmeans) palette = kMeans(rs, 16);
                else palette = reducePalette(rs, 16, useOctree ? &tree : NULL);
                if (useOctree && !useDefaultPalette) octree_free_node(tree.root);
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
                } case OutputType::Raw: {
                    outstream << makeRawImage(characters, colors, palette, pimg.width / 2, pimg.height / 3);
                    outstream.flush();
                    break;
                } case OutputType::BlitImage: {
                    outstream << makeTable(characters, colors, palette, pimg.width / 2, pimg.height / 3, false, true) << ",\n";
                    outstream.flush();
                    break;
                } case OutputType::Vid32: {
                    if (use5bit) videoStream += make32vid_5bit(characters, colors, palette, pimg.width / 2, pimg.height / 3);
                    else videoStream += make32vid_6bit(characters, colors, palette, pimg.width / 2, pimg.height / 3);
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
        } else if (packet->stream_index == audio_stream && mode != OutputType::Lua && mode != OutputType::Raw) {
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
                if (audioStorageSize + newframe->nb_samples > 0) {
                    unsigned offset = audioStorageSize < 0 ? -audioStorageSize : 0;
                    audioStorage = (uint8_t*)realloc(audioStorage, audioStorageSize + newframe->nb_samples);
                    memcpy(audioStorage + audioStorageSize + offset, newframe->data[0] + offset, newframe->nb_samples - offset);
                }
                audioStorageSize += newframe->nb_samples;
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
        header.nstreams = 2;
        header.flags = compression;
        if (use5bit) header.flags |= VID32_FLAG_VIDEO_5BIT_CODES;

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
            outfile.write((char*)&audioChunk, 9);
            outfile.write((char*)audioStorage, audioStorageSize);
        }
    } else if (mode == OutputType::BlitImage) {
        char timestr[26];
        time_t now = time(0);
        struct tm * time = gmtime(&now);
        strftime(timestr, 26, "%FT%T%z", time);
        outfile << "creator = 'sanjuuni',\nversion = '1.0',\nsecondsPerFrame = " << (1.0 / fps) << ",\nanimation = " << (nframe > 1 ? "true" : "false") << ",\ndate = '" << timestr << "',\ntitle = '" << input << "'\n}\n";
    }
cleanup:
    std::cerr << "\rframe " << nframe << "/" << format_ctx->streams[video_stream]->nb_frames << "\n";
    if (outfile.is_open()) outfile.close();
    if (resize_ctx) sws_freeContext(resize_ctx);
    if (resample_ctx) swr_free(&resample_ctx);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&video_codec_ctx);
    if (audio_codec_ctx) avcodec_free_context(&audio_codec_ctx);
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
