/*
Octree quantization

Copyright (c) 2006 Michal Molhanec

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented;
     you must not claim that you wrote the original software.
     If you use this software in a product, an acknowledgment
     in the product documentation would be appreciated but
     is not required.

  2. Altered source versions must be plainly marked as such,
     and must not be misrepresented as being the original software.

  3. This notice may not be removed or altered from any
     source distribution.
*/

/*

octree.cpp -- Octree quantization implementation.
Modified by JackMacWindows for sanjuuni.

*/

#include "sanjuuni.hpp"

struct octree_node {
    uint32_t r, g, b;
    uint32_t counter;
    int leaf;
    int leaf_parent;
    struct octree_node* subnodes[8];
    int palette_entry;
    struct octree_node* prev;
    struct octree_node* next;
    struct octree_node* parent;
};

struct octree_tree {
    struct octree_node* root;
    uint32_t number_of_leaves;
    struct octree_node* leaves_parents;
};

#define BITS_USED 8

static struct octree_node* octree_create_node(struct octree_node* parent) {
    struct octree_node* n = (struct octree_node*) calloc(1, sizeof(struct octree_node));
    if (n) {
        n->parent = parent;
    }
    return n;
}

/* 0 on error */
static int octree_insert_pixel(struct octree_tree* tree, int r, int g, int b) {
    int mask;
    int r_bit, g_bit, b_bit;
    int index;
    int i;
    struct octree_node* node = tree->root;
    for (i = BITS_USED; i >= 0; i--) {
        mask =  1 << i;
        r_bit = (r & mask) >> i;
        g_bit = (g & mask) >> i;
        b_bit = (b & mask) >> i;
        index = (r_bit << 2) + (g_bit << 1) + b_bit;
        if (!node->subnodes[index]) {
            node->subnodes[index] = octree_create_node(node);
            if (!node->subnodes[index]) {
                return 0;
            }
        }
        node = node->subnodes[index];
    }
    if (node->counter == 0) {
        tree->number_of_leaves++;
        node->leaf = 1;
        if (!node->parent->leaf_parent) {
            node->parent->leaf_parent = 1;
            if (tree->leaves_parents) {
                tree->leaves_parents->prev = node->parent;
            }
            node->parent->next = tree->leaves_parents;
            tree->leaves_parents = node->parent;
        }
    }
    node->counter++;
    node->r += r;
    node->g += g;
    node->b += b;
    return 1;
}

static uint32_t octree_calc_counters(struct octree_node* node) {
    int i;
    if (node->leaf) {
        return node->counter;
    }
    for (i = 0; i < 8; i++) {
        if (node->subnodes[i]) {
            node->counter += octree_calc_counters(node->subnodes[i]);
        }
    }
    return node->counter;
}

static struct octree_node* octree_find_smallest(struct octree_tree* tree, uint32_t* last_min) {
    struct octree_node* min = tree->leaves_parents;
    struct octree_node* n = tree->leaves_parents->next;

    while (n != 0) {
        if (min->counter == *last_min) {
            return min;
        }
        if (n->counter < min->counter) {
            min = n;
        }
        n = n->next;
    }
    *last_min = min->counter;
    return min;
}

static void octree_reduce(struct octree_tree* tree) {
    struct octree_node* n;
    uint32_t min = 1;
    int i;
    if (tree->number_of_leaves <= 16) {
        return;
    }
    octree_calc_counters(tree->root);
    while (tree->number_of_leaves > 16) {
        n = octree_find_smallest(tree, &min);
        for (i = 0; i < 8; i++) {
            if (n->subnodes[i]) {
                n->r += n->subnodes[i]->r;
                n->g += n->subnodes[i]->g;
                n->b += n->subnodes[i]->b;
                free(n->subnodes[i]);
                n->subnodes[i] = 0;
                tree->number_of_leaves--;
            }
        }
        tree->number_of_leaves++;
        n->leaf = 1;
        if (!n->parent->leaf_parent) {
            n->parent->leaf_parent = 1;
            n->parent->next = n->next;
            n->parent->prev = n->prev;
            if (n->prev) {
                n->prev->next = n->parent;
            } else {
                tree->leaves_parents = n->parent;
            }
            if (n->next) {
                n->next->prev = n->parent;
            }
        } else {
            if (n->prev) {
                n->prev->next = n->next;
            } else {
                tree->leaves_parents = n->next;
            }
            if (n->next) {
                n->next->prev = n->prev;
            }
        }
    }
}

static void octree_fill_palette(std::vector<Vec3b>& pal, int* index, struct octree_tree* tree) {
    int i;
    struct octree_node* n = tree->leaves_parents;
    while (n) {
        for (i = 0; i < 8; i++) {
            if (n->subnodes[i] && n->subnodes[i]->leaf) {
                pal[*index][0] = n->subnodes[i]->r / n->subnodes[i]->counter;
                pal[*index][1] = n->subnodes[i]->g / n->subnodes[i]->counter;
                pal[*index][2] = n->subnodes[i]->b / n->subnodes[i]->counter;
                n->subnodes[i]->palette_entry = (*index)++;
            }
        }
        n = n->next;
    }
}

static int octree_get_index(struct octree_node* node, int r, int g, int b, int i) {
    int mask, index;
    int r_bit, g_bit, b_bit;
restart:
    if (node->leaf) {
        return node->palette_entry;
    }
    mask =  1 << i;
    r_bit = (r & mask) >> i;
    g_bit = (g & mask) >> i;
    b_bit = (b & mask) >> i;
    index = (r_bit << 2) + (g_bit << 1) + b_bit;
    i--;
    node = node->subnodes[index];
    goto restart;
}

static void octree_free_node(struct octree_node* node) {
    int i;
    for (i = 0; i < 8; i++) {
        if (node->subnodes[i]) {
            octree_free_node(node->subnodes[i]);
        }
    }
    free(node);
}

std::vector<Vec3b> reducePalette_octree(Mat& bmp, int numColors, OpenCL::Device * device) {
    int x, y;
    int i;
    int bpp;
    int r, g, b;
    std::vector<Vec3b> pal(numColors);
    octree_tree _tree;
    octree_tree * tree = &_tree;

    bmp.download();
    tree->number_of_leaves = 0;
    tree->root = octree_create_node(0);
    tree->leaves_parents = 0;
    if (!tree->root) {
        return {};
    }

    for (y = 0; y < bmp.height; y++) {
        for (x = 0; x < bmp.width; x++) {
            if (!octree_insert_pixel(tree, bmp[y][x].x, bmp[y][x].y, bmp[y][x].z)) {
                octree_free_node(tree->root);
                return {};
            }
        }
    }
    
    octree_reduce(tree);
    if (tree->number_of_leaves == numColors) {
        i = 0;
    } else {
        i = 1;
        /* If there is space, left color with index 0 black */
        pal[0][0] = pal[0][1] = pal[0][2] = 0;
    }
    octree_fill_palette(pal, &i, tree);
    while (i < numColors) {
        pal[i][0] = pal[i][0] = pal[i][0] = 0;
        i++;
    }

    octree_free_node(tree->root);

    return pal;
}
