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

extern void toLab(const uchar * image, uchar * output);

Mat makeLabImage(Mat& image, OpenCL::Device * device) {
    Mat retval(image.width, image.height, device);
#ifdef HAS_OPENCL
    if (device != NULL) {
        image.upload();
        OpenCL::Kernel kernel(*device, image.width * image.height, "toLab", *image.mem, *retval.mem);
        kernel.run();
        retval.onHost = false;
        retval.onDevice = true;
    } else {
#endif
        image.download();
        retval.onDevice = false;
        for (int y = 0; y < image.height; y++) {
            for (int x = 0; x < image.width; x++) {
                toLab((uchar*)(image.vec.data() + y * image.width + x), (uchar*)(retval.vec.data() + y * image.width + x));
            }
        }
#ifdef HAS_OPENCL
    }
#endif
    return retval;
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

std::vector<Vec3b> reducePalette_medianCut(Mat& image, int numColors, OpenCL::Device * device) {
    int e = 0;
    if (frexp(numColors, &e) > 0.5) throw std::invalid_argument("color count must be a power of 2");
    image.download();
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

std::vector<Vec3b> reducePalette_kMeans(Mat& image, int numColors, OpenCL::Device * device) {
    Vec3d * originalColors = new Vec3d[image.width * image.height];
    std::vector<std::pair<Vec3d, std::vector<const Vec3d*>>> colorsA, colorsB;
    std::vector<std::pair<Vec3d, std::vector<const Vec3d*>>> *colors = &colorsA, *newColors = &colorsB;
    std::vector<kmeans_state> states(numColors);
    std::vector<std::mutex*> locks;
    bool changed = true;
    // get initial centroids
    // TODO: optimize
    image.download();
    std::vector<Vec3b> med = reducePalette_medianCut(image, numColors, device);
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
    std::vector<Vec3b> retval;
    for (int i = 0; i < numColors; i++) {
        retval.push_back(Vec3b((*newColors)[i].first));
        delete locks[i];
    }
    delete[] originalColors;
    return retval;
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
    /*if (device != NULL) {
        // Apparently this is really slow. I don't want to delete it though, so it stays. It took me too much effort to get working.
        uchar pal[48];
        for (int i = 0; i < palette.size(); i++) {pal[i*3] = palette[i][0]; pal[i*3+1] = palette[i][1]; pal[i*3+2] = palette[i][2];}
        OpenCL::Memory<uchar> palette_mem(*device, 48, 1, pal);
        OpenCL::Memory<float> error(*device, image.width * 3);
        OpenCL::Memory<float> newerror(*device, image.width * 3);
        image.upload();
        OpenCL::Kernel kernel(*device, 1, "floydSteinbergDither", *image.mem, *retval.mem, palette_mem, (uchar)palette.size(), error, newerror, (ulong)image.width);
        for (int y = 0; y < image.height; y++) {
            kernel.enqueue_run(1, y);
            device->get_cl_queue().enqueueCopyBuffer(newerror.get_cl_buffer(), error.get_cl_buffer(), 0, 0, image.width * 3 * sizeof(float));
            device->get_cl_queue().enqueueFillBuffer<float>(newerror.get_cl_buffer(), 0.0f, 0, image.width * 3 * sizeof(float));
        }
        kernel.finish_queue();
        retval.onHost = false;
        retval.onDevice = true;
    } else {*/
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
    //}
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
        image.upload();
        OpenCL::Kernel kernel(*device, image.width * image.height, "rgbToPaletteKernel", *image.mem, *output.mem, palette_mem, (uchar)palette.size());
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
