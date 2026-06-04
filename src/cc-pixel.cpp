/*
 * cc-pixel.cpp
 * Generates a ComputerCraft character+color from 2x3 pixel regions.
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

#define _NUM_ARGS2(X,X64,X63,X62,X61,X60,X59,X58,X57,X56,X55,X54,X53,X52,X51,X50,X49,X48,X47,X46,X45,X44,X43,X42,X41,X40,X39,X38,X37,X36,X35,X34,X33,X32,X31,X30,X29,X28,X27,X26,X25,X24,X23,X22,X21,X20,X19,X18,X17,X16,X15,X14,X13,X12,X11,X10,X9,X8,X7,X6,X5,X4,X3,X2,X1,N,...) N
#define NUM_ARGS(...) _NUM_ARGS2(0, __VA_ARGS__ ,64,63,62,61,60,59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)
#define ARG2(typ, name) ARG(typ, name)
#define ARG4(typ, name, ...)  ARG(typ, name), ARG2(__VA_ARGS__)
#define ARG6(typ, name, ...)  ARG(typ, name), ARG4(__VA_ARGS__)
#define ARG8(typ, name, ...)  ARG(typ, name), ARG6(__VA_ARGS__)
#define ARG10(typ, name, ...) ARG(typ, name), ARG8(__VA_ARGS__)
#define ARG12(typ, name, ...) ARG(typ, name), ARG10(__VA_ARGS__)
#define ARG14(typ, name, ...) ARG(typ, name), ARG12(__VA_ARGS__)
#define ARG16(typ, name, ...) ARG(typ, name), ARG14(__VA_ARGS__)
#define ARG18(typ, name, ...) ARG(typ, name), ARG16(__VA_ARGS__)
#define ARG20(typ, name, ...) ARG(typ, name), ARG18(__VA_ARGS__)
#define ARG22(typ, name, ...) ARG(typ, name), ARG20(__VA_ARGS__)
#define ARG24(typ, name, ...) ARG(typ, name), ARG22(__VA_ARGS__)
#define _ARGS_N3(N, ...) ARG ## N(__VA_ARGS__)
#define _ARGS_N2(N, ...) _ARGS_N3(N, __VA_ARGS__)
#define ARGS(...) _ARGS_N2(NUM_ARGS(__VA_ARGS__), __VA_ARGS__)

#if defined(WEBGPU)

#define ARG(typ, name) name: typ
#define PTR(space, typ) ??(ptr<space, typ>??)
#define KERNEL(name, ...) @compute @workgroup_size(64) fn name(ARGS(__VA_ARGS__))
#define FUNC(rtype, name, ...) fn name(ARGS(__VA_ARGS__)) -> rtype
#define VAR(typ, name) var name: typ
#define LOCALVAR(typ, name) var<function> name: typ
#define LET(typ, name) let name: typ
#define CONSTANT(typ, name) const name: typ
#define ARRAY(typ, name, sz) var name: array<typ, sz>
#define CONSTANTARRAY(typ, name, sz) const name: array<typ, sz>
#define CONSTANTARRAY2(typ, name, sz, sz2) const name: array<array<typ, sz2>, sz>
#define ARRAYARG(typ, name, sz) ??(array<typ, sz>??), name
#define ARRAYDEF(typ, sz, ...) array<typ, sz>(__VA_ARGS__)
#define TERNARY(cond, tv, fv) select(fv, tv, cond)
#define CAST(typ, val) typ(val)

#define uchar u32
#define int i32
#define uint u32
#define long i32
#define ulong u32
#define float f32
#define uchar2 vec2u
#define uchar3 vec3u
#define float3 vec3f
#define volatile

#elif defined(OPENGL)

#define ARG(typ, name) shared typ name;
#define PTR(space, typ) space typ*
#define KERNEL(name, ...) ARGS(__VA_ARGS__); void name()
#define FUNC(rtype, name, ...) static rtype name(ARGS(__VA_ARGS__))
#define VAR(typ, name) typ name
#define LOCALVAR(typ, name) typ name
#define LET(typ, name) __private const typ name
#define CONSTANT(typ, name) static __constant typ name
#define ARRAY(typ, name, sz) __private typ name[sz]
#define CONSTANTARRAY(typ, name, sz) static __constant typ name[sz]
#define CONSTANTARRAY2(typ, name, sz, sz2) static __constant typ name[sz][sz2]
#define ARRAYARG(typ, name, sz) __private typ*, name
#define ARRAYDEF(typ, sz, ...) {__VA_ARGS__}
#define TERNARY(cond, tv, fv) cond ? tv : fv
#define CAST(typ, val) (typ)(val)

#define private
#define workgroup shared
#define storage shared

#define get_global_id(n) gl_GlobalInvocationID.x
#define get_global_offset(n) gl_LocalInvocationID.y
#define get_group_id(n) gl_WorkGroupID.x
#define get_local_id(n) gl_LocalInvocationID.x
#define get_local_size(n) gl_WorkGroupSize.x
#define get_global_size(n) gl_WorkGroupSize.x * gl_WorkGroupSize.y

#elif defined(OPENCL)

#define ARG(typ, name) typ name
#define PTR(space, typ) space typ*
#define KERNEL(name, ...) __kernel void name(ARGS(__VA_ARGS__))
#define FUNC(rtype, name, ...) static rtype name(ARGS(__VA_ARGS__))
#define VAR(typ, name) __private typ name
#define LOCALVAR(typ, name) __local typ name
#define LET(typ, name) __private const typ name
#define CONSTANT(typ, name) static __constant typ name
#define ARRAY(typ, name, sz) __private typ name[sz]
#define CONSTANTARRAY(typ, name, sz) static __constant typ name[sz]
#define CONSTANTARRAY2(typ, name, sz, sz2) static __constant typ name[sz][sz2]
#define ARRAYARG(typ, name, sz) __private typ*, name
#define ARRAYDEF(typ, sz, ...) {__VA_ARGS__}
#define TERNARY(cond, tv, fv) cond ? tv : fv
#define CAST(typ, val) (typ)(val)

#define private __private
#define workgroup __local
#define storage __global
#define uniform __constant

#else

#include "sanjuuni.hpp"

#define ARG(typ, name) typ name
#define PTR(space, typ) typ*
#define KERNEL(name, ...) void name(ARGS(__VA_ARGS__))
#define FUNC(rtype, name, ...) static rtype name(ARGS(__VA_ARGS__))
#define VAR(typ, name) typ name
#define LOCALVAR(typ, name) static typ name
#define LET(typ, name) const typ name
#define CONSTANT(typ, name) static const typ name
#define ARRAY(typ, name, sz) typ name[sz]
#define CONSTANTARRAY(typ, name, sz) static const typ name[sz]
#define CONSTANTARRAY2(typ, name, sz, sz2) static const typ name[sz][sz2]
#define ARRAYARG(typ, name, sz) typ*, name
#define ARRAYDEF(typ, sz, ...) {__VA_ARGS__}
#define TERNARY(cond, tv, fv) (cond ? tv : fv)
#define CAST(typ, val) (typ)(val)

#define get_global_id(n) 0
#define get_global_offset(n) 0
#define get_group_id(n) 0
#define get_local_id(n) 0
#define get_local_size(n) 64
#define get_global_size(n) 0
#define barrier(n) ((void)0)
#define atomic_inc(n) (*n++)
#define CLK_LOCAL_MEM_FENCE 0
#define CLK_GLOBAL_MEM_FENCE 0

static float distance(float3 p0, float3 p1) {return sqrtf((p1.x - p0.x)*(p1.x - p0.x) + (p1.y - p0.y)*(p1.y - p0.y) + (p1.z - p0.z)*(p1.z - p0.z));}
float3 operator+(const float3& a, const float3& b) {return {a.x + b.x, a.y + b.y, a.z + b.z};}
float3 operator-(const float3& a, const float3& b) {return {a.x - b.x, a.y - b.y, a.z - b.z};}
float3& operator+=(float3& a, const float3& b) {a.x += b.x; a.y += b.y; a.z += b.z; return a;}
float3& operator+=(float3& a, float b) {a.x += b; a.y += b; a.z += b; return a;}
float3 operator*(const float3& v, float s) {return {v.x * s, v.y * s, v.z * s};}
int operator==(const float3& a, const float3& b) {return a.x == b.x && a.y == b.y && a.z == b.z;}
inline uchar3 vload3(int n, const uchar * data) {return {data[n*3], data[n*3+1], data[n*3+2]};}
inline float3 vload3(int n, const float * data) {return {data[n*3], data[n*3+1], data[n*3+2]};}
inline void vstore3(uchar3 v, int n, uchar * data) {data[n*3] = v.x; data[n*3+1] = v.y; data[n*3+2] = v.z;}
inline void vstore3(float3 v, int n, float * data) {data[n*3] = v.x; data[n*3+1] = v.y; data[n*3+2] = v.z;}

#endif

#ifdef SPIRV
#define atomic_inc(n) (*n++) // SPIRV-Tools doesn't support OpenCL 1.1, and apparently atomic_inc wasn't included in 1.0
#endif

/* OpenCL-compatible C to be able to copy straight to OpenCL */
/* can't use external functions besides what CLC provides */

CONSTANT(uint, MAX_LOCAL_SIZE) = 256;
#ifndef NULL
#define NULL ((void*)0)
#endif

FUNC(uchar, getComponent, uchar3, c, int, cmp) {
    switch (cmp) {
        case 0: {return c.x;}
        case 1: {return c.y;}
        case 2: {return c.z;}
        default: {return 0;}
    }
}

/* unrolled Floyd-Steinberg dithering */
FUNC(void, ditherCCImage, ARRAYARG(float3, img, 6), float3, a, float3, b, uchar, ac, uchar, bc, ARRAYARG(uchar, out, 6)) {
    VAR(float3, err);
    /* 0, 0 */
    if (distance(img[0], a) < distance(img[0], b)) {
        err = img[0] - a;
        img[0] = a;
    } else {
        err = img[0] - b;
        img[0] = b;
    }
    img[1] += err * 0.4375f;
    img[2] += err * 0.3125f;
    img[3] += err * 0.0625f;
    /* 1, 0 */
    if (distance(img[1], a) < distance(img[1], b)) {
        err = img[1] - a;
        img[1] = a;
    } else {
        err = img[1] - b;
        img[1] = b;
    }
    img[2] += err * 0.1875f;
    img[3] += err * 0.3125f;
    /* 0, 1 */
    if (distance(img[2], a) < distance(img[2], b)) {
        err = img[2] - a;
        img[2] = a;
    } else {
        err = img[2] - b;
        img[2] = b;
    }
    img[3] += err * 0.4375f;
    img[4] += err * 0.3125f;
    img[5] += err * 0.0625f;
    /* 1, 1 */
    if (distance(img[3], a) < distance(img[3], b)) {
        err = img[3] - a;
        img[3] = a;
    } else {
        err = img[3] - b;
        img[3] = b;
    }
    img[4] += err * 0.1875f;
    img[5] += err * 0.3125f;
    /* 0, 2 */
    if (distance(img[4], a) < distance(img[4], b)) {
        err = img[4] - a;
        img[4] = a;
    } else {
        err = img[4] - b;
        img[4] = b;
    }
    img[5] += err * 0.4375f;
    /* 1, 2 */
    if (distance(img[5], a) < distance(img[5], b)) {
        err = img[5] - a;
        img[5] = a;
    } else {
        err = img[5] - b;
        img[5] = b;
    }
    /* generate image */
    if (img[0].x == a.x && img[0].y == a.y && img[0].z == a.z) {out[0] = ac;} else {out[0] = bc;}
    if (img[1].x == a.x && img[1].y == a.y && img[1].z == a.z) {out[1] = ac;} else {out[1] = bc;}
    if (img[2].x == a.x && img[2].y == a.y && img[2].z == a.z) {out[2] = ac;} else {out[2] = bc;}
    if (img[3].x == a.x && img[3].y == a.y && img[3].z == a.z) {out[3] = ac;} else {out[3] = bc;}
    if (img[4].x == a.x && img[4].y == a.y && img[4].z == a.z) {out[4] = ac;} else {out[4] = bc;}
    if (img[5].x == a.x && img[5].y == a.y && img[5].z == a.z) {out[5] = ac;} else {out[5] = bc;}
}

/* character: high byte = bg << 4 | fg, low byte = char */
KERNEL(toCCPixel, PTR(storage, uchar), colors, PTR(storage, uchar), character, PTR(storage, uchar), color, PTR(uniform, uchar), palette, ulong, size) {
    ARRAY(uchar, used_colors, 6);
    VAR(int, n_used_colors) = 0;
    VAR(int, i);
    VAR(int, j);
    VAR(int, tmp);
    VAR(int, maxComponent);
    VAR(uchar, ch) = 128;
    VAR(uchar, fg);
    VAR(uchar, bg);
    VAR(bool, found) = false;
    VAR(uchar2, red);
    VAR(uchar2, green);
    VAR(uchar2, blue);
    ARRAY(float, color_distances, 3);
    ARRAY(uchar, color_map, 16);
    ARRAY(uchar, b, 3);
    ARRAY(int, sums, 3);
    ARRAY(float3, palf, 4);
    ARRAY(float3, dither_in, 6);
    ARRAY(uchar, dither_out, 6);
    if (get_global_id(0) * 6 >= size) {return;}
    colors += get_global_id(0) * 6;
    character += get_global_id(0);
    color += get_global_id(0);
    for (i = 0; i < 6; i++) {
        found = false;
        for (j = 0; j < n_used_colors; j++) {
            if (used_colors[j] == colors[i]) {
                found = true;
                break;
            }
        }
        if (!found) {used_colors[n_used_colors] = colors[i]; n_used_colors++;}
    }
    switch (n_used_colors) {
    case 1: {
        *character = 0x20;
        *color = used_colors[0] << 4;
        break;
    } case 2: {
        fg = used_colors[1]; bg = used_colors[0];
        for (i = 0; i < 5; i++) {if (colors[i] == fg) {ch = ch | (1 << i);}}
        if (colors[5] == fg) {ch = (~ch & 0x1F) | 128; fg = bg; bg = used_colors[1];}
        *character = ch;
        *color = fg | (bg << 4);
        break;
    } case 3: {
        sums[0] = palette[(used_colors[0])*3+0] + palette[(used_colors[0])*3+1] + palette[(used_colors[0])*3+2];
        sums[1] = palette[(used_colors[1])*3+0] + palette[(used_colors[1])*3+1] + palette[(used_colors[1])*3+2];
        sums[2] = palette[(used_colors[2])*3+0] + palette[(used_colors[2])*3+1] + palette[(used_colors[2])*3+2];
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
        palf[0].x = palette[(used_colors[0])*3+0]; palf[0].y = palette[(used_colors[0])*3+1]; palf[0].z = palette[(used_colors[0])*3+2];
        palf[1].x = palette[(used_colors[1])*3+0]; palf[1].y = palette[(used_colors[1])*3+1]; palf[1].z = palette[(used_colors[1])*3+2];
        palf[2].x = palette[(used_colors[2])*3+0]; palf[2].y = palette[(used_colors[2])*3+1]; palf[2].z = palette[(used_colors[2])*3+2];
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
            if ((palette[(used_colors[0])*3+0] + palette[(used_colors[0])*3+1] + palette[(used_colors[0])*3+2]) < 32) {
                color_map[used_colors[0]] = used_colors[1];
                color_map[used_colors[1]] = used_colors[1];
                color_map[used_colors[2]] = used_colors[2];
                fg = used_colors[1]; bg = used_colors[2];
            } else if ((palette[(used_colors[2])*3+0] + palette[(used_colors[2])*3+1] + palette[(used_colors[2])*3+2]) >= 224) {
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
        for (i = 0; i < 5; i++) {if (color_map[colors[i]] == fg) {ch = ch | (1 << i);}}
        if (color_map[colors[5]] == fg) {ch = (~ch & 0x1F) | 128; fg = bg; bg = used_colors[1];}
        *character = ch;
        *color = fg | (bg << 4);
        break;
    } case 4: {
        /* find the colors that are used twice (there are exactly one or two) */
        for (i = 0; i < 4; i++) {color_map[used_colors[i]] = 0;}
        fg = 0xFF; bg = 0xFF;
        for (i = 0; i < 6; i++) {
            color_map[colors[i]]++;
            if (color_map[colors[i]] == 2) {
                if (fg == 0xFF) {fg = colors[i];}
                else {bg = colors[i];}
            }
        }
        color_map[fg] = fg;
        /* if there's only one reused color, take the middle color of the other three for the background */
        if (bg == 0xFF) {
            tmp = 0;
            for (i = 0; i < 4; i++) {if (used_colors[i] != fg) {b[tmp] = used_colors[i]; tmp++;}}
            sums[0] = palette[(b[0])*3+0] + palette[(b[0])*3+1] + palette[(b[0])*3+2];
            sums[1] = palette[(b[1])*3+0] + palette[(b[1])*3+1] + palette[(b[1])*3+2];
            sums[2] = palette[(b[2])*3+0] + palette[(b[2])*3+1] + palette[(b[2])*3+2];
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
        b[0] = 0xFF; b[1] = 0xFF;
        found = false;
        for (i = 0; i < 4; i++) {
            if (used_colors[i] != fg && used_colors[i] != bg) {
                if (!found) {
                    found = true;
                    b[0] = used_colors[i];
                    palf[0].x = palette[(used_colors[i])*3+0]; palf[0].y = palette[(used_colors[i])*3+1]; palf[0].z = palette[(used_colors[i])*3+2];
                } else {
                    b[1] = used_colors[i];
                    palf[1].x = palette[(used_colors[i])*3+0]; palf[1].y = palette[(used_colors[i])*3+1]; palf[1].z = palette[(used_colors[i])*3+2];
                }
            }
        }
        palf[2].x = palette[(fg)*3+0]; palf[2].y = palette[(fg)*3+1]; palf[2].z = palette[(fg)*3+2];
        palf[3].x = palette[(bg)*3+0]; palf[3].y = palette[(bg)*3+1]; palf[3].z = palette[(bg)*3+2];
        if (distance(palf[0], palf[2]) < distance(palf[0], palf[3])) {color_map[b[0]] = fg;}
        else {color_map[b[0]] = bg;}
        if (distance(palf[1], palf[2]) < distance(palf[1], palf[3])) {color_map[b[1]] = fg;}
        else {color_map[b[1]] = bg;}
        for (i = 0; i < 5; i++) {if (color_map[colors[i]] == fg) {ch = ch | (1 << i);}}
        if (color_map[colors[5]] == fg) {ch = (~ch & 0x1F) | 128; tmp = fg; fg = bg; bg = tmp;}
        *character = ch;
        *color = fg | (bg << 4);
        break;
    } default: {
        /* Fall back on median cut */
        red.x = 255; green.x = 255; blue.x = 255;
        red.y = 0; green.y = 0; blue.y = 0;
        for (i = 0; i < n_used_colors; i++) {
            if (palette[(used_colors[i])*3+0] < red.x) {red.x = palette[(used_colors[i])*3+0];}
            if (palette[(used_colors[i])*3+0] > red.y) {red.y = palette[(used_colors[i])*3+0];}
            if (palette[(used_colors[i])*3+1] < green.x) {green.x = palette[(used_colors[i])*3+1];}
            if (palette[(used_colors[i])*3+1] > green.y) {green.y = palette[(used_colors[i])*3+1];}
            if (palette[(used_colors[i])*3+2] < blue.x) {blue.x = palette[(used_colors[i])*3+2];}
            if (palette[(used_colors[i])*3+2] > blue.y) {blue.y = palette[(used_colors[i])*3+2];}
        }
        sums[0] = red.y - red.x; sums[1] = green.y - green.x; sums[2] = blue.y - blue.x;
        if (sums[0] > sums[1] && sums[0] > sums[2]) {maxComponent = 0;}
        else if (sums[1] > sums[2] && sums[1] > sums[0]) {maxComponent = 1;}
        else {maxComponent = 2;}
        /* insertion sort */
        for (i = 0; i < 6; i++) {dither_out[i] = colors[i];}
        for (i = 0; i < 6; i++) {
            tmp = dither_out[i];
            fg = palette[tmp*3+maxComponent];
            for (j = i; j > 0 && palette[dither_out[j-1]*3+maxComponent] > fg; j--) {
                dither_out[j] = dither_out[j-1];
            }
            dither_out[j] = tmp;
        }
        for (i = 0; i < 6; i++) {
            if (i < 3) {color_map[i] = dither_out[i];}
            else {b[i - 3] = dither_out[i];}
        }
        dither_in[0].x = palette[(colors[0])*3+0]; dither_in[0].y = palette[(colors[0])*3+1]; dither_in[0].z = palette[(colors[0])*3+2];
        dither_in[1].x = palette[(colors[1])*3+0]; dither_in[1].y = palette[(colors[1])*3+1]; dither_in[1].z = palette[(colors[1])*3+2];
        dither_in[2].x = palette[(colors[2])*3+0]; dither_in[2].y = palette[(colors[2])*3+1]; dither_in[2].z = palette[(colors[2])*3+2];
        dither_in[3].x = palette[(colors[3])*3+0]; dither_in[3].y = palette[(colors[3])*3+1]; dither_in[3].z = palette[(colors[3])*3+2];
        dither_in[4].x = palette[(colors[4])*3+0]; dither_in[4].y = palette[(colors[4])*3+1]; dither_in[4].z = palette[(colors[4])*3+2];
        dither_in[5].x = palette[(colors[5])*3+0]; dither_in[5].y = palette[(colors[5])*3+1]; dither_in[5].z = palette[(colors[5])*3+2];
        palf[0].x = palette[(color_map[2])*3+0]; palf[0].y = palette[(color_map[2])*3+1]; palf[0].z = palette[(color_map[2])*3+2];
        palf[1].x = palette[(b[2])*3+0]; palf[1].y = palette[(b[2])*3+1]; palf[1].z = palette[(b[2])*3+2];
        ditherCCImage(dither_in, palf[0], palf[1], color_map[2], b[2], dither_out);
        fg = color_map[2]; bg = b[2];
        for (i = 0; i < 5; i++) {if (dither_out[i] == fg) {ch = ch | (1 << i);}}
        if (dither_out[5] == fg) {ch = (~ch & 0x1F) | 128; fg = bg; bg = color_map[2];}
        *character = ch;
        *color = fg | (bg << 4);
        break;
    }
    }
}

KERNEL(toNFPPixel, PTR(storage, uchar), colors, PTR(storage, uchar), bg, ulong, size) {
    ARRAY(uchar, color_counts, 16) = ARRAYDEF(uchar, 16, 0);
    VAR(uchar, max_color) = 0;
    VAR(uchar, max_count) = 0;
    VAR(uchar, i); VAR(uchar, c);
    if (get_global_id(0) * 6 >= size) {return;}
    colors += get_global_id(0) * 6;
    bg += get_global_id(0);
    for (i = 0; i < 6; i++) {
        c = colors[i];
        color_counts[c]++;
        if (color_counts[c] > max_count) {
            max_color = c;
            max_count = color_counts[c];
        }
    }
    *bg = max_color << 4;
}

KERNEL(toLab, PTR(storage, uchar), image, PTR(storage, uchar), output, ulong, size) {
    VAR(float, r); VAR(float, g); VAR(float, b); VAR(float, X); VAR(float, Y); VAR(float, Z); VAR(float, L); VAR(float, a); VAR(float, B);
    if (get_global_id(0) >= size) {return;}
    r = image[get_global_id(0)*3] / 255.0f; g = image[get_global_id(0)*3+1] / 255.0f; b = image[get_global_id(0)*3+2] / 255.0f;
    if (r > 0.04045f) {r = pow((r + 0.055f) / 1.055f, 2.4f);}
    else {r = r / 12.92f;}
    if (g > 0.04045f) {g = pow((g + 0.055f) / 1.055f, 2.4f);}
    else {g = g / 12.92f;}
    if (b > 0.04045f) {b = pow((b + 0.055f) / 1.055f, 2.4f);}
    else {b = b / 12.92f;}
    r = r * 100; g = g * 100; b = b * 100;
    X = (r * 0.4124f + g * 0.3576f + b * 0.1805f) / 95.047f;
    Y = (r * 0.2126f + g * 0.7152f + b * 0.0722f) / 100.000f;
    Z = (r * 0.0193f + g * 0.1192f + b * 0.9505f) / 108.883f;
    if (X > 0.008856f) {X = cbrt(X);}
    else {X = (7.787f * X) + (16.0f / 116.0f);}
    if (Y > 0.008856f) {Y = cbrt(Y);}
    else {Y = (7.787f * Y) + (16.0f / 116.0f);}
    if (Z > 0.008856f) {Z = cbrt(Z);}
    else {Z = (7.787f * Z) + (16.0f / 116.0f);}
    L = (116 * Y) - 16; a = 500 * (X - Y) + 128; B = 200 * (Y - Z) + 128;
    output[get_global_id(0)*3] = L; output[get_global_id(0)*3+1] = a; output[get_global_id(0)*3+2] = B;
}

FUNC(float3, closestPixel, float3, pix, PTR(uniform, uchar), palette, uchar, palette_size) {
    VAR(uchar, i);
    VAR(float3, closest); VAR(float3, col);
    VAR(float, dist);
    VAR(uchar, index) = 0;
    closest.x = palette[0]; closest.y = palette[1]; closest.z = palette[2];
    dist = distance(pix, closest);
    for (i = 1; i < palette_size; i++) {
        col.x = palette[i*3]; col.y = palette[i*3+1]; col.z = palette[i*3+2];
        if (distance(pix, col) < dist) {
            closest = col;
            dist = distance(pix, col);
            index = i;
        }
    }
    //if (palette_index != NULL) {*palette_index = index;}
    return closest;
}

FUNC(float3, closestPixelA, float3, pix, PTR(uniform, uchar), palette, uchar, palette_size, PTR(storage, uchar), palette_index) {
    VAR(uchar, i);
    VAR(float3, closest); VAR(float3, col);
    VAR(float, dist);
    VAR(uchar, index) = 0;
    closest.x = palette[0]; closest.y = palette[1]; closest.z = palette[2];
    dist = distance(pix, closest);
    for (i = 1; i < palette_size; i++) {
        col.x = palette[i*3]; col.y = palette[i*3+1]; col.z = palette[i*3+2];
        if (distance(pix, col) < dist) {
            closest = col;
            dist = distance(pix, col);
            index = i;
        }
    }
    *palette_index = index;
    return closest;
}

KERNEL(thresholdKernel, PTR(storage, uchar), image, PTR(storage, uchar), output, PTR(uniform, uchar), palette, uchar, palette_size) {
    VAR(float3, pix); VAR(float3, closest);
    pix.x = image[get_global_id(0)*3]; pix.y = image[get_global_id(0)*3+1]; pix.z = image[get_global_id(0)*3+2];
    closest = closestPixel(pix, palette, palette_size);
    output[get_global_id(0)*3] = closest.x; output[get_global_id(0)*3+1] = closest.y; output[get_global_id(0)*3+2] = closest.z;
}

// Adapted from https://community.arm.com/arm-community-blogs/b/graphics-gaming-and-vr-blog/posts/when-parallelism-gets-tricky-accelerating-floyd-steinberg-on-the-mali-gpu
KERNEL(floydSteinbergDither,
    PTR(storage, uchar), image,
    PTR(storage, uchar), output,
    PTR(uniform, uchar), palette,
    uchar, palette_size,
    PTR(storage, float), error,
    PTR(storage, uint), workgroup_rider,
    PTR(storage, volatile uint), workgroup_progress,
    PTR(workgroup, volatile uint), progress,
    ulong, width, ulong, height
) {
    LOCALVAR(uint, workgroup_number);
    VAR(uint, x);
    VAR(float3, pix); VAR(float3, closest); VAR(float3, err);
    VAR(uint, id);
    VAR(ulong, yoff);

    if (get_local_id(0) == 0) {
        workgroup_number = atomic_inc(workgroup_rider);
        for (VAR(int, i) = 0; i < get_local_size(0); i++) {progress[i] = 0;}
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    id = workgroup_number * get_local_size(0) + get_local_id(0);
    yoff = id*width;
    for (x = 0; x < width;) {
        if (TERNARY(get_local_id(0) == 0, workgroup_number == 0 || workgroup_progress[workgroup_number-1] >= x + 2, progress[get_local_id(0)-1] >= x + 2)) {
            if (id < height) {
                pix.x = image[(yoff+x)*3]; pix.y = image[(yoff+x)*3+1]; pix.z = image[(yoff+x)*3+2];
                pix += vload3(yoff + x, error);
                closest = closestPixel(pix, palette, palette_size);
                output[(yoff+x)*3] = closest.x; output[(yoff+x)*3+1] = closest.y; output[(yoff+x)*3+2] = closest.z;
                err = pix - closest;
                if (x < width - 1) {
                    vstore3(vload3(yoff + x + 1, error) + (err * 0.3125f), yoff + x + 1, error);
                    vstore3((err * 0.0625f), yoff + width + x + 1, error);
                }
                if (x > 0) {vstore3(vload3(yoff + width + x - 1, error) + (err * 0.125f), yoff + width + x - 1, error);}
                vstore3(vload3(yoff + width + x, error) + (err * 0.1875f), yoff + width + x, error);
            }
            if (get_local_id(0) == get_local_size(0) - 1) {workgroup_progress[workgroup_number] = x;}
            else {progress[get_local_id(0)] = x;}
            x++;
        }
    }
    if (get_local_id(0) == get_local_size(0) - 1) {workgroup_progress[workgroup_number] = width + 2;}
    else {progress[get_local_id(0)] = width + 2;}
}

CONSTANTARRAY2(int, thresholdMap, 8, 8) = ARRAYDEF(??(array<int, 8>??), 8,
    ARRAYDEF(int, 8,  0, 32,  8, 40,  2, 34, 10, 42),
    ARRAYDEF(int, 8, 48, 16, 56, 24, 50, 18, 58, 26),
    ARRAYDEF(int, 8, 12, 44,  4, 36, 14, 46,  6, 38),
    ARRAYDEF(int, 8, 60, 28, 52, 20, 62, 30, 54, 22),
    ARRAYDEF(int, 8,  3, 35, 11, 43,  1, 33,  9, 41),
    ARRAYDEF(int, 8, 51, 19, 59, 27, 49, 17, 57, 25),
    ARRAYDEF(int, 8, 15, 47,  7, 39, 13, 45,  5, 37),
    ARRAYDEF(int, 8, 63, 31, 55, 23, 61, 29, 53, 21)
);

KERNEL(orderedDither, PTR(storage, uchar), image, PTR(storage, uchar), output, PTR(uniform, uchar), palette, uchar, palette_size, ulong, width, float, factor) {
    VAR(float3, pix); VAR(float3, closest);
    pix.x = image[get_global_id(0)*3]; pix.y = image[get_global_id(0)*3+1]; pix.z = image[get_global_id(0)*3+2];
    pix += factor * (thresholdMap[(get_global_id(0) / width) % 8][(get_global_id(0) % width) % 8] / 64.0f - 0.5f);
    closest = closestPixel(pix, palette, palette_size);
    output[get_global_id(0)*3] = closest.x; output[get_global_id(0)*3+1] = closest.y; output[get_global_id(0)*3+2] = closest.z;
}

KERNEL(rgbToPaletteKernel, PTR(storage, uchar), image, PTR(storage, uchar), output, PTR(storage, uchar), palette, uchar, palette_size, ulong, size) {
    VAR(uchar, i);
    if (get_global_id(0) >= size) {return;}
    for (i = 0; i < palette_size; i++) {
        if (image[get_global_id(0)*3] == palette[i*3] && image[get_global_id(0)*3+1] == palette[i*3+1] && image[get_global_id(0)*3+2] == palette[i*3+2]) {
            output[get_global_id(0)] = i;
            return;
        }
    }
}

KERNEL(copyColors, PTR(storage, uchar), input, PTR(storage, uchar), colors, ulong, width, ulong, height) {
    VAR(ulong, y) = get_global_id(0) * 2 / width;
    VAR(ulong, x) = get_global_id(0) * 2 % width;
    if (y >= height) {return;}
    colors[(y-y%3)*width + x*3 + (y%3)*2] = input[y*width+x];
    colors[(y-y%3)*width + x*3 + (y%3)*2 + 1] = TERNARY(x == width - 1, 15, input[y*width+x+1]);
}

KERNEL(calculateRange_A, PTR(storage, uchar), input, PTR(storage, uchar), ranges, ulong, n, ulong, offset) {
    VAR(ulong, i);
    VAR(uchar, minr) = 255; VAR(uchar, ming) = 255; VAR(uchar, minb) = 255; VAR(uchar, maxr) = 0; VAR(uchar, maxg) = 0; VAR(uchar, maxb) = 0; VAR(uchar, r); VAR(uchar, g); VAR(uchar, b);
    VAR(int, id) = get_global_id(0);
    if (id * 128 >= n) {return;}
    input += offset * 3;
    for (i = id * 128; i < (id + 1) * 128 && i < n; i++) {
        r = input[i*3]; g = input[i*3+1]; b = input[i*3+2];
        if (r < minr) {minr = r;}
        if (g < ming) {ming = g;}
        if (b < minb) {minb = b;}
        if (r > maxr) {maxr = r;}
        if (g > maxg) {maxg = g;}
        if (b > maxb) {maxb = b;}
    }
    ranges[id*6] = minr;
    ranges[id*6+1] = ming;
    ranges[id*6+2] = minb;
    ranges[id*6+3] = maxr;
    ranges[id*6+4] = maxg;
    ranges[id*6+5] = maxb;
}

KERNEL(calculateRange_B, PTR(storage, uchar), input, PTR(storage, uchar), components, ulong, n, uchar, id) {
    VAR(ulong, i);
    VAR(uchar, minr) = 255;
    VAR(uchar, ming) = 255;
    VAR(uchar, minb) = 255;
    VAR(uchar, maxr) = 0;
    VAR(uchar, maxg) = 0;
    VAR(uchar, maxb) = 0;
    ARRAY(uchar, ranges, 3);
    VAR(uchar, maxComponent);
    for (i = 0; i < n; i++) {
        if (input[i*6] < minr) {minr = input[i*6];}
        if (input[i*6+1] < ming) {ming = input[i*6+1];}
        if (input[i*6+2] < minb) {minb = input[i*6+2];}
        if (input[i*6+3] > maxr) {maxr = input[i*6+3];}
        if (input[i*6+4] > maxg) {maxg = input[i*6+4];}
        if (input[i*6+5] > maxb) {maxb = input[i*6+5];}
    }
    ranges[0] = maxr - minr;
    ranges[1] = maxg - ming;
    ranges[2] = maxb - minb;
    if (ranges[0] > ranges[1] && ranges[0] > ranges[2]) {maxComponent = 0;}
    else if (ranges[1] > ranges[0] && ranges[1] > ranges[2]) {maxComponent = 1;}
    else {maxComponent = 2;}
    if (maxComponent == components[id >> 1]) {
        if (abs(CAST(int, ranges[maxComponent]) - CAST(int, ranges[(maxComponent+1)%3])) < 8 && abs(CAST(int, ranges[maxComponent]) - CAST(int, ranges[(maxComponent+2)%3])) < 8)
            {maxComponent = TERNARY(ranges[(maxComponent+1)%3] > ranges[(maxComponent+2)%3], (maxComponent + 1) % 3, (maxComponent + 2) % 3);}
        else if (abs(CAST(int, ranges[maxComponent]) - CAST(int, ranges[(maxComponent+1)%3])) < 8) {maxComponent = (maxComponent + 1) % 3;}
        else if (abs(CAST(int, ranges[maxComponent]) - CAST(int, ranges[(maxComponent+2)%3])) < 8) {maxComponent = (maxComponent + 2) % 3;}
    }
    components[id] = maxComponent;
}

KERNEL(diffuseKernel, PTR(storage, uchar), buffer, ulong, offset, ulong, size, float, step) {
    if (offset + get_global_id(0) >= size) {return;}
    vstore3(vload3(get_global_id(0) * step, buffer), offset + get_global_id(0), buffer);
}

KERNEL(averageKernel_A, PTR(storage, uchar), src, PTR(storage, uint), result, ulong, offset, ulong, length) {
    VAR(uint, r) = 0;
    VAR(uint, g) = 0;
    VAR(uint, b) = 0;
    VAR(int, i);
    VAR(int, id) = get_global_id(0);
    if (id * 128 >= length) {return;}
    src += offset * 3;
    for (i = id * 128; i < (id + 1) * 128 && i < length; i++) {
        r += src[i*3]; g += src[i*3+1]; b += src[i*3+2];
    }
    result[id*3] = r; result[id*3+1] = g; result[id*3+2] = b;
}

KERNEL(averageKernel_B, PTR(storage, uint), src, PTR(storage, uchar), result, ulong, offset, ulong, length) {
    VAR(uint, r) = 0;
    VAR(uint, g) = 0;
    VAR(uint, b) = 0;
    VAR(int, i);
    for (i = 0; i < length / 128 + TERNARY(length % 128, 1, 0); i++) {
        r += src[i*3]; g += src[i*3+1]; b += src[i*3+2];
    }
    result[offset*3] = r / length; result[offset*3+1] = g / length; result[offset*3+2] = b / length;
}

KERNEL(kMeans_bucket_kernel, PTR(storage, uchar), image, PTR(storage, uchar), buckets, PTR(uniform, uchar), palette, ulong, palette_size) {
    VAR(float3, pix);
    pix.x = image[get_global_id(0)*3]; pix.y = image[get_global_id(0)*3+1]; pix.z = image[get_global_id(0)*3+2];
    closestPixelA(pix, palette, palette_size, buckets + get_global_id(0));
}

KERNEL(kMeans_recenter_kernel_A, PTR(storage, uchar), image, PTR(storage, uchar), buckets, PTR(storage, uint), output, ulong, length, PTR(workgroup, uchar), buckets_aux) {
    VAR(uint, id) = get_group_id(0);
    VAR(uint, color) = get_local_id(0);
    VAR(uint, numColors) = get_local_size(0);
    VAR(uint, r) = 0;
    VAR(uint, g) = 0;
    VAR(uint, b) = 0;
    VAR(uint, total) = 0;
    VAR(ulong, offset) = id * 128;
    VAR(int, i);
    barrier(CLK_LOCAL_MEM_FENCE);
    for (i = 0; i < 128; i++) {buckets_aux[i] = buckets[offset+i];}
    barrier(CLK_LOCAL_MEM_FENCE);
    for (i = 0; i < 128 && i + offset < length; i++) {
        if (buckets_aux[i] == color) {
            r += image[(offset+i)*3]; g += image[(offset+i)*3+1]; b += image[(offset+i)*3+2]; total++;
        }
    }
    output[id*numColors*4+color*4] = r;
    output[id*numColors*4+color*4+1] = g;
    output[id*numColors*4+color*4+2] = b;
    output[id*numColors*4+color*4+3] = total;
}

KERNEL(kMeans_recenter_kernel_B, PTR(storage, uint), src, PTR(storage, uchar), palette, ulong, nparts, PTR(storage, uchar), changed) {
    VAR(uint, color) = get_global_id(0);
    VAR(uint, numColors) = get_global_size(0);
    VAR(uint, r) = 0;
    VAR(uint, g) = 0;
    VAR(uint, b) = 0;
    VAR(uint, total) = 0;
    VAR(uchar, o_r); VAR(uchar, o_g); VAR(uchar, o_b); VAR(uchar, n_r); VAR(uchar, n_g); VAR(uchar, n_b);
    VAR(int, i);
    for (i = 0; i < nparts; i++) {
        r += src[i*numColors*4+color*4];
        g += src[i*numColors*4+color*4+1];
        b += src[i*numColors*4+color*4+2];
        total += src[i*numColors*4+color*4+3];
    }
    o_r = palette[color*3];
    n_r = r / total;
    palette[color*3] = n_r;
    o_g = palette[color*3+1];
    n_g = g / total;
    palette[color*3+1] = n_g;
    o_b = palette[color*3+2];
    n_b = b / total;
    palette[color*3+2] = n_b;
    if (o_r != n_r || o_g != n_g || o_b != n_b) {*changed = 1;}
}

/* Sorting code from Eric Bainville, BSD-style license */
/* Retrieved from http://www.bealto.com/gpu-sorting_parallel-bitonic-2.html */

#define data_t uchar3

FUNC(uchar, getKey, uchar3, v, uchar, n) {
    switch (n) {
        case 0: {return v.x;}
        case 1: {return v.y;}
        case 2: {return v.z;}
        default: {return 0;}
    }
}

#define ORDER(a,b,c) { VAR(bool, swap) = reverse ^ (getKey(a,c)<getKey(b,c)); VAR(data_t, auxa) = a; VAR(data_t, auxb) = b; a = TERNARY(swap,auxb,auxa); b = TERNARY(swap,auxa,auxb); }

// N/2 threads
KERNEL(ParallelBitonic_B2, PTR(storage, uchar), data, int, inc, int, dir, PTR(uniform, uchar), components, uchar, compid, ulong, offset) {
    VAR(int, t) = get_global_id(0);        // thread index
    VAR(int, low) = t & (inc - 1);         // low order bits (below INC)
    VAR(int, i) = (t << 1) - low;          // insert 0 at position INC
    VAR(bool, reverse) = ((dir & i) == 0); // asc/desc order
    data += (i + offset) * 3;        // translate to first value

    // Load
    VAR(data_t, x0) = vload3(0, data);
    VAR(data_t, x1) = vload3(inc, data);

    // Sort
    ORDER(x0, x1, components[compid])

    // Store
    vstore3(x0, 0, data);
    vstore3(x1, inc, data);
}

// N/4 threads
KERNEL(ParallelBitonic_B4, PTR(storage, uchar), data, int, inc, int, dir, PTR(uniform, uchar), components, uchar, compid, ulong, offset) {
    inc >>= 1;
    VAR(int, t) = get_global_id(0);        // thread index
    VAR(int, low) = t & (inc - 1);         // low order bits (below INC)
    VAR(int, i) = ((t - low) << 2) + low;  // insert 00 at position INC
    VAR(bool, reverse) = ((dir & i) == 0); // asc/desc order
    data += (i + offset) * 3;        // translate to first value

    // Load
    VAR(data_t, x0) = vload3(0, data);
    VAR(data_t, x1) = vload3(inc, data);
    VAR(data_t, x2) = vload3(2 * inc, data);
    VAR(data_t, x3) = vload3(3 * inc, data);

    // Sort
    ORDER(x0, x2, components[compid])
    ORDER(x1, x3, components[compid])
    ORDER(x0, x1, components[compid])
    ORDER(x2, x3, components[compid])

    // Store
    vstore3(x0, 0, data);
    vstore3(x1, inc, data);
    vstore3(x2, 2 * inc, data);
    vstore3(x3, 3 * inc, data);
}

#define ORDERV(x,a,b,c) { VAR(bool, swap) = reverse ^ (getKey(x[a],c)<getKey(x[b],c)); \
      VAR(data_t, auxa) = x[a]; VAR(data_t, auxb) = x[b]; \
      x[a] = TERNARY(swap,auxb,auxa); x[b] = TERNARY(swap,auxa,auxb); }
#define B2V(x,a,c) { ORDERV(x,a,a+1,c) }
#define B4V(x,a,c) { for (VAR(int, i4)=0;i4<2;i4++) { ORDERV(x,a+i4,a+i4+2,c) } B2V(x,a,c) B2V(x,a+2,c) }
#define B8V(x,a,c) { for (VAR(int, i8)=0;i8<4;i8++) { ORDERV(x,a+i8,a+i8+4,c) } B4V(x,a,c) B4V(x,a+4,c) }
#define B16V(x,a,c) { for (VAR(int, i16)=0;i16<8;i16++) { ORDERV(x,a+i16,a+i16+8,c) } B8V(x,a,c) B8V(x,a+8,c) }

// N/8 threads
KERNEL(ParallelBitonic_B8, PTR(storage, uchar), data, int, inc, int, dir, PTR(uniform, uchar), components, uchar, compid, ulong, offset) {
    inc >>= 2;
    VAR(int, t) = get_global_id(0);        // thread index
    VAR(int, low) = t & (inc - 1);         // low order bits (below INC)
    VAR(int, i) = ((t - low) << 3) + low;  // insert 000 at position INC
    VAR(bool, reverse) = ((dir & i) == 0); // asc/desc order
    data += (i + offset) * 3;        // translate to first value

    // Load
    ARRAY(data_t, x, 8);
    for (VAR(int, k) = 0; k < 8; k++) {x[k] = vload3(k * inc, data);}

    // Sort
    B8V(x, 0, components[compid])

    // Store
    for (VAR(int, k) = 0; k < 8; k++) {vstore3(x[k], k * inc, data);}
}

// N/16 threads
KERNEL(ParallelBitonic_B16, PTR(storage, uchar), data, int, inc, int, dir, PTR(uniform, uchar), components, uchar, compid, ulong, offset) {
    inc >>= 3;
    VAR(int, t) = get_global_id(0);        // thread index
    VAR(int, low) = t & (inc - 1);         // low order bits (below INC)
    VAR(int, i) = ((t - low) << 4) + low;  // insert 0000 at position INC
    VAR(bool, reverse) = ((dir & i) == 0); // asc/desc order
    data += (i + offset) * 3;        // translate to first value

    // Load
    ARRAY(data_t, x, 16);
    for (VAR(int, k) = 0; k < 16; k++) {x[k] = vload3(k * inc, data);}

    // Sort
    B16V(x, 0, components[compid])

    // Store
    for (VAR(int, k) = 0; k < 16; k++) {vstore3(x[k], k * inc, data);}
}

// N/2 threads, AUX[2*WG]
KERNEL(ParallelBitonic_C2_pre, PTR(storage, uchar), data, int, inc, int, dir, PTR(uniform, uchar), components, uchar, compid, ulong, offset, PTR(workgroup, uchar), aux) {
    VAR(int, t) = get_global_id(0); // thread index
    data += offset * 3;

    // Terminate the INC loop inside the workgroup
    for (; inc > 0; inc >>= 1) {
        VAR(int, low) = t & (inc - 1);         // low order bits (below INC)
        VAR(int, i) = (t << 1) - low;          // insert 0 at position INC
        VAR(bool, reverse) = ((dir & i) == 0); // asc/desc order

        barrier(CLK_GLOBAL_MEM_FENCE);

        // Load
        VAR(data_t, x0) = vload3(i, data);
        VAR(data_t, x1) = vload3(i + inc, data);

        // Sort
        ORDER(x0, x1, components[compid])

        barrier(CLK_GLOBAL_MEM_FENCE);

        // Store
        vstore3(x0, i, data);
        vstore3(x1, i + inc, data);
    }
}

// N/2 threads, AUX[2*WG]
KERNEL(ParallelBitonic_C2, PTR(storage, uchar), data, int, inc0, int, dir, PTR(uniform, uchar), components, uchar, compid, ulong, offset, PTR(workgroup, uchar), aux) {
    VAR(int, t) = get_global_id(0);               // thread index
    VAR(int, wgBits) = 2 * get_local_size(0) - 1; // bit mask to get index in local memory AUX (size is 2*WG)
    data += offset * 3;

    for (VAR(int, inc) = inc0; inc > 0; inc >>= 1) {
        VAR(int, low) = t & (inc - 1);         // low order bits (below INC)
        VAR(int, i) = (t << 1) - low;          // insert 0 at position INC
        VAR(bool, reverse) = ((dir & i) == 0); // asc/desc order
        VAR(data_t, x0);
        VAR(data_t, x1);

        // Load
        if (inc == inc0) {
            // First iteration: load from global memory
            x0 = vload3(i, data);
            x1 = vload3(i + inc, data);
        } else {
            // Other iterations: load from local memory
            barrier(CLK_LOCAL_MEM_FENCE);
            x0 = vload3(i & wgBits, aux);
            x1 = vload3((i + inc) & wgBits, aux);
        }

        // Sort
        ORDER(x0, x1, components[compid])

        // Store
        if (inc == 1) {
            // Last iteration: store to global memory
            vstore3(x0, i, data);
            vstore3(x1, i + inc, data);
        } else {
            // Other iterations: store to local memory
            barrier(CLK_LOCAL_MEM_FENCE);
            vstore3(x0, i & wgBits, aux);
            vstore3(x1, (i + inc) & wgBits, aux);
        }
    }
}

// N/4 threads, AUX[4*WG]
KERNEL(ParallelBitonic_C4_0, PTR(storage, uchar), data, int, inc0, int, dir, PTR(uniform, uchar), components, uchar, compid, ulong, offset, PTR(workgroup, uchar), aux) {
    VAR(int, t) = get_global_id(0);               // thread index
    VAR(int, wgBits) = 4 * get_local_size(0) - 1; // bit mask to get index in local memory AUX (size is 4*WG)
    data += offset * 3;

    for (VAR(int, inc) = inc0 >> 1; inc > 0; inc >>= 2) {
        VAR(int, low) = t & (inc - 1);         // low order bits (below INC)
        VAR(int, i) = ((t - low) << 2) + low;  // insert 00 at position INC
        VAR(bool, reverse) = ((dir & i) == 0); // asc/desc order
        ARRAY(data_t, x, 4);

        // Load
        if (inc == inc0 >> 1) {
            // First iteration: load from global memory
            for (VAR(int, k) = 0; k < 4; k++) {x[k] = vload3(i + k * inc, data);}
        } else {
            // Other iterations: load from local memory
            barrier(CLK_LOCAL_MEM_FENCE);
            for (VAR(int, k) = 0; k < 4; k++) {x[k] = vload3((i + k * inc) & wgBits, aux);}
        }

        // Sort
        B4V(x, 0, components[compid]);

        // Store
        if (inc == 1) {
            // Last iteration: store to global memory
            for (VAR(int, k) = 0; k < 4; k++) {vstore3(x[k], i + k * inc, data);}
        } else {
            // Other iterations: store to local memory
            barrier(CLK_LOCAL_MEM_FENCE);
            for (VAR(int, k) = 0; k < 4; k++) {vstore3(x[k], (i + k * inc) & wgBits, aux);}
        }
    }
}

KERNEL(ParallelBitonic_C4, PTR(storage, uchar), data, int, inc0, int, dir, PTR(uniform, uchar), components, uchar, compid, ulong, offset, PTR(workgroup, uchar), aux) {
    VAR(int, t) = get_global_id(0);               // thread index
    VAR(int, wgBits) = 4 * get_local_size(0) - 1; // bit mask to get index in local memory AUX (size is 4*WG)
    VAR(int, inc);
    VAR(int, low);
    VAR(int, i);
    VAR(bool, reverse);
    ARRAY(data_t, x, 4);

    // First iteration, global input, local output
    data += offset * 3;
    inc = inc0 >> 1;
    low = t & (inc - 1);        // low order bits (below INC)
    i = ((t - low) << 2) + low; // insert 00 at position INC
    reverse = ((dir & i) == 0); // asc/desc order
    for (VAR(int, k) = 0; k < 4; k++) {x[k] = vload3(i + k * inc, data);}
    B4V(x, 0, components[compid]);
    for (VAR(int, k) = 0; k < 4; k++) {vstore3(x[k], (i + k * inc) & wgBits, aux);}
    barrier(CLK_LOCAL_MEM_FENCE);

    // Internal iterations, local input and output
    for (; inc > 1; inc >>= 2) {
        low = t & (inc - 1);        // low order bits (below INC)
        i = ((t - low) << 2) + low; // insert 00 at position INC
        reverse = ((dir & i) == 0); // asc/desc order
        for (VAR(int, k) = 0; k < 4; k++) {x[k] = vload3((i + k * inc) & wgBits, aux);}
        B4V(x, 0, components[compid]);
        barrier(CLK_LOCAL_MEM_FENCE);
        for (VAR(int, k) = 0; k < 4; k++) {vstore3(x[k], (i + k * inc) & wgBits, aux);}
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    // Final iteration, local input, global output, INC=1
    i = t << 2;
    reverse = ((dir & i) == 0); // asc/desc order
    for (VAR(int, k) = 0; k < 4; k++) {x[k] = vload3((i + k) & wgBits, aux);}
    B4V(x, 0, components[compid]);
    for (VAR(int, k) = 0; k < 4; k++) {vstore3(x[k], i + k, data);}
}
