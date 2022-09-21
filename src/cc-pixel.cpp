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

#include "sanjuuni.hpp"

// OpenCL polyfills
#define __global
#define __local
#define __constant constexpr
#define __private
#define __kernel

static float distance(float3 p0, float3 p1) {return sqrt((p1.x - p0.x)*(p1.x - p0.x) + (p1.y - p0.y)*(p1.y - p0.y) + (p1.z - p0.z)*(p1.z - p0.z));}
float3 operator-(const float3& a, const float3& b) {return {a.x - b.x, a.y - b.y, a.z - b.z};}
float3& operator+=(float3& a, const float3& b) {a.x += b.x; a.y += b.y; a.z += b.z; return a;}
float3 operator*(const float3& v, float s) {return {v.x * s, v.y * s, v.z * s};}
int operator==(const float3& a, const float3& b) {return a.x == b.x && a.y == b.y && a.z == b.z;}

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
        red.y = green.y = blue.y = 0;
        for (i = 0; i < n_used_colors; i++) {
            if (palette[used_colors[i]].x < red.x) red.x = palette[used_colors[i]].x;
            if (palette[used_colors[i]].x > red.y) red.y = palette[used_colors[i]].x;
            if (palette[used_colors[i]].y < green.x) green.x = palette[used_colors[i]].y;
            if (palette[used_colors[i]].y > green.y) green.y = palette[used_colors[i]].y;
            if (palette[used_colors[i]].z < blue.x) blue.x = palette[used_colors[i]].z;
            if (palette[used_colors[i]].z > blue.y) blue.y = palette[used_colors[i]].z;
        }
        sums[0] = red.y - red.x; sums[1] = green.y - green.x; sums[2] = blue.y - blue.x;
        if (sums[0] > sums[1] && sums[0] > sums[2]) maxComponent = 0;
        else if (sums[1] > sums[2] && sums[1] > sums[0]) maxComponent = 1;
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
