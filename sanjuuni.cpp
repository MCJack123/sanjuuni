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
}
//#include <CL/opencl.hpp>
#include <Poco/Base64Encoder.h>
#include <Poco/Checksum.h>
#include <Poco/Util/OptionProcessor.h>
#include <Poco/Util/OptionSet.h>
#include <Poco/Util/OptionException.h>
#include <Poco/Util/IntValidator.h>
#include <Poco/Util/HelpFormatter.h>
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
struct Vec3i : public std::array<int, 3> {
    Vec3i() = default;
    template<typename T> Vec3i(const std::array<T, 3>& a) {(*this)[0] = a[0]; (*this)[1] = a[1]; (*this)[2] = a[2];}
    template<typename T> Vec3i(std::initializer_list<T> il) {(*this)[0] = *(il.begin()); (*this)[1] = *(il.begin()+1); (*this)[2] = *(il.begin()+2);}
    Vec3i operator*(double b) {return {(*this)[0] * b, (*this)[1] * b, (*this)[2] * b};}
    Vec3i operator/(double b) {return {(*this)[0] / b, (*this)[1] / b, (*this)[2] / b};}
    Vec3i operator-(const Vec3i& b) {return {(*this)[0] - b[0], (*this)[1] - b[1], (*this)[2] - b[2]};}
};
struct Vec3b : public std::array<uint8_t, 3> {
    Vec3b() = default;
    template<typename T> Vec3b(const std::array<T, 3>& a) {(*this)[0] = a[0]; (*this)[1] = a[1]; (*this)[2] = a[2];}
    template<typename T> Vec3b(std::initializer_list<T> il) {(*this)[0] = *(il.begin()); (*this)[1] = *(il.begin()+1); (*this)[2] = *(il.begin()+2);}
    Vec3b& operator+=(const Vec3i& a) {(*this)[0] += a[0]; (*this)[1] += a[1]; (*this)[2] += a[2]; return *this;}
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
typedef vector2d<uint8_t> Mat1b;
typedef vector2d<Vec3b> Mat;
template<typename T> T min(T a, T b) {return a < b ? a : b;}
template<typename T> T max(T a, T b) {return a > b ? a : b;}

//cl::Platform CLPlatform;
bool isOpenCLInitialized = false;
static const std::string hexstr = "0123456789abcdef";
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

static std::vector<Vec3b> medianCut(std::vector<Vec3b>& pal, int num) {
    if (num == 1) {
        Vec3i sum = {0, 0, 0};
        for (const Vec3b& v : pal) {sum[0] += v[0]; sum[1] += v[1]; sum[2] += v[2];}
        return {Vec3b(sum / (double)pal.size())};
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
        std::sort(pal.begin(), pal.end(), [maxComponent](const Vec3b& a, const Vec3b& b)->bool {return a[maxComponent] < b[maxComponent];});
        std::vector<Vec3b> a, b;
        for (int i = 0; i < pal.size(); i++) {
            if (i < pal.size() / 2) a.push_back(pal[i]);
            else b.push_back(pal[i]);
        }
        std::vector<Vec3b> ar, br;
        std::thread at([&ar, &a, num](){ar = medianCut(a, num / 2);}), bt([&br, &b, num](){br = medianCut(b, num / 2);});
        at.join(); bt.join();
        for (const Vec3b& v : br) ar.push_back(v);
        return ar;
    }
}

std::vector<Vec3b> reducePalette(const Mat& image, int numColors) {
    int e = 0;
    if (frexp(numColors, &e) > 0.5) throw std::invalid_argument("color count must be a power of 2");
    std::vector<Vec3b> pal;
    for (int y = 0; y < image.height; y++)
        for (int x = 0; x < image.width; x++)
            pal.push_back(Vec3b(image.at(y, x)));
    std::vector<Vec3b> uniq(pal);
    uniq.erase(std::unique(uniq.begin(), uniq.end()), uniq.end());
    if (numColors >= uniq.size()) return uniq;
    return medianCut(pal, numColors);
}

static Vec3b nearestColor(const std::vector<Vec3b>& palette, const Vec3b& color) {
    double n, dist = 1e100;
    for (int i = 0; i < palette.size(); i++) {
        const Vec3b& v = palette[i];
        double d = sqrt((v[0] - color[0])*(v[0] - color[0]) + (v[1] - color[1])*(v[1] - color[1]) + (v[2] - color[2])*(v[2] - color[2]));
        if (d < dist) {n = i; dist = d;}
    }
    return palette[n];
}

static void thresholdWorker(const Mat * in, Mat * out, const std::vector<Vec3b> * palette, std::queue<int> * queue, std::mutex * mtx) {
    while (true) {
        mtx->lock();
        if (queue->empty()) {
            mtx->unlock();
            return;
        }
        int row = queue->front();
        queue->pop();
        mtx->unlock();
        for (int x = 0; x < in->width; x++) out->at(row, x) = nearestColor(*palette, in->at(row, x));
    }
}

Mat thresholdImage(const Mat& image, const std::vector<Vec3b>& palette) {
    Mat output(image.width, image.height);
    initCL();
    if (/*CLPlatform() &&*/ false) {
        // Use OpenCL for computation
    } else {
        // Use normal CPU for computation
        std::queue<int> height;
        std::mutex mtx;
        std::vector<std::thread*> thread_pool;
        unsigned nthreads = std::thread::hardware_concurrency();
        if (!nthreads) nthreads = 8; // default if no value is available
        mtx.lock(); // pause threads until queue is filled
        for (int i = 0; i < nthreads; i++) thread_pool.push_back(new std::thread(thresholdWorker, &image, &output, &palette, &height, &mtx));
        for (int i = 0; i < image.height; i++) height.push(i);
        mtx.unlock(); // release threads to start working
        for (int i = 0; i < nthreads; i++) {thread_pool[i]->join(); delete thread_pool[i];}
    }
    return output;
}

Mat ditherImage(Mat image, const std::vector<Vec3b>& palette) {
    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            Vec3b c = image.at(y, x);
            Vec3b newpixel = nearestColor(palette, c);
            image.at(y, x) = newpixel;
            Vec3i err = Vec3i(c) - Vec3i(newpixel);
            if (x < image.width - 1) image.at(y, x + 1) += err * (7.0/16.0);
            if (y < image.height - 1) {
                if (x > 1) image.at(y + 1, x - 1) += err * (3.0/16.0);
                image.at(y + 1, x) += err * (5.0/16.0);
                if (x < image.width - 1) image.at(y + 1, x + 1) += err * (1.0/16.0);
            }
        }
    }
    return image;
}

Mat1b rgbToPaletteImage(const Mat& image, const std::vector<Vec3b>& palette) {
    Mat1b output(image.width, image.height);
    for (int y = 0; y < image.height; y++)
        for (int x = 0; x < image.width; x++)
            output.at(y, x) = std::find(palette.begin(), palette.end(), image.at(y, x)) - palette.begin();
    return output;
}

static void ccImageWorker(uchar * colors, uchar * character, uchar * color, uchar3 * palette, std::queue<int> * queue, std::mutex * mtx) {
    while (true) {
        mtx->lock();
        if (queue->empty()) {
            mtx->unlock();
            return;
        }
        int offset = queue->front();
        queue->pop();
        mtx->unlock();
        toCCPixel(colors + (offset * 6), character + offset, color + offset, palette);
    }
}

void makeCCImage(const Mat1b& input, const std::vector<Vec3b>& palette, uchar** chars, uchar** width) {
    // Create the input and output data
    uchar * colors = new uchar[input.height * input.width];
    for (int y = 0; y < input.height - input.height % 3; y++) {
        for (int x = 0; x < input.width - input.width % 2; x+=2) {
            if (input[y][x] > 15) throw std::runtime_error("Too many colors (1)");
            if (input[y][x+1] > 15) throw std::runtime_error("Too many colors (2)");
            colors[(y-y%3)*input.width + x*3 + (y%3)*2] = input[y][x];
            colors[(y-y%3)*input.width + x*3 + (y%3)*2 + 1] = input[y][x+1];
        }
    }
    *chars = new uchar[(input.height / 3) * (input.width / 2)];
    *width = new uchar[(input.height / 3) * (input.width / 2)];
    uchar3 * pal = new uchar3[palette.size()];
    for (int i = 0; i < palette.size(); i++) pal[i] = {palette[i][0], palette[i][1], palette[i][2]};
    initCL();
    if (/*CLPlatform() &&*/ false) {
        // Use OpenCL for computation
    } else {
        // Use normal CPU for computation
        std::queue<int> offsets;
        std::mutex mtx;
        std::vector<std::thread*> thread_pool;
        unsigned nthreads = std::thread::hardware_concurrency();
        if (!nthreads) nthreads = 8; // default if no value is available
        mtx.lock(); // pause threads until queue is filled
        for (int i = 0; i < nthreads; i++) thread_pool.push_back(new std::thread(ccImageWorker, colors, *chars, *width, pal, &offsets, &mtx));
        for (int i = 0; i < (input.height / 3) * (input.width / 2); i++) offsets.push(i);
        mtx.unlock(); // release threads to start working
        for (int i = 0; i < nthreads; i++) {thread_pool[i]->join(); delete thread_pool[i];}
    }
    delete[] pal;
    delete[] colors;
}

std::string makeLuaFile(const uchar * characters, const uchar * colors, const std::vector<Vec3b>& palette, int width, int height) {
    std::string retval = "local image, palette = {\n";
    for (int y = 0; y < height; y++) {
        std::string text, fg, bg;
        for (int x = 0; x < width; x++) {
            uchar c = characters[y*width+x], cc = colors[y*width+x];
            text += "\\" + std::to_string(c);
            fg += hexstr.substr(cc & 0xf, 1);
            bg += hexstr.substr(cc >> 4, 1);
        }
        retval += "    {\n        \"" + text + "\",\n        \"" + fg + "\",\n        \"" + bg + "\"\n    },\n";
    }
    retval += "}, {\n";
    for (const Vec3b& c : palette) {
        retval += "    {" + std::to_string(c[2] / 255.0) + ", " + std::to_string(c[1] / 255.0) + ", " + std::to_string(c[0] / 255.0) + "},\n";
    }
    retval += "}\n\nfor i, v in ipairs(palette) do term.setPaletteColor(2^(i-1), table.unpack(v)) end\nfor y, r in ipairs(image) do\n    term.setCursorPos(1, y)\n    term.blit(table.unpack(r))\nend\nread()\nfor i = 0, 15 do term.setPaletteColor(2^i, term.nativePaletteColor(2^i)) end\nterm.setBackgroundColor(colors.black)\nterm.setTextColor(colors.white)\nterm.setCursorPos(1, 1)\nterm.clear()\n";
    return retval;
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

int main(int argc, const char * argv[]) {
    OptionSet options;
    options.addOption(Option("input", "i", "Input image or video", true, "file", true));
    options.addOption(Option("output", "o", "Output file path", false, "path", true));
    options.addOption(Option("lua", "l", "Output a Lua script file (default for images; only does one frame)"));
    options.addOption(Option("raw", "r", "Output a rawmode-based image/video file (default for videos)"));
    options.addOption(Option("http", "s", "Serve an HTTP server that has each frame split up + a player program", false, "port", true).validator(new IntValidator(1, 65535)));
    options.addOption(Option("websocket", "w", "Serve a WebSocket that sends the image/video with audio", false, "port", true).validator(new IntValidator(1, 65535)));
    options.addOption(Option("default-palette", "p", "Use the default CC palette instead of generating an optimized one"));
    options.addOption(Option("threshold", "t", "Use thresholding instead of dithering"));
    options.addOption(Option("width", "w", "Resize the image to the specified width", false, "size", true).validator(new IntValidator(1, 65535)));
    options.addOption(Option("height", "H", "Resize the image to the specified height", false, "size", true).validator(new IntValidator(1, 65535)));
    options.addOption(Option("help", "h", "Show this help"));
    OptionProcessor argparse(options);
    argparse.setUnixStyle(true);

    std::string input, output;
    bool useDefaultPalette = false, noDither = false;
    int mode = -1, port;
    int width = -1, height = -1;
    try {
        for (int i = 1; i < argc; i++) {
            std::string option, arg;
            if (argparse.process(argv[i], option, arg)) {
                if (option == "input") input = arg;
                else if (option == "output") output = arg;
                else if (option == "lua") mode = 0;
                else if (option == "raw") mode = 1;
                else if (option == "http") {mode = 2; port = std::stoi(arg);}
                else if (option == "websocket") {mode = 3; port = std::stoi(arg);}
                else if (option == "default-palette") useDefaultPalette = true;
                else if (option == "threshold") noDither = true;
                else if (option == "width") width = std::stoi(arg);
                else if (option == "height") height = std::stoi(arg);
                else if (option == "help") throw HelpException();
            }
        }
        argparse.checkRequired();
        if (!(mode == 2 || mode == 3) && output == "") throw MissingOptionException("Required option not specified: output");
    } catch (const OptionException &e) {
        if (e.className() != "HelpException") std::cerr << e.displayText() << "\n";
        HelpFormatter help(options);
        help.setUnixStyle(true);
        help.setCommand(argv[0]);
        help.setHeader("sanjuuni converts images and videos into a format that can be displayed in ComputerCraft.");
        help.format(e.className() == "HelpException" ? std::cout : std::cerr);
        return 0;
    }

    AVFormatContext * format_ctx = NULL;
    AVCodecContext * video_codec_ctx = NULL, * audio_codec_ctx = NULL;
    AVCodec * video_codec = NULL, * audio_codec = NULL;
    SwsContext * resize_ctx = NULL;
    int error, video_stream = -1, audio_stream = -1;
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

    std::string rawtmp, luatmp;
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
    if (mode == 1) outstream << "32Vid 1.1\n" << ((double)video_codec_ctx->framerate.num / (double)video_codec_ctx->framerate.den) << "\n";
    int nframe = 0;
    while (av_read_frame(format_ctx, packet) >= 0) {
        if (packet->stream_index == video_stream) {
            avcodec_send_packet(video_codec_ctx, packet);
            while ((error = avcodec_receive_frame(video_codec_ctx, frame)) == 0) {
                std::cerr << "\rframe " << nframe++ << "/" << format_ctx->streams[video_stream]->nb_frames;
                std::cerr.flush();
                Mat in(frame->width, frame->height), out;
                if (resize_ctx == NULL) {
                    if (width != -1 || height != -1) {
                        width = width == -1 ? height * ((double)in.width / (double)in.height) : width;
                        height = height == -1 ? width * ((double)in.height / (double)in.width) : height;
                    } else {
                        width = frame->width;
                        height = frame->height;
                    }
                    resize_ctx = sws_getContext(frame->width, frame->height, (AVPixelFormat)frame->format, width, height, AV_PIX_FMT_BGR24, SWS_BICUBIC, NULL, NULL, NULL);
                }
                if (mode == -1 && !luatmp.empty()) {
                    luatmp = "";
                    outstream << "32Vid 1.1\n" << ((double)video_codec_ctx->framerate.num / (double)video_codec_ctx->framerate.den) << "\n" << rawtmp;
                    mode = 1;
                }
                Mat rs(width, height);
                uint8_t * data = new uint8_t[width * height * 3];
                int stride[3] = {width * 3, width * 3, width * 3};
                uint8_t * ptrs[3] = {data, data + 1, data + 2};
                sws_scale(resize_ctx, frame->data, frame->linesize, 0, frame->height, ptrs, stride);
                for (int y = 0; y < height; y++)
                    for (int x = 0; x < width; x++)
                        rs.at(y, x) = {data[y*width*3+x*3], data[y*width*3+x*3+1], data[y*width*3+x*3+2]};
                delete[] data;
                std::vector<Vec3b> palette;
                if (useDefaultPalette) palette = defaultPalette;
                else palette = reducePalette(rs, 16);
                if (noDither) out = thresholdImage(rs, palette);
                else out = ditherImage(rs, palette);
                Mat1b pimg = rgbToPaletteImage(out, palette);
                uchar *characters, *colors;
                makeCCImage(pimg, palette, &characters, &colors);
                switch (mode) {
                case -1: {
                    rawtmp = makeRawImage(characters, colors, palette, pimg.width / 2, pimg.height / 3);
                    luatmp = makeLuaFile(characters, colors, palette, pimg.width / 2, pimg.height / 3);
                    break;
                } case 0: {
                    outstream << makeLuaFile(characters, colors, palette, pimg.width / 2, pimg.height / 3);
                    break;
                } case 1: {
                    outstream << makeRawImage(characters, colors, palette, pimg.width / 2, pimg.height / 3);
                    break;
                } case 2: {
                    std::cerr << "Unimplemented!\n";
                    break;
                } case 3: {
                    std::cerr << "Unimplemented!\n";
                    break;
                }
                }
                delete[] characters;
                delete[] colors;
            }
            if (error != AVERROR_EOF && error != AVERROR(EAGAIN)) {
                std::cerr << "Failed to grab frame: " << avErrorString(error) << "\n";
            }
        } else if (packet->stream_index == audio_stream) {

        }
    }
    if (!luatmp.empty()) outstream << luatmp;
    if (outfile.is_open()) outfile.close();
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&video_codec_ctx);
    if (audio_codec_ctx) avcodec_free_context(&audio_codec_ctx);
    avformat_close_input(&format_ctx);
    return 0;
}
