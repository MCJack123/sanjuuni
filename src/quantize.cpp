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

std::vector<Vec3b> reducePalette_medianCut(const Mat& image, int numColors) {
    int e = 0;
    if (frexp(numColors, &e) > 0.5) throw std::invalid_argument("color count must be a power of 2");
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

std::vector<Vec3b> reducePalette_kMeans(const Mat& image, int numColors) {
    Vec3d * originalColors = new Vec3d[image.width * image.height];
    std::vector<std::pair<Vec3d, std::vector<const Vec3d*>>> colorsA, colorsB;
    std::vector<std::pair<Vec3d, std::vector<const Vec3d*>>> *colors = &colorsA, *newColors = &colorsB;
    std::vector<kmeans_state> states(numColors);
    std::vector<std::mutex*> locks;
    bool changed = true;
    // get initial centroids
    // TODO: optimize
    std::vector<Vec3b> med = reducePalette_medianCut(image, numColors);
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

Mat thresholdImage(const Mat& image, const std::vector<Vec3b>& palette) {
    Mat output(image.width, image.height);
    for (int i = 0; i < image.height; i++)
        work.push(thresholdImage_worker, new threshold_state {i, &image, &output, &palette});
    work.wait();
    return output;
}

Mat ditherImage(const Mat& image, const std::vector<Vec3b>& palette) {
    vector2d<Vec3d> errmap(image.width, image.height);
    Mat retval(image.width, image.height);
    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            Vec3d c = Vec3d(image.at(y, x)) + errmap.at(y, x);
            Vec3b newpixel = nearestColor(palette, c);
            retval.at(y, x) = newpixel;
            Vec3d err = c - Vec3d(newpixel);
            if (x < image.width - 1) errmap.at(y, x + 1) += err * (7.0/16.0);
            if (y < image.height - 1) {
                if (x > 1) errmap.at(y + 1, x - 1) += err * (3.0/16.0);
                errmap.at(y + 1, x) += err * (5.0/16.0);
                if (x < image.width - 1) errmap.at(y + 1, x + 1) += err * (1.0/16.0);
            }
        }
    }
    return retval;
}

Mat1b rgbToPaletteImage(const Mat& image, const std::vector<Vec3b>& palette) {
    Mat1b output(image.width, image.height);
    for (int y = 0; y < image.height; y++)
        for (int x = 0; x < image.width; x++)
            output.at(y, x) = std::find(palette.begin(), palette.end(), image.at(y, x)) - palette.begin();
    return output;
}
