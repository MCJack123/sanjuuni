/*
 * quantize.cpp
 * Functions for quantizing images, including palette reduction, thresholding,
 * and dithering.
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
#include <algorithm>
#include <list>

#define ALLOWB (2+4+8)

extern void toLab(const uchar * image, uchar * output, ulong size);

#define PARALLEL_BITONIC_B2_KERNEL "ParallelBitonic_B2"
#define PARALLEL_BITONIC_B4_KERNEL "ParallelBitonic_B4"
#define PARALLEL_BITONIC_B8_KERNEL "ParallelBitonic_B8"
#define PARALLEL_BITONIC_B16_KERNEL "ParallelBitonic_B16"
#define PARALLEL_BITONIC_C2_KERNEL "ParallelBitonic_C2"
#define PARALLEL_BITONIC_C4_KERNEL "ParallelBitonic_C4"

Mat makeLabImage(Mat& image, OpenCL::Device * device) {
    Mat retval(image.width, image.height, device);
#ifdef HAS_OPENCL
    if (device != NULL) {
        image.upload();
        OpenCL::Kernel kernel(*device, image.width * image.height, "toLab", *image.mem, *retval.mem, (ulong)(image.width * image.height));
        kernel.run();
        retval.onHost = false;
        retval.onDevice = true;
    } else {
#endif
        image.download();
        retval.onDevice = false;
        for (int y = 0; y < image.height; y++) {
            for (int x = 0; x < image.width; x++) {
                toLab((uchar*)(image.vec.data() + y * image.width + x), (uchar*)(retval.vec.data() + y * image.width + x), image.width * image.height);
            }
        }
#ifdef HAS_OPENCL
    }
#endif
    return retval;
}

Vec3b convertColorToLab(const Vec3b& color) {
    float r, g, b, X, Y, Z, L, a, B;
    r = color[0] / 255.0; g = color[1] / 255.0; b = color[2] / 255.0;
    if (r > 0.04045) r = pow((r + 0.055) / 1.055, 2.4);
    else r = r / 12.92;
    if (g > 0.04045) g = pow((g + 0.055) / 1.055, 2.4);
    else g = g / 12.92;
    if (b > 0.04045) b = pow((b + 0.055) / 1.055, 2.4);
    else b = b / 12.92;
    r = r * 100; g = g * 100; b = b * 100;
    X = (r * 0.4124 + g * 0.3576 + b * 0.1805) / 95.047;
    Y = (r * 0.2126 + g * 0.7152 + b * 0.0722) / 100.000;
    Z = (r * 0.0193 + g * 0.1192 + b * 0.9505) / 108.883;
    if (X > 0.008856) X = cbrt(X);
    else X = (7.787 * X) + (16.0 / 116.0);
    if (Y > 0.008856) Y = cbrt(Y);
    else Y = (7.787 * Y) + (16.0 / 116.0);
    if (Z > 0.008856) Z = cbrt(Z);
    else Z = (7.787 * Z) + (16.0 / 116.0);
    L = (116 * Y) - 16; a = 500 * (X - Y) + 128; B = 200 * (Y - Z) + 128;
    return {floor(L + 0.5), floor(a + 0.5), floor(B + 0.5)};
}

std::vector<Vec3b> convertLabPalette(const std::vector<Vec3b>& palette) {
    std::vector<Vec3b> pal;
    for (const Vec3b& color : palette) {
        double Y = (color[0] + 16.0) / 116.0;
        double X = (color[1] - 128.0) / 500.0 + Y, Z = Y - (color[2] - 128.0) / 200.0;
        if (Y*Y*Y > 0.008856) Y = Y*Y*Y;
        else Y = (Y - 16.0 / 116.0) / 7.787;
        if (X*X*X > 0.008856) X = X*X*X;
        else X = (X - 16.0 / 116.0) / 7.787;
        if (Z*Z*Z > 0.008856) Z = Z*Z*Z;
        else Z = (Z - 16.0 / 116.0) / 7.787;
        X *= 0.95047; Z *= 1.08883;
        double R = X *  3.2406 + Y * -1.5372 + Z * -0.4986;
        double G = X * -0.9689 + Y *  1.8758 + Z *  0.0415;
        double B = X *  0.0557 + Y * -0.2040 + Z *  1.0570;
        if (R > 0.0031308) R = 1.055 * pow(R, 1.0 / 2.4) - 0.055;
        else R = 12.92 * R;
        if (G > 0.0031308) G = 1.055 * pow(G, 1.0 / 2.4) - 0.055;
        else G = 12.92 * G;
        if (B > 0.0031308) B = 1.055 * pow(B, 1.0 / 2.4) - 0.055;
        else B = 12.92 * B;
        if (R < 0) R = 0;
        if (R > 255) R = 255;
        if (G < 0) G = 0;
        if (G > 255) G = 255;
        if (B < 0) B = 0;
        if (B > 255) B = 255;
        pal.push_back(Vec3b{R * 255, G * 255, B * 255});
    }
    return pal;
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
        if (ranges[0] > ranges[1] && ranges[0] > ranges[2]) maxComponent = 0;
        else if (ranges[1] > ranges[2] && ranges[1] > ranges[0]) maxComponent = 1;
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

#ifdef HAS_OPENCL
static void medianCutGPUQueue(
    OpenCL::Device& device,
    OpenCL::Memory<uchar>& buffer,
    OpenCL::Memory<uchar>& components,
    OpenCL::Memory<uchar>& result,
    ulong length, ulong offset, uchar n, int numColors
) {
    ulong nparts = length / 128 + (length % 128 ? 1 : 0);
    if (n >= numColors) {
        // Average the buckets
        OpenCL::Memory<uint> temp(device, nparts, 3, false, true);
        OpenCL::Kernel avgA(device, nparts, "averageKernel_A", buffer, temp, offset, length);
        OpenCL::Kernel avgB(device, 1, 1, "averageKernel_B", temp, result, (ulong)(n - numColors), length);
        result.enqueue_read_from_device(); // VERY IMPORTANT LINE, IT BREAKS WITHOUT THIS
        avgA.enqueue_run();
        avgB.enqueue_run();
        return;
    }
    // Find maximum range
    OpenCL::Memory<uchar> intranges_mem(device, nparts, 6, false, true);
    OpenCL::Kernel rangeA(device, nparts, "calculateRange_A", buffer, intranges_mem, length, offset);
    OpenCL::Kernel rangeB(device, 1, 1, "calculateRange_B", intranges_mem, components, nparts, n);
    rangeA.enqueue_run();
    rangeB.enqueue_run();
    // Sort entries
    // Adapted from http://www.bealto.com/gpu-sorting_parallel-bitonic-2.html
    for (size_t length_ = 1; length_ < length; length_ <<= 1) {
        long inc = length_;
        std::list<int> strategy; // vector defining the sequence of reductions
        {
            int ii = inc;
            while (ii > 0) {
                if (ii == 128 || ii == 32 || ii == 8) {
                    strategy.push_back(-1);
                    break;
                }          // C kernel
                int d = 1; // default is 1 bit
                if (0) d = 1;
                // Force jump to 128
                else if (ii == 256) d = 1;
                else if (ii == 512 && (ALLOWB & 4)) d = 2;
                else if (ii == 1024 && (ALLOWB & 8)) d = 3;
                else if (ii == 2048 && (ALLOWB & 16)) d = 4;
                else if (ii >= 8 && (ALLOWB & 16)) d = 4;
                else if (ii >= 4 && (ALLOWB & 8)) d = 3;
                else if (ii >= 2 && (ALLOWB & 4)) d = 2;
                else d = 1;
                strategy.push_back(d);
                ii >>= d;
            }
        }

        while (inc > 0) {
            int ninc = 0;
            std::string kid;
            int doLocal = 0;
            int nThreads = 0;
            int d = strategy.front();
            strategy.pop_front();

            switch (d) {
            case -1:
                kid = PARALLEL_BITONIC_C4_KERNEL;
                ninc = -1; // reduce all bits
                doLocal = 4;
                nThreads = length >> 2;
                break;
            case 4:
                kid = PARALLEL_BITONIC_B16_KERNEL;
                ninc = 4;
                nThreads = length >> ninc;
                break;
            case 3:
                kid = PARALLEL_BITONIC_B8_KERNEL;
                ninc = 3;
                nThreads = length >> ninc;
                break;
            case 2:
                kid = PARALLEL_BITONIC_B4_KERNEL;
                ninc = 2;
                nThreads = length >> ninc;
                break;
            case 1:
                kid = PARALLEL_BITONIC_B2_KERNEL;
                ninc = 1;
                nThreads = length >> ninc;
                break;
            default: printf("Strategy error!\n"); break;
            }
            OpenCL::Kernel kern(device, nThreads, kid, buffer, (int)inc, (int)(length_ << 1), components, n, offset);
            int wg = kern.get_max_workgroup_size(device);
            wg = std::min(wg, 256);
            wg = std::min(wg, nThreads);
            kern.set_ranges(nThreads, wg);
            if (doLocal > 0) kern.add_parameters(OpenCL::LocalMemory<uchar>(doLocal * wg * 3));
            kern.enqueue_run();
            device.get_cl_queue().enqueueBarrierWithWaitList();
            if (ninc < 0) break; // done
            inc >>= ninc;
        }
    }
    // Recurse
    medianCutGPUQueue(device, buffer, components, result, length / 2, offset, n << 1, numColors);
    medianCutGPUQueue(device, buffer, components, result, length / 2, offset + length / 2, (n << 1) | 1, numColors);
}
#endif

std::vector<Vec3b> reducePalette_medianCut(Mat& image, int numColors, OpenCL::Device * device) {
    int e = 0;
    if (frexp(numColors, &e) > 0.5) throw std::invalid_argument("color count must be a power of 2");
    std::vector<Vec3b> newpal(numColors);
#ifdef HAS_OPENCL
    if (device != NULL) {
        image.upload();
        size_t sz = image.width * image.height;
        bool diffuse = false;
        if (sz & (sz - 1)) {
            diffuse = true;
            sz = 1 << ((int)log2(sz) + 1);
        }
        OpenCL::Memory<uchar> buffer(*device, sz, 3, false, true);
        OpenCL::Memory<uchar> components(*device, numColors, 3, false, true);
        OpenCL::Memory<uchar> pal(*device, numColors, 3);
        device->get_cl_queue().enqueueFillBuffer<uchar>(components.get_cl_buffer(), 255, 0, 1);
        device->get_cl_queue().enqueueCopyBuffer(image.mem->get_cl_buffer(), buffer.get_cl_buffer(), 0, 0, image.width * image.height * 3);
        if (diffuse) {
            float step = (float)sz / (image.width * image.height);
            OpenCL::Kernel copy(*device, sz - (image.width * image.height), "diffuseKernel", buffer, (ulong)(image.width * image.height), (ulong)sz, step);
            copy.enqueue_run();
        }
        device->get_cl_queue().enqueueBarrierWithWaitList();
        medianCutGPUQueue(*device, buffer, components, pal, sz, 0, 1, numColors);
        pal.enqueue_read_from_device();
        device->finish_queue();
        for (int i = 0; i < numColors; i++) newpal[i] = {pal[i*3], pal[i*3+1], pal[i*3+2]};
    } else {
#endif
        image.download();
        std::vector<Vec3b> pal;
        for (int y = 0; y < image.height; y++)
            for (int x = 0; x < image.width; x++)
                pal.push_back(Vec3b(image.at(y, x)));
        std::vector<Vec3b> uniq(pal);
        uniq.erase(std::unique(uniq.begin(), uniq.end()), uniq.end());
        if (numColors >= uniq.size()) return uniq;
        medianCut(pal, numColors, -1, newpal.begin());
        work.wait();
#ifdef HAS_OPENCL
    }
#endif
    // Move the darkest color to 15, and the lightest to 0
    // This fixes some background issues & makes subtitles a bit simpler
    std::vector<Vec3b>::iterator darkest = newpal.begin(), lightest = newpal.begin();
    for (auto it = newpal.begin(); it != newpal.end(); it++) {
        if ((int)(*it)[0] + (int)(*it)[1] + (int)(*it)[2] < (int)(*darkest)[0] + (int)(*darkest)[1] + (int)(*darkest)[2]) darkest = it;
        if ((int)(*it)[0] + (int)(*it)[1] + (int)(*it)[2] > (int)(*lightest)[0] + (int)(*lightest)[1] + (int)(*lightest)[2]) lightest = it;
    }
    Vec3b d = *darkest, l = *lightest;
    if (darkest == lightest) {
        // All colors are the same, add extra white for subtitles
        newpal.erase(darkest);
        newpal.pop_back();
        //l = {255, 255, 255};
    } else if (darkest > lightest) {
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
    for (const Vec3d* c : (*state->colors)[state->i].second) {
        sum += *c;
        size++;
    }
    if (size == 0) return;
    sum = sum / size;
    if (Vec3b(sum) != Vec3b((*state->colors)[state->i].first)) *state->changed = true;
    (*state->newColors)[state->i].first = sum;
    (*state->newColors)[state->i].second.clear();
}

std::vector<Vec3b> reducePalette_kMeans(Mat& image, int numColors, OpenCL::Device * device) {
    std::vector<Vec3b> newpal(numColors);
#ifdef HAS_OPENCL
    if (device != NULL) {
        ulong nparts = (image.width * image.height) / 128 + ((image.width * image.height) % 128 ? 1 : 0);
        std::vector<Vec3b> basepal = reducePalette_medianCut(image, 16, device);
        OpenCL::Memory<uchar> palette(*device, numColors, 3);
        OpenCL::Memory<uchar> buckets(*device, image.width * image.height, 1, false, true);
        OpenCL::Memory<uint> avgbuf(*device, nparts * numColors, 4, false, true);
        OpenCL::Memory<uchar> changed(*device, 1, 1);
        OpenCL::Kernel bucket(*device, image.width * image.height, "kMeans_bucket_kernel", *image.mem, buckets, palette, (ulong)numColors);
        OpenCL::Kernel recenterA(*device, nparts * numColors, numColors, "kMeans_recenter_kernel_A", *image.mem, buckets, avgbuf, (ulong)(image.width * image.height), OpenCL::LocalMemory<uchar>(128));
        OpenCL::Kernel recenterB(*device, numColors, numColors, "kMeans_recenter_kernel_B", avgbuf, palette, nparts, changed);
        image.upload();
        for (int i = 0; i < numColors; i++) {
            palette[i*3] = basepal[i][0];
            palette[i*3+1] = basepal[i][1];
            palette[i*3+2] = basepal[i][2];
        }
        palette.enqueue_write_to_device();
        int loop = 0;
        do {
            changed[0] = 0;
            changed.enqueue_write_to_device();
            bucket.enqueue_run();
            recenterA.enqueue_run();
            recenterB.enqueue_run();
            changed.enqueue_read_from_device();
            device->finish_queue();
        } while (changed[0] && loop++ < 100);
        palette.read_from_device();
        for (int i = 0; i < numColors; i++) newpal[i] = {palette[i*3], palette[i*3+1], palette[i*3+2]};
    } else {
#endif
        Vec3d * originalColors = new Vec3d[image.width * image.height];
        std::vector<std::pair<Vec3d, std::vector<const Vec3d*>>> colorsA, colorsB;
        std::vector<std::pair<Vec3d, std::vector<const Vec3d*>>> *colors = &colorsA, *newColors = &colorsB;
        std::vector<kmeans_state> states(numColors);
        std::vector<std::mutex*> locks;
        bool changed = true;
        // get initial centroids
        // TODO: optimize
        std::vector<Vec3b> med = reducePalette_medianCut(image, 16, device);
        for (int i = 0; i < numColors; i++) colors->push_back(std::make_pair(med[i], std::vector<const Vec3d*>()));
        image.download();
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
            Vec3d sum {0, 0, 0};
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
        for (int i = 0; i < numColors; i++) {
            newpal[i] = Vec3b((*newColors)[i].first);
            delete locks[i];
        }
        delete[] originalColors;
#ifdef HAS_OPENCL
    }
#endif
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

Vec3b nearestColor(const std::vector<Vec3b>& palette, const Vec3d& color, int* _n) {
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

Mat thresholdImage(Mat& image, const std::vector<Vec3b>& palette, OpenCL::Device * device) {
    Mat output(image.width, image.height, device);
#ifdef HAS_OPENCL
    if (device != NULL) {
        uchar pal[48];
        for (int i = 0; i < palette.size(); i++) {pal[i*3] = palette[i][0]; pal[i*3+1] = palette[i][1]; pal[i*3+2] = palette[i][2];}
        OpenCL::Memory<uchar> palette_mem(*device, 48, 1, pal);
        palette_mem.write_to_device();
        image.upload();
        OpenCL::Kernel kernel(*device, image.width * image.height, "thresholdKernel", *image.mem, *output.mem, palette_mem, (uchar)palette.size());
        kernel.run();
        output.onHost = false;
        output.onDevice = true;
    } else {
#endif
        image.download();
        output.onDevice = false;
        for (int i = 0; i < image.height; i++)
            work.push(thresholdImage_worker, new threshold_state {i, &image, &output, &palette});
        work.wait();
#ifdef HAS_OPENCL
    }
#endif
    return output;
}

Mat ditherImage(Mat& image, const std::vector<Vec3b>& palette, OpenCL::Device * device) {
    Mat retval(image.width, image.height, device);
#ifdef HAS_OPENCL
    if (device != NULL && false) {
        ulong progress_size = image.height / WORKGROUP_SIZE + (image.height % WORKGROUP_SIZE ? 1 : 0);
        OpenCL::Memory<uchar> palette_mem(*device, palette.size(), 3);
        for (int i = 0; i < palette.size(); i++) {palette_mem[i*3] = palette[i][0]; palette_mem[i*3+1] = palette[i][1]; palette_mem[i*3+2] = palette[i][2];}
        OpenCL::Memory<float> error(*device, image.width * (image.height + 1) * 3, 1, false, true);
        OpenCL::Memory<uint> workgroup_rider(*device, 1, 1, false, true);
        OpenCL::Memory<uint> workgroup_progress(*device, progress_size, 1, false, true);
        OpenCL::LocalMemory<uint> progress(WORKGROUP_SIZE);
        device->get_cl_queue().enqueueFillBuffer<float>(error.get_cl_buffer(), 0.0f, 0, image.width * (image.height + 1) * 3 * sizeof(float));
        palette_mem.enqueue_write_to_device();
        image.upload();
        OpenCL::Kernel kernel(
            *device, image.height, "floydSteinbergDither",
            *image.mem,
            *retval.mem,
            palette_mem,
            (uchar)palette.size(),
            error,
            workgroup_rider,
            workgroup_progress,
            progress,
            (ulong)image.width, (ulong)image.height);
        kernel.run();
        retval.onHost = false;
        retval.onDevice = true;
    } else {
#endif
        image.download();
        retval.onDevice = false;
        std::vector<Vec3d> error(image.width);
        for (int y = 0; y < image.height; y++) {
            std::vector<Vec3d> newerror(image.width);
            for (int x = 0; x < image.width; x++) {
                Vec3d c = Vec3d(image.at(y, x)) + error[x];
                Vec3b newpixel = nearestColor(palette, c);
                retval.at(y, x) = newpixel;
                Vec3d err = c - Vec3d(newpixel);
                if (x < image.width - 1) {
                    error[x + 1] += err * (5.0/16.0);
                    newerror[x + 1] += err * (1.0/16.0);
                }
                if (x > 0) newerror[x - 1] += err * (2.0/16.0);
                newerror[x] += err * (3.0/16.0);
            }
            error = newerror;
        }
#ifdef HAS_OPENCL
    }
#endif
    return retval;
}

static const int thresholdMap[8][8] = {
    { 0, 32,  8, 40,  2, 34, 10, 42},
    {48, 16, 56, 24, 50, 18, 58, 26},
    {12, 44,  4, 36, 14, 46,  6, 38},
    {60, 28, 52, 20, 62, 30, 54, 22},
    { 3, 35, 11, 43,  1, 33,  9, 41},
    {51, 19, 59, 27, 49, 17, 57, 25},
    {15, 47,  7, 39, 13, 45,  5, 37},
    {63, 31, 55, 23, 61, 29, 53, 21}
};

static double colorDistance(const Vec3b& a, const Vec3b& b) {
    Vec3b c = {b[0] > a[0] ? b[0] - a[0] : a[0] - b[0], b[1] > a[1] ? b[1] - a[1] : a[1] - b[1], b[2] > a[2] ? b[2] - a[2] : a[2] - b[2]};
    return sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
}

Mat ditherImage_ordered(Mat& image, const std::vector<Vec3b>& palette, OpenCL::Device * device) {
    Mat retval(image.width, image.height, device);
    double distance = 0;
    for (const Vec3b& a : palette)
        for (const Vec3b& b : palette)
            distance += colorDistance(a, b);
    distance /= palette.size() * palette.size() * 6;
#ifdef HAS_OPENCL
    if (device != NULL) {
        uchar pal[48];
        for (int i = 0; i < palette.size(); i++) {pal[i*3] = palette[i][0]; pal[i*3+1] = palette[i][1]; pal[i*3+2] = palette[i][2];}
        OpenCL::Memory<uchar> palette_mem(*device, 48, 1, pal);
        palette_mem.write_to_device();
        image.upload();
        OpenCL::Kernel kernel(*device, image.width * image.height, "orderedDither", *image.mem, *retval.mem, palette_mem, (uchar)palette.size(), (ulong)image.width, distance);
        kernel.run();
        retval.onHost = false;
        retval.onDevice = true;
    } else {
#endif
        image.download();
        retval.onDevice = false;
        for (int y = 0; y < image.height; y++) {
            for (int x = 0; x < image.width; x++) {
                Vec3d c = Vec3d(image.at(y, x)) + distance * (thresholdMap[y % 8][x % 8] / 64.0 - 0.5);
                Vec3b newpixel = nearestColor(palette, c);
                retval.at(y, x) = newpixel;
            }
        }
#ifdef HAS_OPENCL
    }
#endif
    return retval;
}

Mat1b rgbToPaletteImage(Mat& image, const std::vector<Vec3b>& palette, OpenCL::Device * device) {
    Mat1b output(image.width, image.height, device);
#ifdef HAS_OPENCL
    if (device != NULL) {
        uchar pal[48];
        for (int i = 0; i < palette.size(); i++) {pal[i*3] = palette[i][0]; pal[i*3+1] = palette[i][1]; pal[i*3+2] = palette[i][2];}
        OpenCL::Memory<uchar> palette_mem(*device, 48, 1, pal);
        palette_mem.write_to_device();
        image.upload();
        OpenCL::Kernel kernel(*device, image.width * image.height, "rgbToPaletteKernel", *image.mem, *output.mem, palette_mem, (uchar)palette.size(), (ulong)(image.width * image.height));
        kernel.run();
        output.onHost = false;
        output.onDevice = true;
    } else {
#endif
        image.download();
        output.onDevice = false;
        for (int y = 0; y < image.height; y++)
            for (int x = 0; x < image.width; x++)
                output.at(y, x) = std::find(palette.begin(), palette.end(), Vec3b(image.at(y, x))) - palette.begin();
#ifdef HAS_OPENCL
    }
#endif
    return output;
}
