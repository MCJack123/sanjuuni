/*
 * compute.hpp
 * Generic interface for compute backends.
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

#ifndef SANJUUNI_COMPUTE_HPP
#define SANJUUNI_COMPUTE_HPP

#include <string>
#include <memory>

#define WORKGROUP_SIZE 64 // needs to be 64 to fully use AMD GPUs

template<typename T>
class ComputeMemory {
protected:
    inline ComputeMemory() {}
public:
    virtual ~ComputeMemory() = 0;
    virtual ComputeMemory& operator=(ComputeMemory&& memory) noexcept = 0;
    virtual const unsigned long length() const = 0;
    virtual const unsigned int dimensions() const = 0;
    virtual const unsigned long range() const = 0;
    virtual const unsigned long capacity() const = 0;
    virtual void read_from_device(const bool blocking=true) = 0;
    virtual void write_to_device(const bool blocking=true) = 0;
    virtual void enqueue_read_from_device() = 0;
    virtual void enqueue_write_to_device() = 0;
    virtual void enqueue_copy_buffer(const ComputeMemory<T> * source, unsigned long source_offset, unsigned long dest_offset, unsigned long size) = 0;
    virtual void finish_queue() = 0;
    virtual void reset(const T value=(T)0) = 0;
    virtual T* const data() = 0;
    virtual const T* const data() const = 0;
    virtual T& operator[](const unsigned long i) = 0;
    virtual const T& operator[](const unsigned long i) const = 0;

    template<typename T>
    void enqueue_fill_buffer(const T& value, const unsigned long offset, const unsigned long size);
};

template<typename T>
class ComputeLocalMemory {
private:
    unsigned long N = 0ll;
public:
    ComputeLocalMemory(unsigned long sz): N(sz) {}
    ulong get_size() const {return N * sizeof(T);}
};

class ComputeKernel {
protected:
    inline ComputeKernel() {}
public:
    virtual ComputeKernel& set_ranges(const unsigned long N, const unsigned long workgroup_size=(unsigned long)WORKGROUP_SIZE) = 0;
    virtual const ulong range() const = 0;
    virtual unsigned int get_number_of_parameters() const = 0;
    virtual unsigned int get_max_workgroup_size(const ComputeDevice * device) = 0;
    virtual ComputeKernel& enqueue_run(const unsigned int t=1u, const unsigned int o=0u) = 0;
    virtual ComputeKernel& finish_queue() = 0;
    virtual ComputeKernel& run(const unsigned int t=1u) = 0;
    template<class... T> ComputeKernel& add_parameters(const T&... parameters);
};

class ComputeDevice {
protected:
    inline ComputeDevice() {}
public:
    virtual ~ComputeDevice() = 0;
    virtual void finish_queue() = 0;
    virtual bool is_initialized() const = 0;
    virtual void enqueue_barrier_with_wait_list() = 0;

    template<typename T>
    std::unique_ptr<ComputeMemory<T>> create_memory(const unsigned long N, const unsigned int dimensions=1u, const bool allocate_host=true, const bool allocate_device=true, const T value=(T)0);
    template<typename T>
    std::unique_ptr<ComputeMemory<T>> create_memory(const unsigned long N, const unsigned int dimensions, T* const host_buffer, const bool allocate_device=true);
    template<class... T>
    std::unique_ptr<ComputeKernel> create_kernel(const unsigned long N, const std::string& name, const T&... parameters);
    template<class... T>
    std::unique_ptr<ComputeKernel> create_kernel(const unsigned long N, const unsigned int workgroup_size, const std::string& name, const T&... parameters);
};

#ifdef HAS_OPENCL
#include "opencl.hpp"
#endif
#ifdef HAS_OPENGL
#include "opengl.hpp"
#endif

#define TRY_TYPE(typ, code) { typ * this_cast = dynamic_cast<typ*>(this); if (this_cast != nullptr) code }

template<typename T>
std::unique_ptr<ComputeMemory<T>> ComputeDevice::create_memory(const unsigned long N, const unsigned int dimensions=1u, const bool allocate_host=true, const bool allocate_device=true, const T value=(T)0) {
#ifdef HAS_OPENCL
    TRY_TYPE(OpenCL::Device, return OpenCL::Memory<T>(*this_cast, N, dimensions, allocate_host, allocate_device, value);)
#endif
    return nullptr;
}

template<typename T>
std::unique_ptr<ComputeMemory<T>> ComputeDevice::create_memory(const unsigned long N, const unsigned int dimensions, T* const host_buffer, const bool allocate_device=true) {
#ifdef HAS_OPENCL
    TRY_TYPE(OpenCL::Device, return OpenCL::Memory<T>(*this_cast, N, dimensions, host_buffer, allocate_device);)
#endif
    return nullptr;
}

template<class... T>
std::unique_ptr<ComputeKernel> ComputeDevice::create_kernel(const ulong N, const std::string& name, const T&... parameters) {
#ifdef HAS_OPENCL
    TRY_TYPE(OpenCL::Device, return OpenCL::Kernel(*this_cast, N, name, parameters...);)
#endif
    return nullptr;
}

template<class... T>
std::unique_ptr<ComputeKernel> ComputeDevice::create_kernel(const ulong N, const uint workgroup_size, const std::string& name, const T&... parameters) {
#ifdef HAS_OPENCL
    TRY_TYPE(OpenCL::Device, return OpenCL::Kernel(*this_cast, N, workgroup_size, name, parameters...);)
#endif
    return nullptr;
}

template<typename T>
void ComputeMemory<T>::enqueue_fill_buffer(const T& value, const unsigned long offset, const unsigned long size) {
#ifdef HAS_OPENCL
    TRY_TYPE(OpenCL::Device, this_cast->enqueue_fill_buffer_impl<T>(value, offset, size))
#endif
}

template<class... T>
ComputeKernel& add_parameters(const T&... parameters) {
#ifdef HAS_OPENCL
    TRY_TYPE(OpenCL::Kernel, this_cast->add_parameters_impl(parameters...))
#endif
    return *this;
}

#endif