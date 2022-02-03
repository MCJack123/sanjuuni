#define CL_HPP_TARGET_OPENCL_VERSION 200
#define CL_HPP_ENABLE_EXCEPTIONS
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <CL/opencl.hpp>
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
using namespace cv;
using namespace cl;
using namespace Poco::Util;

cl::Platform CLPlatform;
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
    try {
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
    }
}

static std::vector<Vec3b> medianCut(std::vector<Vec3b>& pal, int num) {
    if (num == 1) {
        Vec3i sum = {0, 0, 0};
        for (const Vec3b& v : pal) {sum[0] += v[0]; sum[1] += v[1]; sum[2] += v[2];}
        return {Vec3b(sum / (double)pal.size())};
    } else {
        Vec2b red = {255, 0}, green = {255, 0}, blue = {255, 0};
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
    for (int y = 0; y < image.rows; y++)
        for (int x = 0; x < image.cols; x++)
            pal.push_back(image.at<Vec3b>(y, x));
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

Mat ditherImage(Mat image, std::vector<Vec3b> palette) {
    for (int y = 0; y < image.rows; y++) {
        for (int x = 0; x < image.cols; x++) {
            Vec3b c = image.at<Vec3b>(y, x);
            Vec3b newpixel = nearestColor(palette, c);
            image.at<Vec3b>(y, x) = newpixel;
            Vec3i err = Vec3i(c) - Vec3i(newpixel);
            if (x < image.cols - 1) image.at<Vec3b>(y, x + 1) += err * (7.0/16.0);
            if (y < image.rows - 1) {
                if (x > 1) image.at<Vec3b>(y + 1, x - 1) += err * (3.0/16.0);
                image.at<Vec3b>(y + 1, x) += err * (5.0/16.0);
                if (x < image.cols - 1) image.at<Vec3b>(y + 1, x + 1) += err * (1.0/16.0);
            }
        }
    }
    return image;
}

Mat1b rgbToPaletteImage(const Mat& image, const std::vector<Vec3b>& palette) {
    Mat1b output(image.rows, image.cols);
    for (int y = 0; y < image.rows; y++) {
        for (int x = 0; x < image.cols; x++) {
            output.at<uint8_t>(y, x) = std::find(palette.begin(), palette.end(), image.at<Vec3b>(y, x)) - palette.begin();
        }
    }
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

void makeCCImage(const Mat1b& input, const std::vector<Vec3b>& palette, uchar** chars, uchar** cols) {
    // Create the input and output data
    uchar * colors = new uchar[input.rows * input.cols];
    for (int y = 0; y < input.rows - input.rows % 3; y++) {
        for (int x = 0; x < input.cols - input.cols % 2; x+=2) {
            if (input[y][x] > 15) throw std::runtime_error("Too many colors (1)");
            if (input[y][x+1] > 15) throw std::runtime_error("Too many colors (2)");
            colors[(y-y%3)*input.cols + x*3 + (y%3)*2] = input[y][x];
            colors[(y-y%3)*input.cols + x*3 + (y%3)*2 + 1] = input[y][x+1];
        }
    }
    *chars = new uchar[(input.rows / 3) * (input.cols / 2)];
    *cols = new uchar[(input.rows / 3) * (input.cols / 2)];
    uchar3 * pal = new uchar3[palette.size()];
    for (int i = 0; i < palette.size(); i++) pal[i] = {palette[i][0], palette[i][1], palette[i][2]};
    initCL();
    if (CLPlatform() && false) {
        // Use OpenCL for computation
    } else {
        // Use normal CPU for computation
        std::queue<int> offsets;
        std::mutex mtx;
        std::vector<std::thread*> thread_pool;
        unsigned nthreads = std::thread::hardware_concurrency();
        if (!nthreads) nthreads = 8; // default if no value is available
        mtx.lock(); // pause threads until queue is filled
        for (int i = 0; i < nthreads; i++) thread_pool.push_back(new std::thread(ccImageWorker, colors, *chars, *cols, pal, &offsets, &mtx));
        for (int i = 0; i < (input.rows / 3) * (input.cols / 2); i++) offsets.push(i);
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
        output.put(palette[i][0]);
        output.put(palette[i][1]);
        output.put(palette[i][2]);
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

class HelpException: public OptionException {
public:
    virtual const char * className() const noexcept {return "HelpException";}
    virtual const char * name() const noexcept {return "Help";}
};

int main(int argc, const char * argv[]) {
    OptionSet options;
    options.addOption(Option("input", "i", "Input image or video", true, "file", true));
    options.addOption(Option("output", "o", "Output file path", false, "path", true));
    options.addOption(Option("lua", "l", "Output a Lua script file (default for images)"));
    options.addOption(Option("raw", "r", "Output a rawmode-based image/video file (required for videos)"));
    options.addOption(Option("http", "s", "Serve an HTTP server that has each frame split up + a player program", false, "port", true).validator(new IntValidator(1, 65535)));
    options.addOption(Option("websocket", "w", "Serve a WebSocket that sends the image/video with audio", false, "port", true).validator(new IntValidator(1, 65535)));
    options.addOption(Option("default-palette", "p", "Use the default CC palette instead of generating an optimized one"));
    options.addOption(Option("help", "h", "Show this help"));
    OptionProcessor argparse(options);
    argparse.setUnixStyle(true);

    std::string input, output;
    bool useDefaultPalette = false;
    int mode = 0, port;
    try {
        for (int i = 1; i < argc; i++) {
            std::string option, arg;
            if (argparse.process(argv[i], option, arg)) {
                if (option == "input") input = arg;
                else if (option == "output") output = arg;
                else if (option == "lua") mode = 0;
                else if (option == "raw") mode = 1;
                else if (option == "http") {mode = 2; arg = std::stoi(arg);}
                else if (option == "websocket") {mode = 3; arg = std::stoi(arg);}
                else if (option == "default-palette") useDefaultPalette = true;
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

    Mat in = imread(input);
    Mat out;
    std::vector<Vec3b> palette;
    if (useDefaultPalette) palette = defaultPalette;
    else palette = reducePalette(in, 16);
    out = ditherImage(in, palette);
    if (argc > 2) {
        imshow("Image", out);
        waitKey(0);
    }
    Mat1b pimg = rgbToPaletteImage(out, palette);
    uchar *characters, *colors;
    makeCCImage(pimg, palette, &characters, &colors);
    switch (mode) {
    case 0: {
        std::string data = makeLuaFile(characters, colors, palette, pimg.cols / 2, pimg.rows / 3);
        if (output == "-") std::cout << data;
        else {
            std::ofstream out(output);
            if (!out.good()) {
                std::cerr << "Could not open output file!\n";
                break;
            }
            out << data;
            out.close();
        }
        break;
    } case 1: {
        std::string data = makeRawImage(characters, colors, palette, pimg.cols / 2, pimg.rows / 3);
        if (output == "-") std::cout << "32Vid 1.1\n0\n" << data;
        else {
            std::ofstream out(output);
            if (!out.good()) {
                std::cerr << "Could not open output file!\n";
                break;
            }
            out << "32Vid 1.1\n0\n" << data;
            out.close();
        }
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
    return 0;
}
