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
#include <Poco/Base64Encoder.h>
#include <Poco/Checksum.h>

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
