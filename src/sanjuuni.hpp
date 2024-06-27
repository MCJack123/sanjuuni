/*
 * sanjuuni.hpp
 * Common types for sanjuuni, as well as function definitions for other files.
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

#include <cstdint>
#include <cmath>
#include <array>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <atomic>
#include <stdexcept>

#ifdef HAS_OPENCL
#include "opencl.hpp"
#else
namespace OpenCL {typedef struct Device Device;}
typedef unsigned int uint;
typedef unsigned long ulong;
#endif

/* Type definitions for ComputerCraft character-related code. */
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

/* Type definitions for quantization code. */
struct Vec3d : public std::array<double, 3> {
    Vec3d() = default;
    Vec3d(const uchar3& a) {(*this)[0] = a.x; (*this)[1] = a.y; (*this)[2] = a.z;}
    template<typename T> Vec3d(const std::array<T, 3>& a) {(*this)[0] = a[0]; (*this)[1] = a[1]; (*this)[2] = a[2];}
    template<typename T> Vec3d(std::initializer_list<T> il) {(*this)[0] = *(il.begin()); (*this)[1] = *(il.begin()+1); (*this)[2] = *(il.begin()+2);}
    Vec3d operator+(const Vec3d& b) const {return {(*this)[0] + b[0], (*this)[1] + b[1], (*this)[2] + b[2]};}
    Vec3d operator+(double b) const {return {(double)(*this)[0] + b, (double)(*this)[1] + b, (double)(*this)[2] + b};}
    Vec3d operator-(const Vec3d& b) const {return {(*this)[0] - b[0], (*this)[1] - b[1], (*this)[2] - b[2]};}
    Vec3d operator*(double b) const {return {(double)(*this)[0] * b, (double)(*this)[1] * b, (double)(*this)[2] * b};}
    Vec3d operator/(double b) const {return {(*this)[0] / b, (*this)[1] / b, (*this)[2] / b};}
    Vec3d& operator+=(const Vec3d& a) {(*this)[0] += a[0]; (*this)[1] += a[1]; (*this)[2] += a[2]; return *this;}
};

struct Vec3b : public std::array<uint8_t, 3> {
    Vec3b() = default;
    Vec3b(const uchar3& a) {(*this)[0] = a.x; (*this)[1] = a.y; (*this)[2] = a.z;}
    template<typename T> Vec3b(const std::array<T, 3>& a) {(*this)[0] = a[0]; (*this)[1] = a[1]; (*this)[2] = a[2];}
    template<typename T> Vec3b(std::initializer_list<T> il) {(*this)[0] = *(il.begin()); (*this)[1] = *(il.begin()+1); (*this)[2] = *(il.begin()+2);}
    operator uchar3() const {return {(*this)[0], (*this)[1], (*this)[2]};}
};

template<typename T>
class vector2d {
public:
    unsigned width;
    unsigned height;
    std::vector<T> vec;
#ifdef HAS_OPENCL
    std::shared_ptr<OpenCL::Memory<T>> mem;
#endif
    bool onHost = true, onDevice = false;
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
    vector2d(unsigned w, unsigned h, OpenCL::Device * dev, T v = T()): width(w), height(h), vec((size_t)w*h, v) {
#ifdef HAS_OPENCL
        if (dev != NULL) mem = std::make_shared<OpenCL::Memory<T>>(*dev, w * h, 1, vec.data());
#endif
    }
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
    void remove_last_line() {vec.resize(width*--height);}
    void download() {
#ifdef HAS_OPENCL
        if (mem != NULL && !onHost && onDevice) mem->read_from_device();
#endif
        onHost = true;
    }
    void upload() {
#ifdef HAS_OPENCL
        if (mem != NULL && !onDevice && onHost) mem->write_to_device();
#endif
        onDevice = true;
    }
};

/* Class for handling multicore workloads. */
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
        if (expected_finish_count > 0) wait();
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
typedef vector2d<uchar3> Mat;
#undef min
#undef max
template<typename T> inline T min(T a, T b) {return a < b ? a : b;}
template<typename T> inline T max(T a, T b) {return a > b ? a : b;}

/* 32vid types and constants. */
#define VID32_FLAG_VIDEO_COMPRESSION_NONE     0x0000
#define VID32_FLAG_VIDEO_COMPRESSION_ANS      0x0001
#define VID32_FLAG_VIDEO_COMPRESSION_DEFLATE  0x0002
#define VID32_FLAG_VIDEO_COMPRESSION_CUSTOM   0x0003
#define VID32_FLAG_AUDIO_COMPRESSION_NONE     0x0000
#define VID32_FLAG_AUDIO_COMPRESSION_DFPWM    0x0004
#define VID32_FLAG_VIDEO_5BIT_CODES           0x0010
#define VID32_FLAG_VIDEO_MULTIMONITOR         0x0020
#define VID32_FLAG_VIDEO_MULTIMONITOR_WIDTH(x)  ((x & 7) << 6)
#define VID32_FLAG_VIDEO_MULTIMONITOR_HEIGHT(x) ((x & 7) << 9)

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
        Subtitle4,
        Combined,
        CombinedIndex,

        MultiMonitorVideo = 64
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

struct Vid32CombinedFrame {
    uint32_t size;
    uint8_t type;
    uint8_t data[];
};

struct ASSSubtitleEvent {
    unsigned width;
    unsigned height;
    unsigned startFrame;
    unsigned length;
    uint8_t alignment;
    int marginLeft;
    int marginRight;
    int marginVertical;
    Vec3b color;
    std::string text;
};

/** A global work queue to push tasks to. If using as a library, remember to define this! */
extern WorkQueue work;

/* cc-pixel */
/**
 * Converts a 2x3 array of colors into a character and color pair using the specified palette.
 * @param colors A 6-long array of pixel values (0-15), from top-left, top-right, ..., bottom-right
 * @param character A pointer to the resulting character
 * @param color A pointer to the resulting color pair
 * @param palette A 16-long array of colors in the palette
 * @param size Reserved for OpenCL
 */
extern void toCCPixel(const uchar * colors, uchar * character, uchar * color, const uchar * palette, ulong size = 1);

/* octree */
/**
 * Generates an optimized palette for an image using octrees.
 * @param bmp The image to generate a palette for
 * @param numColors The number of colors to get
 * @return An optimized palette for the image
 */
extern std::vector<Vec3b> reducePalette_octree(Mat& bmp, int numColors, OpenCL::Device * device = NULL);

/* quantize */
/**
 * Converts an sRGB image into CIELAB color space.
 * @param image The image to convert
 * @return A new image with all pixels in Lab color space
 */
extern Mat makeLabImage(Mat& image, OpenCL::Device * device = NULL);
/**
 * Converts a single RGB color into a CIELAB color.
 * @param color The color to convert
 * @return The color represented in Lab color space
 */
extern Vec3b convertColorToLab(const Vec3b& color);
/**
 * Converts a list of Lab colors into sRGB colors.
 * @param palette The colors to convert
 * @return A new list with all colors converted to RGB
 */
extern std::vector<Vec3b> convertLabPalette(const std::vector<Vec3b>& palette);
/**
 * Determines the nearest palette color for a color.
 * @param palette The palette to search in
 * @param color The color to match
 * @param _n A pointer to store the index in, if desired
 * @return The palette color that is closest to the input color
 */
extern Vec3b nearestColor(const std::vector<Vec3b>& palette, const Vec3d& color, int* _n = NULL);
/**
 * Generates an optimized palette for an image using the median cut algorithm.
 * @param iamge The image to generate a palette for
 * @param numColors The number of colors to get (must be a power of 2)
 * @return An optimized palette for the image
 */
extern std::vector<Vec3b> reducePalette_medianCut(Mat& image, int numColors, OpenCL::Device * device = NULL);
/**
 * Generates an optimized palette for an image using the k-means algorithm.
 * @param iamge The image to generate a palette for
 * @param numColors The number of colors to get
 * @return An optimized palette for the image
 */
extern std::vector<Vec3b> reducePalette_kMeans(Mat& image, int numColors, OpenCL::Device * device = NULL);
/**
 * Reduces the colors in an image using the specified palette through thresholding.
 * @param image The image to reduce
 * @param palette The palette to use
 * @return A reduced-color version of the image using the palette
 */
extern Mat thresholdImage(Mat& image, const std::vector<Vec3b>& palette, OpenCL::Device * device = NULL);
/**
 * Reduces the colors in an image using the specified palette through Floyd-
 * Steinberg dithering.
 * @param image The image to reduce
 * @param palette The palette to use
 * @return A reduced-color version of the image using the palette
 */
extern Mat ditherImage(Mat& image, const std::vector<Vec3b>& palette, OpenCL::Device * device = NULL);
/**
 * Reduces the colors in an image using the specified palette through ordered
 * dithering.
 * @param image The image to reduce
 * @param palette The palette to use
 * @return A reduced-color version of the image using the palette
 */
extern Mat ditherImage_ordered(Mat& image, const std::vector<Vec3b>& palette, OpenCL::Device * device = NULL);
/**
 * Converts an RGB image into an indexed image using the specified palette. The
 * image must have been reduced before using this function.
 * @param image The image to convert
 * @param palette The palette for the image
 * @return An indexed version of the image
 */
extern Mat1b rgbToPaletteImage(Mat& image, const std::vector<Vec3b>& palette, OpenCL::Device * device = NULL);

/* generator */
/**
 * Converts an indexed image into a character-based format suitable for CC.
 * @param input The image to convert
 * @param palette The palette for the image
 * @param chars A pointer to the destination character array pointer - this must
 * be freed with `delete[]` once finished
 * @param cols A pointer to the destination color pair array pointer - this must
 * be freed with `delete[]` once finished
 */
extern void makeCCImage(Mat1b& input, const std::vector<Vec3b>& palette, uchar** chars, uchar** cols, OpenCL::Device * device = NULL);
/**
 * Generates a blit table from the specified CC image.
 * @param characters The character array to use
 * @param colors The color pair array to use
 * @param palette The palette for the image
 * @param width The width of the image in characters
 * @param height The height of the image in characters
 * @param compact Whether to make the output as compact as possible
 * @param embedPalette Whether to embed the palette as a `palette` key (for BIMG)
 * @return The generated blit image source for the image data
 */
extern std::string makeTable(const uchar * characters, const uchar * colors, const std::vector<Vec3b>& palette, int width, int height, bool compact = false, bool embedPalette = false, bool binary = false);
/**
 * Generates an NFP image from the specified CC image. This changes proportions!
 * @param characters The character array to use
 * @param colors The color pair array to use
 * @param palette The palette for the image
 * @param width The width of the image in characters
 * @param height The height of the image in characters
 * @return The generated NFP for the image data
 */
extern std::string makeNFP(const uchar * characters, const uchar * colors, const std::vector<Vec3b>& palette, int width, int height);
/**
 * Generates a Lua display script from the specified CC image. This file can be
 * run directly to show the image on-screen.
 * @param characters The character array to use
 * @param colors The color pair array to use
 * @param palette The palette for the image
 * @param width The width of the image in characters
 * @param height The height of the image in characters
 * @return The generated script file with image embedded
 */
extern std::string makeLuaFile(const uchar * characters, const uchar * colors, const std::vector<Vec3b>& palette, int width, int height);
/**
 * Generates a raw mode frame from the specified CC image.
 * @param characters The character array to use
 * @param colors The color pair array to use
 * @param palette The palette for the image
 * @param width The width of the image in characters
 * @param height The height of the image in characters
 * @return The generated raw mode frame
 */
extern std::string makeRawImage(const uchar * screen, const uchar * colors, const std::vector<Vec3b>& palette, int width, int height);
/**
 * Generates an uncompressed 32vid frame from the specified CC image.
 * @param characters The character array to use
 * @param colors The color pair array to use
 * @param palette The palette for the image
 * @param width The width of the image in characters
 * @param height The height of the image in characters
 * @return The generated 32vid frame
 */
extern std::string make32vid(const uchar * characters, const uchar * colors, const std::vector<Vec3b>& palette, int width, int height);
/**
 * Generates a 32vid frame from the specified CC image using the custom compression scheme.
 * @param characters The character array to use
 * @param colors The color pair array to use
 * @param palette The palette for the image
 * @param width The width of the image in characters
 * @param height The height of the image in characters
 * @return The generated 32vid frame
 */
extern std::string make32vid_cmp(const uchar * characters, const uchar * colors, const std::vector<Vec3b>& palette, int width, int height);
/**
 * Generates a 32vid frame from the specified CC image using the custom ANS compression scheme.
 * @param characters The character array to use
 * @param colors The color pair array to use
 * @param palette The palette for the image
 * @param width The width of the image in characters
 * @param height The height of the image in characters
 * @return The generated 32vid frame
 */
extern std::string make32vid_ans(const uchar * characters, const uchar * colors, const std::vector<Vec3b>& palette, int width, int height);
