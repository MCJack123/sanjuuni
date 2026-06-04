/*
 * opengl.hpp
 * OpenGL-based compute backend.
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

#ifndef SANJUUNI_OPENGL_HPP
#define SANJUUNI_OPENGL_HPP

#include "compute.hpp"
extern "C" {
#include <GL/gl.h>
}

template<typename T>
class GLMemory: public ComputeMemory<T> {
public:
    virtual inline const unsigned long length() const {
    
    }

    virtual inline const unsigned int dimensions() const {
    
    }

    virtual inline const unsigned long range() const {
    
    }

    virtual inline const unsigned long capacity() const {
    
    }

    virtual inline void read_from_device(const bool blocking=true) {
    
    }

    virtual inline void write_to_device(const bool blocking=true) {
    
    }

    virtual inline void enqueue_read_from_device() {
    
    }

    virtual inline void enqueue_write_to_device() {
    
    }

    virtual inline void enqueue_fill_buffer(const T& value, const unsigned long offset, const unsigned long size) {

    }

    virtual inline void enqueue_copy_buffer(const ComputeMemory<T> * source, unsigned long source_offset, unsigned long dest_offset, unsigned long size) {
    
    }

    virtual inline void finish_queue() {
    
    }

    virtual inline void reset(const T value=(T)0) {
    
    }

    virtual inline T* const data() {
    
    }

    virtual inline const T* const data() const {
    
    }

    virtual inline T& operator[](const unsigned long i) {
    
    }

    virtual inline const T& operator[](const unsigned long i) const {
    
    }

};

class GLKernel: public ComputeKernel {
public:
    virtual inline ComputeKernel& set_ranges(const unsigned long N, const unsigned long workgroup_size=(unsigned long)WORKGROUP_SIZE) {
    
    }

    virtual inline const ulong range() const {
    
    }

    virtual inline unsigned int get_number_of_parameters() const {
    
    }

    virtual inline unsigned int get_max_workgroup_size() {
    
    }

    virtual inline ComputeKernel& enqueue_run(const unsigned int t=1u, const unsigned int o=0u) {
    
    }

    virtual inline ComputeKernel& finish_queue() {
    
    }

    virtual inline ComputeKernel& run(const unsigned int t=1u) {
    
    }

};

class GLDevice: public ComputeDevice {
public:
    virtual inline void finish_queue() {
    
    }

    virtual inline bool is_initialized() const {
    
    }

    virtual inline void enqueue_barrier_with_wait_list() {
    
    }

};

#endif