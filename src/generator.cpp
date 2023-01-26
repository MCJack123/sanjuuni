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
#include <sstream>
#include <algorithm>
#include <Poco/Base64Encoder.h>
#include <Poco/Checksum.h>

static const char * hexstr = "0123456789abcdef";

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
    for (int i = 0; i < palette.size() && i < 16; i++) pal[i] = {palette[i][0], palette[i][1], palette[i][2]};
    for (int i = 0; i < (height / 3) * (width / 2); i++)
        work.push(makeCCImage_worker, new makeCCImage_state {i, colors, chars, cols, pal});
    work.wait();
    delete[] pal;
    delete[] colors;
}

std::string makeTable(const uchar * characters, const uchar * colors, const std::vector<Vec3b>& palette, int width, int height, bool compact, bool embedPalette, bool binary) {
    std::stringstream retval;
    retval << (compact ? "{" : "{\n");
    for (int y = 0; y < height; y++) {
        std::string text, fg, bg;
        for (int x = 0; x < width; x++) {
            uchar c = characters[y*width+x], cc = colors[y*width+x];
            if ((binary || (c >= 32 && c < 127)) && c != '"' && c != '\\') text += c;
            else text += "\\" + std::to_string(c);
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

struct compare_node {bool operator()(tree_node *a, tree_node *b) {return *a < *b;}};

std::string make32vid_cmp(const uchar * characters, const uchar * colors, const std::vector<Vec3b>& palette, int width, int height) {
    std::string screen, col, pal;
    tree_node screen_nodes[32], color_nodes[24]; // color codes 16-23 = repeat last color 2^(n-15) times
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

std::string makeLuaFile(const uchar * characters, const uchar * colors, const std::vector<Vec3b>& palette, int width, int height) {
    return "local image, palette = " + makeTable(characters, colors, palette, width, height) + "\n\nterm.clear()\nfor i = 0, #palette in ipairs(palette) do term.setPaletteColor(2^i, table.unpack(palette[i])) end\nfor y, r in ipairs(image) do\n    term.setCursorPos(1, y)\n    term.blit(table.unpack(r))\nend\nread()\nfor i = 0, 15 do term.setPaletteColor(2^i, term.nativePaletteColor(2^i)) end\nterm.setBackgroundColor(colors.black)\nterm.setTextColor(colors.white)\nterm.setCursorPos(1, 1)\nterm.clear()\n";
}
