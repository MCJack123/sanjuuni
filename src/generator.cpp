/*
 * generator.cpp
 * Generates video outputs from quantized ComputerCraft frames.
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
#include <cstring>
#include <algorithm>
#include <sstream>
#include <stack>

#ifndef NO_POCO
#include <Poco/Base64Encoder.h>
#include <Poco/Checksum.h>
#else
static const char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const uint32_t crc32[] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
    0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
    0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
    0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
    0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
    0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,
    0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
    0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
    0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
    0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,
    0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
    0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
    0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
    0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
    0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,
    0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
    0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
    0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
    0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
    0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
    0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
    0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};
namespace Poco {
    class Base64Encoder {
        std::ostream& out;
    public:
        Base64Encoder(std::ostream& out): out(out) {}
        void write(const char * str, size_t size) {
            const unsigned char * ustr = (const unsigned char*)str;
            size_t pos = 0;
            for (size_t pos = 0; pos + 2 < size; pos += 3) {
                out.put(base64[ustr[pos] >> 2]);
                out.put(base64[(ustr[pos] & 0x03) << 4 | (ustr[pos+1] & 0xF0) >> 4]);
                out.put(base64[(ustr[pos+1] & 0x0F) << 2 | (ustr[pos+2] & 0xA0) >> 6]);
                out.put(base64[ustr[pos+2] & 0x3F]);
            }
            switch (size % 3) {
                case 0: break;
                case 1:
                    out.put(base64[ustr[size-1] >> 2]);
                    out.put(base64[(ustr[size-1] & 0x03) << 4]);
                    out.put('=');
                    out.put('=');
                    break;
                case 2:
                    out.put(base64[ustr[size-2] >> 2]);
                    out.put(base64[(ustr[size-2] & 0x03) << 4 | (ustr[size-1] & 0xF0) >> 4]);
                    out.put(base64[(ustr[size-1] & 0x0F) << 2]);
                    out.put('=');
                    break;
            }
        }
        void close() {}
    };
    class Checksum {
        uint32_t sum = 0xFFFFFFFF;
    public:
        void update(const std::string& str) {
            for (unsigned char c : str)
                sum = crc32[(uint32_t)c ^ (sum & 0xFF)] ^ (sum >> 8);
        }
        uint32_t checksum() {return ~sum;}
    };
}
#endif

static const char * hexstr = "0123456789abcdef";

struct makeCCImage_state {
    int i;
    uchar* colors;
    uchar** chars;
    uchar** cols;
    uchar * pal;
    ulong size;
};

static void makeCCImage_worker(void* s) {
    makeCCImage_state * state = (makeCCImage_state*)s;
    toCCPixel(state->colors + (state->i * 6), *state->chars + state->i, *state->cols + state->i, state->pal, state->size);
    delete state;
}

void makeCCImage(Mat1b& input, const std::vector<Vec3b>& palette, uchar** chars, uchar** cols, OpenCL::Device * device) {
    int width = input.width - input.width % 2, height = input.height - input.height % 3;
    uchar pal[48];
    for (int i = 0; i < palette.size() && i < 16; i++) {pal[i*3] = palette[i][0]; pal[i*3+1] = palette[i][1]; pal[i*3+2] = palette[i][2];}
#ifdef HAS_OPENCL
    if (device != NULL) {
        *chars = new uchar[(height / 3) * (width / 2)];
        *cols = new uchar[(height / 3) * (width / 2)];
        input.upload();
        try {
            OpenCL::Memory<uchar> colors_mem(*device, height * width / 6, 6, false, true);
            OpenCL::Memory<uchar> chars_mem(*device, (height / 3) * (width / 2), 1, *chars);
            OpenCL::Memory<uchar> cols_mem(*device, (height / 3) * (width / 2), 1, *cols);
            OpenCL::Memory<uchar> palette_mem(*device, palette.size(), 3, pal);
            OpenCL::Kernel copykernel(*device, height * width / 2, "copyColors", *input.mem, colors_mem, (ulong)width, (ulong)height);
            OpenCL::Kernel kernel(*device, height * width / 6, "toCCPixel", colors_mem, chars_mem, cols_mem, palette_mem, (ulong)(width * height));
            palette_mem.enqueue_write_to_device();
            copykernel.enqueue_run();
            kernel.enqueue_run();
            chars_mem.enqueue_read_from_device();
            cols_mem.enqueue_read_from_device();
            device->finish_queue();
        } catch (std::exception &e) {
            delete[] *chars;
            delete[] *cols;
            *chars = NULL;
            *cols = NULL;
            throw e;
        }
    } else {
#endif
        input.download();
        uchar * colors = new uchar[height * width];
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x+=2) {
                if (input[y][x] > 15) {
                    delete[] colors;
                    throw std::runtime_error("Too many colors (1)");
                }
                if (input[y][x+1] > 15) {
                    delete[] colors;
                    throw std::runtime_error("Too many colors (2)");
                }
                colors[(y-y%3)*width + x*3 + (y%3)*2] = input[y][x];
                colors[(y-y%3)*width + x*3 + (y%3)*2 + 1] = input[y][x+1];
            }
        }
        *chars = new uchar[(height / 3) * (width / 2)];
        *cols = new uchar[(height / 3) * (width / 2)];
        for (int i = 0; i < (height / 3) * (width / 2); i++)
            work.push(makeCCImage_worker, new makeCCImage_state {i, colors, chars, cols, pal, (ulong)(width * height)});
        work.wait();
        delete[] colors;
#ifdef HAS_OPENCL
    }
#endif
}

std::string makeTable(const uchar * characters, const uchar * colors, const std::vector<Vec3b>& palette, int width, int height, bool compact, bool embedPalette, bool binary) {
    std::stringstream retval;
    retval << (compact ? "{" : "{\n");
    for (int y = 0; y < height; y++) {
        std::string text, fg, bg;
        for (int x = 0; x < width; x++) {
            uchar c = characters[y*width+x], cc = colors[y*width+x];
            if ((binary || (c >= 32 && c < 127)) && c != '"' && c != '\\') text += c;
            else text += (c < 10 ? "\\00" : (c < 100 ? "\\0" : "\\")) + std::to_string(c);
            fg += hexstr[cc & 0xf];
            bg += hexstr[cc >> 4];
        }
        if (compact) retval << "{\"" << text << "\",\"" << fg << "\",\"" << bg << "\"},";
        else retval << "    {\n        \"" << text << "\",\n        \"" << fg << "\",\n        \"" << bg << "\"\n    },\n";
    }
    retval << (embedPalette ? (compact ? "palette={" : "    palette = {\n") : (compact ? "},{" : "}, {\n"));
    bool first = true;
    for (const Vec3b& c : palette) {
        if (compact) {
            if (first) retval << "[0]=";
            retval << "{" << std::to_string(c[2] / 255.0) << "," << std::to_string(c[1] / 255.0) << "," << std::to_string(c[0] / 255.0) << "},";
        } else if (first) retval << "    [0] = {" << std::to_string(c[2] / 255.0) << ", " << std::to_string(c[1] / 255.0) << ", " << std::to_string(c[0] / 255.0) << "},\n";
        else retval << "    {" << std::to_string(c[2] / 255.0) << ", " << std::to_string(c[1] / 255.0) << ", " << std::to_string(c[0] / 255.0) << "},\n";
        first = false;
    }
    retval << (embedPalette ? (compact ? "}}" : "    }\n}") : "}");
    return retval.str();
}

std::string makeNFP(const uchar * characters, const uchar * colors, const std::vector<Vec3b>& palette, int width, int height) {
    std::stringstream retval;
    for (int y = 0; y < height; y++) {
        std::string lines[3];
        for (int x = 0; x < width; x++) {
            int offset = y*height+x;
            char fg = hexstr[colors[offset]&0xf], bg = hexstr[colors[offset]>>4];
            uchar ch = characters[offset];
            lines[0] += ch & 1 ? fg : bg;
            lines[0] += ch & 2 ? fg : bg;
            lines[1] += ch & 4 ? fg : bg;
            lines[1] += ch & 8 ? fg : bg;
            lines[2] += ch & 16 ? fg : bg;
            lines[2] += bg;
        }
        retval << lines[0] << "\n" << lines[1] << "\n" << lines[2] << "\n";
    }
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
    for (int i = 0; i < palette.size(); i++) {
        output.put(palette[i][2]);
        output.put(palette[i][1]);
        output.put(palette[i][0]);
    }
    for (int i = palette.size(); i < 16; i++) {
        output.put(0); output.put(0); output.put(0);
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

std::string make32vid(const uchar * characters, const uchar * colors, const std::vector<Vec3b>& palette, int width, int height) {
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

struct tree_node {
    tree_node * left = NULL;
    tree_node * right = NULL;
    size_t weight = 0;
    uint8_t data = 0;
    bool operator<(const tree_node& rhs) {return weight >= rhs.weight;}
};

struct huffman_code {
    uint8_t symbol = 0;
    uint8_t bits = 0;
    uint16_t code = 0;
    huffman_code() = default;
    huffman_code(uint8_t s, uint8_t b, uint16_t c): symbol(s), bits(b), code(c) {}
};

static int treeHeight(tree_node * node) {
    return max(node->left != NULL ? treeHeight(node->left) : 0, node->right != NULL ? treeHeight(node->right) : 0) + 1;
}

static void loadCodes(tree_node * node, std::vector<huffman_code*>& codes, huffman_code * array, huffman_code partial) {
    if (node->left && node->right) {
        loadCodes(node->left, codes, array, {0, (uint8_t)(partial.bits + 1), (uint16_t)(partial.code << 1)});
        loadCodes(node->right, codes, array, {0, (uint8_t)(partial.bits + 1), (uint16_t)((partial.code << 1) | 1)});
    } else {
        partial.symbol = node->data;
        array[partial.symbol] = partial;
        codes.push_back(&array[partial.symbol]);
    }
}

struct compare_node {bool operator()(tree_node *a, tree_node *b) {
    int ah = treeHeight(a), bh = treeHeight(b);
    if (ah > 14) return true;
    if (bh > 14) return false;
    return *a < *b;
}};

std::string make32vid_cmp(const uchar * characters, const uchar * colors, const std::vector<Vec3b>& palette, int width, int height) {
    std::string screen, col, pal;
    tree_node screen_nodes[32] = {}, color_nodes[24] = {}; // color codes 16-23 = repeat last color 2^(n-15) times
    tree_node internal[31];
    tree_node * internal_next = internal;
    uchar * fgcolors = new uchar[width*height];
    uchar * bgcolors = new uchar[width*height];
    uchar *fgnext = fgcolors, *bgnext = bgcolors;
    uchar fc = 0, fn = 0, bc = 0, bn = 0;
    bool fset = false, bset = false;
    // add weights for Huffman coding
    for (int i = 0; i < width * height; i++) {
        screen_nodes[characters[i] & 0x1F].weight++;
        // RLE encode colors
        uchar fg = colors[i] & 0x0F, bg = colors[i] >> 4;
        bool ok = true;
        if (fn && (fg != fc || fn == 255)) {
            if (!fset) {
                *fgnext++ = fc;
                color_nodes[fc].weight++;
            }
            if (fg == fc) {
                *fgnext++ = 23; color_nodes[23].weight++;
                ok = false;
                fset = true;
            } else {
                fset = false;
                if (--fn) {
                    if (fn & 1) {*fgnext++ = fc; color_nodes[fc].weight++;}
                    if (fn & 2) {*fgnext++ = 16; color_nodes[16].weight++;}
                    if (fn & 4) {*fgnext++ = 17; color_nodes[17].weight++;}
                    if (fn & 8) {*fgnext++ = 18; color_nodes[18].weight++;}
                    if (fn & 16) {*fgnext++ = 19; color_nodes[19].weight++;}
                    if (fn & 32) {*fgnext++ = 20; color_nodes[20].weight++;}
                    if (fn & 64) {*fgnext++ = 21; color_nodes[21].weight++;}
                    if (fn & 128) {*fgnext++ = 22; color_nodes[22].weight++;}
                }
            }
            fn = 0;
            fc = fg;
        }
        if (ok) fn++;
        ok = true;
        if (bn && (bg != bc || bn == 255)) {
            if (!bset) {
                *bgnext++ = bc;
                color_nodes[bc].weight++;
            }
            if (bg == bc) {
                *bgnext++ = 23; color_nodes[23].weight++;
                ok = false;
                bset = true;
            } else {
                bset = false;
                if (--bn) {
                    if (bn & 1) {*bgnext++ = bc; color_nodes[bc].weight++;}
                    if (bn & 2) {*bgnext++ = 16; color_nodes[16].weight++;}
                    if (bn & 4) {*bgnext++ = 17; color_nodes[17].weight++;}
                    if (bn & 8) {*bgnext++ = 18; color_nodes[18].weight++;}
                    if (bn & 16) {*bgnext++ = 19; color_nodes[19].weight++;}
                    if (bn & 32) {*bgnext++ = 20; color_nodes[20].weight++;}
                    if (bn & 64) {*bgnext++ = 21; color_nodes[21].weight++;}
                    if (bn & 128) {*bgnext++ = 22; color_nodes[22].weight++;}
                }
            }
            bn = 0;
            bc = bg;
        }
        if (ok) bn++;
    }
    if (fn) {
        if (!fset) {
            *fgnext++ = fc;
            color_nodes[fc].weight++;
        }
        if (--fn) {
            if (fn & 1) {*fgnext++ = fc; color_nodes[fc].weight++;}
            if (fn & 2) {*fgnext++ = 16; color_nodes[16].weight++;}
            if (fn & 4) {*fgnext++ = 17; color_nodes[17].weight++;}
            if (fn & 8) {*fgnext++ = 18; color_nodes[18].weight++;}
            if (fn & 16) {*fgnext++ = 19; color_nodes[19].weight++;}
            if (fn & 32) {*fgnext++ = 20; color_nodes[20].weight++;}
            if (fn & 64) {*fgnext++ = 21; color_nodes[21].weight++;}
            if (fn & 128) {*fgnext++ = 22; color_nodes[22].weight++;}
        }
    }
    if (bn) {
        if (!bset) {
            *bgnext++ = bc;
            color_nodes[bc].weight++;
        }
        if (--bn) {
            if (bn & 1) {*bgnext++ = bc; color_nodes[bc].weight++;}
            if (bn & 2) {*bgnext++ = 16; color_nodes[16].weight++;}
            if (bn & 4) {*bgnext++ = 17; color_nodes[17].weight++;}
            if (bn & 8) {*bgnext++ = 18; color_nodes[18].weight++;}
            if (bn & 16) {*bgnext++ = 19; color_nodes[19].weight++;}
            if (bn & 32) {*bgnext++ = 20; color_nodes[20].weight++;}
            if (bn & 64) {*bgnext++ = 21; color_nodes[21].weight++;}
            if (bn & 128) {*bgnext++ = 22; color_nodes[22].weight++;}
        }
    }
    for (int i = 0; i < 16; i++) {
        if (i < palette.size()) {
            pal += palette[i][2];
            pal += palette[i][1];
            pal += palette[i][0];
        } else pal += std::string((size_t)3, (char)0);
    }

    // compress screen data
    // construct Huffman tree
    std::priority_queue<tree_node*, std::vector<tree_node*>, compare_node> queue;
    for (int i = 0; i < 32; i++) {
        screen_nodes[i].data = i;
        if (screen_nodes[i].weight) queue.push(&screen_nodes[i]);
    }
    if (queue.size() == 1) {
        // encode a full-screen pattern of the same thing
        screen = std::string(16, '\0') + std::string(1, queue.top()->data);
    } else {
        while (queue.size() > 1) {
            tree_node * parent = internal_next++;
            parent->left = queue.top();
            queue.pop();
            parent->right = queue.top();
            queue.pop();
            parent->weight = parent->left->weight + parent->right->weight;
            queue.push(parent);
        }
        // make canonical codebook
        tree_node * root = queue.top();
        huffman_code codebook[32];
        std::vector<huffman_code*> codes;
        loadCodes(root, codes, codebook, {0, 0, 0});
        std::sort(codes.begin(), codes.end(), [](const huffman_code* a, const huffman_code* b)->bool {return a->bits == b->bits ? a->symbol < b->symbol : a->bits < b->bits;});
        codes[0]->code = 0;
        for (int i = 1; i < codes.size(); i++) {
            if (codes[i]->bits > 15) throw std::logic_error("Too many bits!");
            codes[i]->code = (codes[i-1]->code + 1) << (codes[i]->bits - codes[i-1]->bits);
        }
        // compress data
        for (int i = 0; i < 32; i += 2) screen += codebook[i].bits << 4 | codebook[i+1].bits;
        uint32_t tmp = 0;
        int shift = 32;
        for (int i = 0; i < width * height; i++) {
            huffman_code& code = codebook[characters[i] & 0x1F];
            tmp |= code.code << (shift - code.bits);
            shift -= code.bits;
            while (shift <= 24) {
                screen += (uint8_t)(tmp >> 24);
                tmp <<= 8;
                shift += 8;
            }
        }
        if (shift < 32) screen += (uint8_t)(tmp >> 24);
    }
    
    // compress color data
    // construct Huffman tree
    while (!queue.empty()) queue.pop();
    internal_next = internal;
    for (int i = 0; i < 24; i++) {
        color_nodes[i].data = i;
        if (color_nodes[i].weight) queue.push(&color_nodes[i]);
    }
    if (queue.size() == 1) {
        // encode a full-screen pattern of the same thing (this will probably never happen)
        col = std::string(12, '\0') + std::string(1, queue.top()->data);
    } else {
        while (queue.size() > 1) {
            tree_node * parent = internal_next++;
            parent->left = queue.top();
            queue.pop();
            parent->right = queue.top();
            queue.pop();
            parent->weight = parent->left->weight + parent->right->weight;
            queue.push(parent);
        }
        // make canonical codebook
        tree_node * root = queue.top();
        huffman_code codebook[24];
        std::vector<huffman_code*> codes;
        loadCodes(root, codes, codebook, {0, 0, 0});
        std::sort(codes.begin(), codes.end(), [](const huffman_code* a, const huffman_code* b)->bool {return a->bits == b->bits ? a->symbol < b->symbol : a->bits < b->bits;});
        codes[0]->code = 0;
        for (int i = 1; i < codes.size(); i++) {
            if (codes[i]->bits > 15) throw std::logic_error("Too many bits!");
            codes[i]->code = (codes[i-1]->code + 1) << (codes[i]->bits - codes[i-1]->bits);
        }
        // compress data
        for (int i = 0; i < 24; i += 2) col += codebook[i].bits << 4 | codebook[i+1].bits;
        uint32_t tmp = 0;
        int shift = 32;
        for (uchar * i = fgcolors; i < fgnext; i++) {
            huffman_code& code = codebook[*i];
            tmp |= code.code << (shift - code.bits);
            shift -= code.bits;
            while (shift <= 24) {
                col += (uint8_t)(tmp >> 24);
                tmp <<= 8;
                shift += 8;
            }
        }
        for (uchar * i = bgcolors; i < bgnext; i++) {
            huffman_code& code = codebook[*i];
            tmp |= code.code << (shift - code.bits);
            shift -= code.bits;
            while (shift <= 24) {
                col += (uint8_t)(tmp >> 24);
                tmp <<= 8;
                shift += 8;
            }
        }
        if (shift < 32) col += (uint8_t)(tmp >> 24);
    }

    delete[] fgcolors;
    delete[] bgcolors;
    //std::cout << screen.size() << "/" << col.size() << "\n";
    return screen + col + pal;
}

static int log2i(unsigned int n) {
    if (n == 0) return 0;
    int i = 0;
    while (n) {i++; n >>= 1;}
    return i - 1;
}

class ANSEncoder {
    uint32_t * bitstream;
    size_t length = 0;
    size_t size = 1024;
    size_t bitCount = 0;
    const uint32_t R, L;
    uint32_t X = 0;
    uint32_t * nb;
    int32_t * start;
    uint32_t * encodingTable;
public:
    ANSEncoder(const uint8_t _R, const std::vector<uint32_t>& Ls): R(_R), L(1 << _R) {
        bitstream = (uint32_t*)malloc(size*4);
        // prepare encoding
        size_t Lsl = Ls.size();
        nb = new uint32_t[Lsl];
        start = new int32_t[Lsl];
        uint32_t * next = new uint32_t[Lsl];
        uint8_t * symbol = new uint8_t[L];
        memset(symbol, 0xFF, L);
        uint32_t sumLs = 0;
        const uint32_t step = ((L * 5) >> 3) + 3;
        for (uint8_t s = 0; s < Lsl; s++) {
            const uint32_t v = Ls[s];
            //std::cout << (int)s << " " << v << "\n";
            if (v) {
                const uint32_t ks = R - log2i(v);
                nb[s] = (ks << (R+1)) - (v << ks);
                start[s] = (int32_t)sumLs - (int32_t)v;
                next[s] = v;
                for (int i = 0; i < v; i++) {
                    while (symbol[X] != 0xFF) X = (X + 1) % L;
                    symbol[X] = s;
                    X = (X + step) % L;
                }
                sumLs = sumLs + v;
            }
        }
        // create encoding table
        encodingTable = new uint32_t[2*L];
        for (uint32_t x = L; x < 2*L; x++) {
            const uint8_t s = symbol[x - L];
            encodingTable[start[s] + next[s]++] = x;
        }
        X = L;
        // free temps
        delete[] next;
        delete[] symbol;
    }

    ~ANSEncoder() {
        free(bitstream);
        delete[] nb;
        delete[] start;
        delete[] encodingTable;
    }

    static std::vector<uint32_t> makeLs(uint32_t * Ls, size_t LsS, uint8_t& R, uint8_t maxHeight = 15) {
        // construct Huffman tree
        tree_node nodes[32];
        tree_node internal[31];
        tree_node * internal_next = internal;
        std::priority_queue<tree_node*, std::vector<tree_node*>, compare_node> queue;
        for (int i = 0; i < LsS; i++) {
            nodes[i].data = i;
            nodes[i].weight = Ls[i];
            nodes[i].left = nodes[i].right = NULL;
            if (nodes[i].weight) queue.push(&nodes[i]);
        }
        if (queue.size() == 0) {
            return {};
        } else if (queue.size() == 1) {
            return {queue.top()->data};
        } else {
            while (queue.size() > 1) {
                tree_node * parent = internal_next++;
                parent->left = queue.top();
                queue.pop();
                parent->right = queue.top();
                queue.pop();
                parent->weight = parent->left->weight + parent->right->weight;
                queue.push(parent);
            }
            // make canonical codebook
            tree_node * root = queue.top();
            huffman_code codebook[32];
            std::vector<huffman_code*> codes;
            for (int i = 0; i < 32; i++) codebook[i].bits = 0;
            loadCodes(root, codes, codebook, {0, 0, 0});
            std::sort(codes.begin(), codes.end(), [](const huffman_code* a, const huffman_code* b)->bool {return a->bits == b->bits ? a->symbol < b->symbol : a->bits < b->bits;});
            codes[0]->code = 0;
            for (int i = 1; i < codes.size(); i++) {
                if (codes[i]->bits > 15) throw std::logic_error("Too many bits!");
                codes[i]->code = (codes[i-1]->code + 1) << (codes[i]->bits - codes[i-1]->bits);
            }
            R = 3;
            for (int i = 0; i < LsS; i++) R = max(R, codebook[i].bits);
            // make Huffman Ls
            std::vector<uint32_t> LsH(LsS);
            uint32_t huffSum = 0;
            for (int i = 0; i < LsS; i++) {
                const uint8_t v = codebook[i].bits;
                LsH[i] = v ? 1 << (R - v) : 0;
                huffSum += LsH[i];
            }
            R = log2i(huffSum);
            return LsH;
        }
    }

    void encode(const uint8_t * symbols, size_t _size) {
        bool sSet = false;
        uint8_t s = symbols[_size-1] & 0x1F;
        for (int i = _size-2; i >= 0; i--) {
            const uint8_t nbBits = ((X + nb[s]) >> (R + 1));
            //std::cout << (int)s << " " << (int)nbBits << "\n";
            const uint8_t nexts = symbols[i] & 0x1F;
            bitstream[length++] = ((uint32_t)nbBits << 24) | (X & ((1 << nbBits) - 1));
            bitCount += nbBits;
            if (length >= size) {
                bitstream = (uint32_t*)realloc(bitstream, size * 4 + 4096);
                size += 1024;
            }
            X = encodingTable[start[s] + (X >> nbBits)];
            s = nexts;
        }
        const uint8_t nbBits = ((X + nb[s]) >> (R + 1));
        bitstream[length++] = ((uint32_t)nbBits << 24) | (X & ((1 << nbBits) - 1));
        bitCount += nbBits;
        if (length >= size) {
            bitstream = (uint32_t*)realloc(bitstream, size * 4 + 4096);
            size += 1024;
        }
        X = encodingTable[start[s] + (X >> nbBits)];
    }

    std::string output() const {
        uint32_t pending = 0;
        uint8_t bits = 0;
        uint8_t * data = new uint8_t[bitCount / 8 + 3];
        size_t pos = 0;
        pending = (pending << R) | (X - L);
        bits += R;
        while (bits >= 8) {
            data[pos++] = (pending >> (bits - 8)) & 0xFF;
            bits -= 8;
        }
        for (int i = length - 1; i >= 0; i--) {
            const uint8_t nbBits = bitstream[i] >> 24;
            pending = (pending << nbBits) | (bitstream[i] & 0xFFFFFF);
            bits += nbBits;
            while (bits >= 8) {
                data[pos++] = (pending >> (bits - 8)) & 0xFF;
                bits -= 8;
            }
        }
        if (bits > 0) data[pos++] = (pending << (8 - bits)) & 0xFF;
        std::string retval((const char*)data, pos);
        delete[] data;
        return retval;
    }
};

static uint8_t ansdictcode(uint32_t n) {
    if (n == 0) return 0;
    else return log2i(n) + 1;
}

std::string make32vid_ans(const uchar * characters, const uchar * colors, const std::vector<Vec3b>& palette, int width, int height) {
    std::string screen, col, pal;
    uint32_t screen_freq[32] = {0}, color_freq[24] = {0}; // color codes 16-23 = repeat last color 2^(n-15) times
    uchar * fgcolors = new uchar[width*height];
    uchar * bgcolors = new uchar[width*height];
    uchar *fgnext = fgcolors, *bgnext = bgcolors;
    uchar fc = colors[0] >> 4, fn = 0, bc = colors[0] & 0xF, bn = 0;
    bool fset = false, bset = false;
    // add weights for Huffman coding
    for (int i = 0; i < width * height; i++) {
        screen_freq[characters[i] & 0x1F]++;
        // RLE encode colors
        uchar fg = colors[i] & 0x0F, bg = colors[i] >> 4;
        bool ok = true;
        if (fn && (fg != fc || fn == 255)) {
            if (!fset) {
                *fgnext++ = fc;
                color_freq[fc]++;
            }
            if (fg == fc) {
                *fgnext++ = 23; color_freq[23]++;
                ok = false;
                fset = true;
            } else {
                fset = false;
                if (--fn) {
                    if (fn & 1) {*fgnext++ = fc; color_freq[fc]++;}
                    if (fn & 2) {*fgnext++ = 16; color_freq[16]++;}
                    if (fn & 4) {*fgnext++ = 17; color_freq[17]++;}
                    if (fn & 8) {*fgnext++ = 18; color_freq[18]++;}
                    if (fn & 16) {*fgnext++ = 19; color_freq[19]++;}
                    if (fn & 32) {*fgnext++ = 20; color_freq[20]++;}
                    if (fn & 64) {*fgnext++ = 21; color_freq[21]++;}
                    if (fn & 128) {*fgnext++ = 22; color_freq[22]++;}
                }
            }
            fn = 0;
            fc = fg;
        }
        if (ok) fn++;
        ok = true;
        if (bn && (bg != bc || bn == 255)) {
            if (!bset) {
                *bgnext++ = bc;
                color_freq[bc]++;
            }
            if (bg == bc) {
                *bgnext++ = 23; color_freq[23]++;
                ok = false;
                bset = true;
            } else {
                bset = false;
                if (--bn) {
                    if (bn & 1) {*bgnext++ = bc; color_freq[bc]++;}
                    if (bn & 2) {*bgnext++ = 16; color_freq[16]++;}
                    if (bn & 4) {*bgnext++ = 17; color_freq[17]++;}
                    if (bn & 8) {*bgnext++ = 18; color_freq[18]++;}
                    if (bn & 16) {*bgnext++ = 19; color_freq[19]++;}
                    if (bn & 32) {*bgnext++ = 20; color_freq[20]++;}
                    if (bn & 64) {*bgnext++ = 21; color_freq[21]++;}
                    if (bn & 128) {*bgnext++ = 22; color_freq[22]++;}
                }
            }
            bn = 0;
            bc = bg;
        }
        if (ok) bn++;
    }
    if (fn) {
        if (!fset) {
            *fgnext++ = fc;
            color_freq[fc]++;
        }
        if (--fn) {
            if (fn & 1) {*fgnext++ = fc; color_freq[fc]++;}
            if (fn & 2) {*fgnext++ = 16; color_freq[16]++;}
            if (fn & 4) {*fgnext++ = 17; color_freq[17]++;}
            if (fn & 8) {*fgnext++ = 18; color_freq[18]++;}
            if (fn & 16) {*fgnext++ = 19; color_freq[19]++;}
            if (fn & 32) {*fgnext++ = 20; color_freq[20]++;}
            if (fn & 64) {*fgnext++ = 21; color_freq[21]++;}
            if (fn & 128) {*fgnext++ = 22; color_freq[22]++;}
        }
    }
    if (bn) {
        if (!bset) {
            *bgnext++ = bc;
            color_freq[bc]++;
        }
        if (--bn) {
            if (bn & 1) {*bgnext++ = bc; color_freq[bc]++;}
            if (bn & 2) {*bgnext++ = 16; color_freq[16]++;}
            if (bn & 4) {*bgnext++ = 17; color_freq[17]++;}
            if (bn & 8) {*bgnext++ = 18; color_freq[18]++;}
            if (bn & 16) {*bgnext++ = 19; color_freq[19]++;}
            if (bn & 32) {*bgnext++ = 20; color_freq[20]++;}
            if (bn & 64) {*bgnext++ = 21; color_freq[21]++;}
            if (bn & 128) {*bgnext++ = 22; color_freq[22]++;}
        }
    }
    for (int i = 0; i < 16; i++) {
        if (i < palette.size()) {
            pal += palette[i][2];
            pal += palette[i][1];
            pal += palette[i][0];
        } else pal += std::string((size_t)3, (char)0);
    }

    // compress screen data
    // create Ls
    uint8_t screenR = 0;
    std::vector<uint32_t> screenLs = ANSEncoder::makeLs(screen_freq, 32, screenR);
    if (screenLs.size() == 1) {
        // encode a full-screen pattern of the same thing
        screen = std::string(17, '\0') + std::string(1, screenLs[0]);
    } else {
        // compress data
        screen += screenR;
        for (int i = 0; i < 32; i += 2) screen += ansdictcode(screenLs[i]) << 4 | ansdictcode(screenLs[i+1]);
        ANSEncoder encoder(screenR, screenLs);
        encoder.encode(characters, width * height);
        screen += encoder.output();
    }
    
    // compress color data
    // create Ls
    uint8_t colorsR = 0;
    std::vector<uint32_t> colorsLs = ANSEncoder::makeLs(color_freq, 24, colorsR);
    if (colorsLs.size() == 1) {
        // encode a full-screen pattern of the same thing
        col = std::string(13, '\0') + std::string(1, colorsLs[0]);
    } else {
        // compress data
        col += colorsR;
        for (int i = 0; i < 24; i += 2) col += ansdictcode(colorsLs[i]) << 4 | ansdictcode(colorsLs[i+1]);
        ANSEncoder encoder(colorsR, colorsLs);
        encoder.encode(fgcolors, fgnext - fgcolors);
        encoder.encode(bgcolors, bgnext - bgcolors);
        col += encoder.output();
    }

    delete[] fgcolors;
    delete[] bgcolors;
    //std::cout << screen.size() << "/" << col.size() << "\n";
    return screen + col + pal;
}

std::string makeLuaFile(const uchar * characters, const uchar * colors, const std::vector<Vec3b>& palette, int width, int height) {
    return "-- Generated with sanjuuni\n-- https://sanjuuni.madefor.cc\ndo\nlocal image, palette = " + makeTable(characters, colors, palette, width, height) + "\n\nterm.clear()\nfor i = 0, #palette do term.setPaletteColor(2^i, table.unpack(palette[i])) end\nfor y, r in ipairs(image) do\n    term.setCursorPos(1, y)\n    term.blit(table.unpack(r))\nend\nend\n";
}
