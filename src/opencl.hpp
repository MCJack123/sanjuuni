/*
 * Copyright (c) 2022-2023 Moritz Lehmann
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files, to use this software for
 * educational use, non-military research or non-military commercial use, and to
 * alter it and redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not claim
 *    that you wrote the original software. If you use this software in a product,
 *    an acknowledgment in the product documentation should be provided.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * This has been modified for sanjuuni to have better code practices.
 */

#ifndef OPENCL_HPP
#define OPENCL_HPP

#define WORKGROUP_SIZE 64 // needs to be 64 to fully use AMD GPUs
#define CL_HPP_TARGET_OPENCL_VERSION 300
//#define PTX
//#define LOG

#include <cmath>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <CL/opencl.hpp> // OpenCL 1.0, 1.1, 1.2

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
#ifndef QT_BEGIN_INCLUDE_NAMESPACE
typedef int64_t slong;
typedef uint64_t ulong;
#endif

namespace OpenCL {

template<typename T> inline T min(T a) {return a;}
template<typename T, typename ...Args> inline T min(T a, Args... b) {return a < min(b...) ? a : min(b...);}
template<typename T> inline T max(T a) {return a;}
template<typename T, typename ...Args> inline T max(T a, Args... b) {return a > max(b...) ? a : max(b...);}

inline std::string trim(const std::string& s) { // removes whitespace characters from beginnig and end of string s
    const int l = (int)s.length();
    int a=0, b=l-1;
    char c;
    while(a<l && ((c=s[a])==' '||c=='\t'||c=='\n'||c=='\v'||c=='\f'||c=='\r'||c=='\0')) a++;
    while(b>a && ((c=s[b])==' '||c=='\t'||c=='\n'||c=='\v'||c=='\f'||c=='\r'||c=='\0')) b--;
    return s.substr(a, 1+b-a);
}

inline bool contains(const std::string& s, const std::string& match) {
    return s.find(match)!=std::string::npos;
}

template<class T> inline bool contains(const std::vector<T>& v, const T& match) {
    return std::find(v.begin(), v.end(), match)!=v.end();
}

inline bool contains_any(const std::string& s, const std::vector<std::string>& matches) {
    for(uint i=0u; i<(uint)matches.size(); i++) if(contains(s, matches[i])) return true;
    return false;
}

inline std::string to_lower(const std::string& s) {
    std::string r = "";
    for(uint i=0u; i<(uint)s.length(); i++) {
        const uchar c = s.at(i);
        r += c>64u&&c<91u ? c+32u : c;
    }
    return r;
}

inline uint to_uint(const float x) {
    return (uint)fmax(x+0.5f, 0.5f);
}

inline uint to_uint(const double x) {
    return (uint)fmax(x+0.5, 0.5);
}

inline std::string alignl(const uint n, const std::string& x="") { // converts x to string with spaces behind such that length is n if x is not longer than n
    std::string s = x;
    for(uint i=0u; i<n; i++) s += " ";
    return s.substr(0, max(n, (uint)x.length()));
}

inline std::string alignr(const uint n, const std::string& x="") { // converts x to string with spaces in front such that length is n if x is not longer than n
    std::string s = "";
    for(uint i=0u; i<n; i++) s += " ";
    s += x;
    return s.substr((uint)min((int)s.length()-(int)n, (int)n), s.length());
}

template<typename T> inline std::string alignl(const uint n, const T x) { // converts x to string with spaces behind such that length is n if x does not have more digits than n
    return alignl(n, std::to_string(x));
}

template<typename T> inline std::string alignr(const uint n, const T x) { // converts x to string with spaces in front such that length is n if x does not have more digits than n
    return alignr(n, std::to_string(x));
}

inline void print(const std::string& s="") {
    std::cout << s;
}

inline void println(const std::string& s="") {
    std::cout << s << '\n';
}

struct OpenCLException: public std::runtime_error {
    OpenCLException(std::string what_arg): std::runtime_error(what_arg) {}
};

inline void print_error(std::string err) {throw OpenCLException(err);}

inline void print_warning(std::string msg) {std::cout << msg << "\n";}

inline void print_info(std::string msg) {}

struct Device_Info {
    cl::Device cl_device;
    std::string name, vendor; // device name, vendor
    std::string driver_version, opencl_c_version; // device driver version, OpenCL C version
    uint memory=0u; // global memory in MB
    uint memory_used=0u; // track global memory usage in MB
    uint global_cache=0u, local_cache=0u; // global cache in KB, local cache in KB
    uint max_global_buffer=0u, max_constant_buffer=0u; // maximum global buffer size in MB, maximum constant buffer size in KB
    uint compute_units=0u; // compute units (CUs) can contain multiple cores depending on the microarchitecture
    uint clock_frequency=0u; // in MHz
    bool is_cpu=false, is_gpu=false;
    uint is_fp64_capable=0u, is_fp32_capable=0u, is_fp16_capable=0u, is_int64_capable=0u, is_int32_capable=0u, is_int16_capable=0u, is_int8_capable=0u;
    uint cores=0u; // for CPUs, compute_units is the number of threads (twice the number of cores with hyperthreading)
    float tflops=0.0f; // estimated device FP32 floating point performance in TeraFLOPs/s
    inline Device_Info(const cl::Device& cl_device) {
        this->cl_device = cl_device; // see https://www.khronos.org/registry/OpenCL/sdk/1.2/docs/man/xhtml/clGetDeviceInfo.html
        name = trim(cl_device.getInfo<CL_DEVICE_NAME>()); // device name
        vendor = trim(cl_device.getInfo<CL_DEVICE_VENDOR>()); // device vendor
        driver_version = trim(cl_device.getInfo<CL_DRIVER_VERSION>()); // device driver version
        opencl_c_version = trim(cl_device.getInfo<CL_DEVICE_OPENCL_C_VERSION>()); // device OpenCL C version
        memory = (uint)(cl_device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>()/1048576ull); // global memory in MB
        global_cache = (uint)(cl_device.getInfo<CL_DEVICE_GLOBAL_MEM_CACHE_SIZE>()/1024ull); // global cache in KB
        local_cache = (uint)(cl_device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>()/1024ull); // local cache in KB
        max_global_buffer = (uint)(cl_device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>()/1048576ull); // maximum global buffer size in MB
        max_constant_buffer = (uint)(cl_device.getInfo<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE>()/1024ull); // maximum constant buffer size in KB
        compute_units = (uint)cl_device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>(); // compute units (CUs) can contain multiple cores depending on the microarchitecture
        clock_frequency = (uint)cl_device.getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>(); // in MHz
        is_fp64_capable = (uint)cl_device.getInfo<CL_DEVICE_NATIVE_VECTOR_WIDTH_DOUBLE>()*(uint)contains(cl_device.getInfo<CL_DEVICE_EXTENSIONS>(), "cl_khr_fp64");
        is_fp32_capable = (uint)cl_device.getInfo<CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT>();
        is_fp16_capable = (uint)cl_device.getInfo<CL_DEVICE_NATIVE_VECTOR_WIDTH_HALF>()*(uint)contains(cl_device.getInfo<CL_DEVICE_EXTENSIONS>(), "cl_khr_fp16");
        is_int64_capable = (uint)cl_device.getInfo<CL_DEVICE_NATIVE_VECTOR_WIDTH_LONG>();
        is_int32_capable = (uint)cl_device.getInfo<CL_DEVICE_NATIVE_VECTOR_WIDTH_INT>();
        is_int16_capable = (uint)cl_device.getInfo<CL_DEVICE_NATIVE_VECTOR_WIDTH_SHORT>();
        is_int8_capable = (uint)cl_device.getInfo<CL_DEVICE_NATIVE_VECTOR_WIDTH_CHAR>();
        is_cpu = cl_device.getInfo<CL_DEVICE_TYPE>()==CL_DEVICE_TYPE_CPU;
        is_gpu = cl_device.getInfo<CL_DEVICE_TYPE>()==CL_DEVICE_TYPE_GPU;
        const uint ipc = is_gpu?2u:32u; // IPC (instructions per cycle) is 2 for GPUs and 32 for most modern CPUs
        const bool nvidia_192_cores_per_cu = contains_any(to_lower(name), {"gt 6", "gt 7", "gtx 6", "gtx 7", "quadro k", "tesla k"}) || (clock_frequency<1000u&&contains(to_lower(name), "titan")); // identify Kepler GPUs
        const bool nvidia_64_cores_per_cu = contains_any(to_lower(name), {"p100", "v100", "a100", "a30", " 16", " 20", "titan v", "titan rtx", "quadro t", "tesla t", "quadro rtx"}) && !contains(to_lower(name), "rtx a"); // identify P100, Volta, Turing, A100, A30
        const bool amd_128_cores_per_dualcu = contains(to_lower(name), "gfx10"); // identify RDNA/RDNA2 GPUs where dual CUs are reported
        const bool amd_256_cores_per_dualcu = contains(to_lower(name), "gfx11"); // identify RDNA3 GPUs where dual CUs are reported
        const float nvidia = (float)(contains(to_lower(vendor), "nvidia"))*(nvidia_64_cores_per_cu?64.0f:nvidia_192_cores_per_cu?192.0f:128.0f); // Nvidia GPUs have 192 cores/CU (Kepler), 128 cores/CU (Maxwell, Pascal, Ampere, Hopper, Ada) or 64 cores/CU (P100, Volta, Turing, A100, A30)
        const float amd = (float)(contains_any(to_lower(vendor), {"amd", "advanced"}))*(is_gpu?(amd_256_cores_per_dualcu?256.0f:amd_128_cores_per_dualcu?128.0f:64.0f):0.5f); // AMD GPUs have 64 cores/CU (GCN, CDNA), 128 cores/dualCU (RDNA, RDNA2) or 256 cores/dualCU (RDNA3), AMD CPUs (with SMT) have 1/2 core/CU
        const float intel = (float)(contains(to_lower(vendor), "intel"))*(is_gpu?8.0f:0.5f); // Intel integrated GPUs usually have 8 cores/CU, Intel CPUs (with HT) have 1/2 core/CU
        const float apple = (float)(contains(to_lower(vendor), "apple"))*(128.0f); // Apple ARM GPUs usually have 128 cores/CU
        const float arm = (float)(contains(to_lower(vendor), "arm"))*(is_gpu?8.0f:1.0f); // ARM GPUs usually have 8 cores/CU, ARM CPUs have 1 core/CU
        cores = to_uint((float)compute_units*(nvidia+amd+intel+apple+arm)); // for CPUs, compute_units is the number of threads (twice the number of cores with hyperthreading)
        tflops = 1E-6f*(float)cores*(float)ipc*(float)clock_frequency; // estimated device floating point performance in TeraFLOPs/s
    }
    inline Device_Info() {}; // default constructor
};

std::string get_opencl_c_code(); // implemented in kernel.hpp
inline void print_device_info(const Device_Info& d, const int id=-1) { // print OpenCL device info
    println("\r|----------------.------------------------------------------------------------|");
    if(id>-1) println("| Device ID      | "+alignl(58, std::to_string(id))+" |");
    println("| Device Name    | "+alignl(58, d.name                 )+" |");
    println("| Device Vendor  | "+alignl(58, d.vendor               )+" |");
    println("| Device Driver  | "+alignl(58, d.driver_version       )+" |");
    println("| OpenCL Version | "+alignl(58, d.opencl_c_version     )+" |");
    println("| Compute Units  | "+alignl(58, std::to_string(d.compute_units)+" at "+std::to_string(d.clock_frequency)+" MHz ("+std::to_string(d.cores)+" cores, "+std::to_string(d.tflops)+" TFLOPs/s)")+" |");
    println("| Memory, Cache  | "+alignl(58, std::to_string(d.memory)+" MB, "+std::to_string(d.global_cache)+" KB global / "+std::to_string(d.local_cache)+" KB local")+" |");
    println("| Buffer Limits  | "+alignl(58, std::to_string(d.max_global_buffer)+" MB global, "+std::to_string(d.max_constant_buffer)+" KB constant")+" |");
    println("|----------------'------------------------------------------------------------|");
}
inline std::vector<Device_Info> get_devices(const bool print_info=true) { // returns a std::vector of all available OpenCL devices
    std::vector<Device_Info> devices; // get all devices of all platforms
    std::vector<cl::Platform> cl_platforms; // get all platforms (drivers)
    cl::Platform::get(&cl_platforms);
    for(uint i=0u; i<(uint)cl_platforms.size(); i++) {
        std::vector<cl::Device> cl_devices;
        cl_platforms[i].getDevices(CL_DEVICE_TYPE_ALL, &cl_devices);
        for(uint j=0u; j<(uint)cl_devices.size(); j++) {
            devices.push_back(Device_Info(cl_devices[j]));
        }
    }
    if((uint)cl_platforms.size()==0u||(uint)devices.size()==0u) {
        print_error("There are no OpenCL devices available. Make sure that the OpenCL 1.2 Runtime for your device is installed. For GPUs it comes by default with the graphics driver, for CPUs it has to be installed separately.");
    }
    if(print_info) {
        println("\r|----------------.------------------------------------------------------------|");
        for(uint i=0u; i<(uint)devices.size(); i++) println("| Device ID "+alignr(4u, i)+" | "+alignl(58u, devices[i].name)+" |");
        println("|----------------'------------------------------------------------------------|");
    }
    return devices;
}
inline Device_Info select_device_with_most_flops(const std::vector<Device_Info>& devices=get_devices(), const bool print_info=true) { // returns device with best floating-point performance
    float best_value = 0.0f;
    uint best_i = 0u;
    for(uint i=0u; i<(uint)devices.size(); i++) { // find device with highest (estimated) floating point performance
        if(devices[i].tflops>best_value) {
            best_value = devices[i].tflops;
            best_i = i;
        }
    }
    if(print_info) print_device_info(devices[best_i], best_i);
    return devices[best_i];
}
inline Device_Info select_device_with_most_memory(const std::vector<Device_Info>& devices=get_devices(), const bool print_info=true) { // returns device with largest memory capacity
    uint best_value = 0u;
    uint best_i = 0u;
    for(uint i=0u; i<(uint)devices.size(); i++) { // find device with most memory
        if(devices[i].memory>best_value) {
            best_value = devices[i].memory;
            best_i = i;
        }
    }
    if(print_info) print_device_info(devices[best_i], best_i);
    return devices[best_i];
}
inline Device_Info select_device_with_id(const uint id, const std::vector<Device_Info>& devices=get_devices(), const bool print_info=true) { // returns device with specified ID
    if(id<(uint)devices.size()) {
        if(print_info) print_device_info(devices[id], id);
        return devices[id];
    } else {
        print_error("Your selected Device ID ("+std::to_string(id)+") is wrong.");
        return devices[0]; // is never executed, just to avoid compiler warnings
    }
}

class Device {
private:
    cl::Context cl_context;
    cl::Program cl_program;
    cl::CommandQueue cl_queue;
    bool exists = false;
    inline std::string enable_device_capabilities() const { return // enable FP64/FP16 capabilities if available
        "\n	#define def_workgroup_size "+std::to_string(WORKGROUP_SIZE)+"u"
        "\n	#ifdef cl_khr_fp64"
        "\n	#pragma OPENCL EXTENSION cl_khr_fp64 : enable" // make sure cl_khr_fp64 extension is enabled
        "\n	#endif"
        "\n	#ifdef cl_khr_fp16"
        "\n	#pragma OPENCL EXTENSION cl_khr_fp16 : enable" // make sure cl_khr_fp16 extension is enabled
        "\n	#endif"
        "\n	#ifdef cl_khr_int64_base_atomics"
        "\n	#pragma OPENCL EXTENSION cl_khr_int64_base_atomics : enable" // make sure cl_khr_int64_base_atomics extension is enabled
        "\n	#endif"
    ;}
public:
    Device_Info info;
    inline Device(const Device_Info& info, const std::string& opencl_c_code=get_opencl_c_code()) {
        this->info = info;
        cl_context = cl::Context(info.cl_device);
        cl_queue = cl::CommandQueue(cl_context, info.cl_device); // queue to push commands for the device
        cl::Program::Sources cl_source;
        const std::string kernel_code = enable_device_capabilities()+"\n"+opencl_c_code;
        cl_source.push_back({ kernel_code.c_str(), kernel_code.length() });
        cl_program = cl::Program(cl_context, cl_source);
#ifndef LOG
        int error = cl_program.build("-cl-fast-relaxed-math -w"); // compile OpenCL C code, disable warnings
        if(error) print_warning(cl_program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(info.cl_device)); // print build log
#else // LOG, generate logfile for OpenCL code compilation
        int error = cl_program.build("-cl-fast-relaxed-math"); // compile OpenCL C code
        const std::string log = cl_program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(info.cl_device);
        write_file("bin/kernel.log", log); // save build log
        if((uint)log.length()>2u) print_warning(log); // print build log
#endif // LOG
        if(error) print_error("OpenCL C code compilation failed with error code "+std::to_string(error)+". Make sure there are no errors in kernel.cpp.");
        else print_info("OpenCL C code successfully compiled.");
#ifdef PTX // generate assembly (ptx) file for OpenCL code
        write_file("bin/kernel.ptx", cl_program.getInfo<CL_PROGRAM_BINARIES>()[0]); // save binary (ptx file)
#endif // PTX
        this->exists = true;
    }
    inline Device() {} // default constructor
    inline void finish_queue() { cl_queue.finish(); }
    inline cl::Context get_cl_context() const { return cl_context; }
    inline cl::Program get_cl_program() const { return cl_program; }
    inline cl::CommandQueue get_cl_queue() const { return cl_queue; }
    inline bool is_initialized() const { return exists; }
};

template<typename T> class Memory {
private:
    ulong N = 0ull; // buffer length
    uint d = 1u; // buffer dimensions
    bool host_buffer_exists = false;
    bool device_buffer_exists = false;
    bool external_host_buffer = false;
    T* host_buffer = nullptr; // host buffer
    cl::Buffer device_buffer; // device buffer
    Device* device = nullptr; // pointer to linked Device
    cl::CommandQueue cl_queue; // command queue
    inline void initialize_auxiliary_pointers() {
        /********/ x = s0 = host_buffer; /******/ if(d>0x4u) s4 = host_buffer+N*0x4ull; if(d>0x8u) s8 = host_buffer+N*0x8ull; if(d>0xCu) sC = host_buffer+N*0xCull;
        if(d>0x1u) y = s1 = host_buffer+N; /****/ if(d>0x5u) s5 = host_buffer+N*0x5ull; if(d>0x9u) s9 = host_buffer+N*0x9ull; if(d>0xDu) sD = host_buffer+N*0xDull;
        if(d>0x2u) z = s2 = host_buffer+N*0x2ull; if(d>0x6u) s6 = host_buffer+N*0x6ull; if(d>0xAu) sA = host_buffer+N*0xAull; if(d>0xEu) sE = host_buffer+N*0xEull;
        if(d>0x3u) w = s3 = host_buffer+N*0x3ull; if(d>0x7u) s7 = host_buffer+N*0x7ull; if(d>0xBu) sB = host_buffer+N*0xBull; if(d>0xFu) sF = host_buffer+N*0xFull;
    }
    inline void allocate_device_buffer(Device& device, const bool allocate_device) {
        this->device = &device;
        this->cl_queue = device.get_cl_queue();
        if(allocate_device) {
            device.info.memory_used += (uint)(capacity()/1048576ull); // track device memory usage
            if(device.info.memory_used>device.info.memory) print_error("Device \""+device.info.name+"\" does not have enough memory. Allocating another "+std::to_string((uint)(capacity()/1048576ull))+" MB would use a total of "+std::to_string(device.info.memory_used)+" MB / "+std::to_string(device.info.memory)+" MB.");
            int error = 0;
            device_buffer = cl::Buffer(device.get_cl_context(), CL_MEM_READ_WRITE, capacity(), nullptr, &error);
            if(error==-61) print_error("Memory size is too large at "+std::to_string((uint)(capacity()/1048576ull))+" MB. Device \""+device.info.name+"\" accepts a maximum buffer size of "+std::to_string(device.info.max_global_buffer)+" MB.");
            else if(error) print_error("Device buffer allocation failed with error code "+std::to_string(error)+".");
            device_buffer_exists = true;
        }
    }
public:
    T *x=nullptr, *y=nullptr, *z=nullptr, *w=nullptr; // host buffer auxiliary pointers for multi-dimensional array access (array of structures)
    T *s0=nullptr, *s1=nullptr, *s2=nullptr, *s3=nullptr, *s4=nullptr, *s5=nullptr, *s6=nullptr, *s7=nullptr, *s8=nullptr, *s9=nullptr, *sA=nullptr, *sB=nullptr, *sC=nullptr, *sD=nullptr, *sE=nullptr, *sF=nullptr;
    inline Memory(Device& device, const ulong N, const uint dimensions=1u, const bool allocate_host=true, const bool allocate_device=true, const T value=(T)0) {
        if(!device.is_initialized()) print_error("No Device selected. Call Device constructor.");
        if(N*(ulong)dimensions==0ull) print_error("Memory size must be larger than 0.");
        this->N = N;
        this->d = dimensions;
        allocate_device_buffer(device, allocate_device);
        if(allocate_host) {
            host_buffer = new T[N*(ulong)d];
            for(ulong i=0ull; i<N*(ulong)d; i++) host_buffer[i] = value;
            initialize_auxiliary_pointers();
            host_buffer_exists = true;
        }
        //write_to_device();
    }
    inline Memory(Device& device, const ulong N, const uint dimensions, T* const host_buffer, const bool allocate_device=true) {
        if(!device.is_initialized()) print_error("No Device selected. Call Device constructor.");
        if(N*(ulong)dimensions==0ull) print_error("Memory size must be larger than 0.");
        this->N = N;
        this->d = dimensions;
        allocate_device_buffer(device, allocate_device);
        this->host_buffer = host_buffer;
        initialize_auxiliary_pointers();
        host_buffer_exists = true;
        external_host_buffer = true;
        //write_to_device();
    }
    inline Memory() {} // default constructor
    inline ~Memory() {
        delete_buffers();
    }
    inline Memory& operator=(Memory&& memory) noexcept { // move assignment
        delete_buffers(); // delete existing buffers and restore default state
        N = memory.length(); // copy values/pointers from memory
        d = memory.dimensions();
        device = memory.device;
        cl_queue = memory.device->get_cl_queue();
        if(memory.device_buffer_exists) {
            device_buffer = memory.get_cl_buffer(); // transfer device_buffer pointer
            device->info.memory_used += (uint)(capacity()/1048576ull); // track device memory usage
            device_buffer_exists = true;
        }
        if(memory.host_buffer_exists) {
            host_buffer = memory.exchange_host_buffer(nullptr); // transfer host_buffer pointer
            initialize_auxiliary_pointers();
            host_buffer_exists = true;
            external_host_buffer = memory.external_host_buffer;
        }
        return *this; // destructor of memory will be called automatically
    }
    inline T* const exchange_host_buffer(T* const host_buffer) { // sets host_buffer to new pointer and returns old pointer
        T* const swap = this->host_buffer;
        this->host_buffer = host_buffer;
        return swap;
    }
    inline void add_host_buffer() { // makes only sense if there is no host buffer yet but an existing device buffer
        if(!host_buffer_exists&&device_buffer_exists) {
            host_buffer = new T[N*(ulong)d];
            initialize_auxiliary_pointers();
            read_from_device();
            host_buffer_exists = true;
            external_host_buffer = false;
        } else if(!device_buffer_exists) {
            print_error("There is no existing device buffer, so can't add host buffer.");
        }
    }
    inline void add_device_buffer() { // makes only sense if there is no device buffer yet but an existing host buffer
        if(!device_buffer_exists&&host_buffer_exists) {
            allocate_device_buffer(*device, true);
            write_to_device();
        } else if(!host_buffer_exists) {
            print_error("There is no existing host buffer, so can't add device buffer.");
        }
    }
    inline void delete_host_buffer() {
        host_buffer_exists = false;
        if(!external_host_buffer) delete[] host_buffer;
        if(!device_buffer_exists) {
            N = 0ull;
            d = 1u;
        }
    }
    inline void delete_device_buffer() {
        if(device_buffer_exists) device->info.memory_used -= (uint)(capacity()/1048576ull); // track device memory usage
        device_buffer_exists = false;
        device_buffer = nullptr;
        if(!host_buffer_exists) {
            N = 0ull;
            d = 1u;
        }
    }
    inline void delete_buffers() {
        delete_device_buffer();
        delete_host_buffer();
    }
    inline void reset(const T value=(T)0) {
        if(host_buffer_exists) for(ulong i=0ull; i<N*(ulong)d; i++) host_buffer[i] = value;
        write_to_device();
    }
    inline const ulong length() const { return N; }
    inline const uint dimensions() const { return d; }
    inline const ulong range() const { return N*(ulong)d; }
    inline const ulong capacity() const { return N*(ulong)d*sizeof(T); } // returns capacity of the buffer in Byte
    inline T* const data() { return host_buffer; }
    inline const T* const data() const { return host_buffer; }
    inline T* const operator()() { return host_buffer; }
    inline const T* const operator()() const { return host_buffer; }
    inline T& operator[](const ulong i) { return host_buffer[i]; }
    inline const T& operator[](const ulong i) const { return host_buffer[i]; }
    inline const T operator()(const ulong i) const { return host_buffer[i]; }
    inline const T operator()(const ulong i, const uint dimension) const { return host_buffer[i+(ulong)dimension*N]; } // array of structures
    inline void read_from_device(const bool blocking=true) {
        if(host_buffer_exists&&device_buffer_exists) {
            int res = cl_queue.enqueueReadBuffer(device_buffer, blocking, 0u, capacity(), (void*)host_buffer);
            if (res != CL_SUCCESS) throw OpenCLException(std::to_string(res));
        }
    }
    inline void write_to_device(const bool blocking=true) {
        if(host_buffer_exists&&device_buffer_exists) cl_queue.enqueueWriteBuffer(device_buffer, blocking, 0u, capacity(), (void*)host_buffer);
    }
    inline void read_from_device(const ulong offset, const ulong length, const bool blocking=true) {
        if(host_buffer_exists&&device_buffer_exists) {
            const ulong safe_offset=min(offset, range()), safe_length=min(length, range()-safe_offset);
            if(safe_length>0ull) cl_queue.enqueueReadBuffer(device_buffer, blocking, safe_offset*sizeof(T), safe_length*sizeof(T), (void*)(host_buffer+safe_offset));
        }
    }
    inline void write_to_device(const ulong offset, const ulong length, const bool blocking=true) {
        if(host_buffer_exists&&device_buffer_exists) {
            const ulong safe_offset=min(offset, range()), safe_length=min(length, range()-safe_offset);
            if(safe_length>0ull) cl_queue.enqueueWriteBuffer(device_buffer, blocking, safe_offset*sizeof(T), safe_length*sizeof(T), (void*)(host_buffer+safe_offset));
        }
    }
    inline void read_from_device_1d(const ulong x0, const ulong x1, const int dimension=-1, const bool blocking=true) { // read 1D domain from device, either for all std::vector dimensions (-1) or for a specified dimension
        if(host_buffer_exists&&device_buffer_exists) {
            const uint i0=(uint)max(0, dimension), i1=dimension<0 ? d : i0+1u;
            for(uint i=i0; i<i1; i++) {
                const ulong safe_offset=min((ulong)i*N+x0, range()), safe_length=min(x1-x0, range()-safe_offset);
                if(safe_length>0ull) cl_queue.enqueueReadBuffer(device_buffer, false, safe_offset*sizeof(T), safe_length*sizeof(T), (void*)(host_buffer+safe_offset));
            }
            if(blocking) cl_queue.finish();
        }
    }
    inline void write_to_device_1d(const ulong x0, const ulong x1, const int dimension=-1, const bool blocking=true) { // write 1D domain to device, either for all std::vector dimensions (-1) or for a specified dimension
        if(host_buffer_exists&&device_buffer_exists) {
            const uint i0=(uint)max(0, dimension), i1=dimension<0 ? d : i0+1u;
            for(uint i=i0; i<i1; i++) {
                const ulong safe_offset=min((ulong)i*N+x0, range()), safe_length=min(x1-x0, range()-safe_offset);
                if(safe_length>0ull) cl_queue.enqueueWriteBuffer(device_buffer, false, safe_offset*sizeof(T), safe_length*sizeof(T), (void*)(host_buffer+safe_offset));
            }
            if(blocking) cl_queue.finish();
        }
    }
    inline void read_from_device_2d(const ulong x0, const ulong x1, const ulong y0, const ulong y1, const ulong Nx, const ulong Ny, const int dimension=-1, const bool blocking=true) { // read 2D domain from device, either for all std::vector dimensions (-1) or for a specified dimension
        if(host_buffer_exists&&device_buffer_exists) {
            for(uint y=y0; y<y1; y++) {
                const ulong n = x0+y*Nx;
                const uint i0=(uint)max(0, dimension), i1=dimension<0 ? d : i0+1u;
                for(uint i=i0; i<i1; i++) {
                    const ulong safe_offset=min((ulong)i*N+n, range()), safe_length=min(x1-x0, range()-safe_offset);
                    if(safe_length>0ull) cl_queue.enqueueReadBuffer(device_buffer, false, safe_offset*sizeof(T), safe_length*sizeof(T), (void*)(host_buffer+safe_offset));
                }
            }
            if(blocking) cl_queue.finish();
        }
    }
    inline void write_to_device_2d(const ulong x0, const ulong x1, const ulong y0, const ulong y1, const ulong Nx, const ulong Ny, const int dimension=-1, const bool blocking=true) { // write 2D domain to device, either for all std::vector dimensions (-1) or for a specified dimension
        if(host_buffer_exists&&device_buffer_exists) {
            for(uint y=y0; y<y1; y++) {
                const ulong n = x0+y*Nx;
                const uint i0=(uint)max(0, dimension), i1=dimension<0 ? d : i0+1u;
                for(uint i=i0; i<i1; i++) {
                    const ulong safe_offset=min((ulong)i*N+n, range()), safe_length=min(x1-x0, range()-safe_offset);
                    if(safe_length>0ull) cl_queue.enqueueWriteBuffer(device_buffer, false, safe_offset*sizeof(T), safe_length*sizeof(T), (void*)(host_buffer+safe_offset));
                }
            }
            if(blocking) cl_queue.finish();
        }
    }
    inline void read_from_device_3d(const ulong x0, const ulong x1, const ulong y0, const ulong y1, const ulong z0, const ulong z1, const ulong Nx, const ulong Ny, const ulong Nz, const int dimension=-1, const bool blocking=true) { // read 3D domain from device, either for all std::vector dimensions (-1) or for a specified dimension
        if(host_buffer_exists&&device_buffer_exists) {
            for(uint z=z0; z<z1; z++) {
                for(uint y=y0; y<y1; y++) {
                    const ulong n = x0+(y+z*Ny)*Nx;
                    const uint i0=(uint)max(0, dimension), i1=dimension<0 ? d : i0+1u;
                    for(uint i=i0; i<i1; i++) {
                        const ulong safe_offset=min((ulong)i*N+n, range()), safe_length=min(x1-x0, range()-safe_offset);
                        if(safe_length>0ull) cl_queue.enqueueReadBuffer(device_buffer, false, safe_offset*sizeof(T), safe_length*sizeof(T), (void*)(host_buffer+safe_offset));
                    }
                }
            }
            if(blocking) cl_queue.finish();
        }
    }
    inline void write_to_device_3d(const ulong x0, const ulong x1, const ulong y0, const ulong y1, const ulong z0, const ulong z1, const ulong Nx, const ulong Ny, const ulong Nz, const int dimension=-1, const bool blocking=true) { // write 3D domain to device, either for all std::vector dimensions (-1) or for a specified dimension
        if(host_buffer_exists&&device_buffer_exists) {
            for(uint z=z0; z<z1; z++) {
                for(uint y=y0; y<y1; y++) {
                    const ulong n = x0+(y+z*Ny)*Nx;
                    const uint i0=(uint)max(0, dimension), i1=dimension<0 ? d : i0+1u;
                    for(uint i=i0; i<i1; i++) {
                        const ulong safe_offset=min((ulong)i*N+n, range()), safe_length=min(x1-x0, range()-safe_offset);
                        if(safe_length>0ull) cl_queue.enqueueWriteBuffer(device_buffer, false, safe_offset*sizeof(T), safe_length*sizeof(T), (void*)(host_buffer+safe_offset));
                    }
                }
            }
            if(blocking) cl_queue.finish();
        }
    }
    inline void enqueue_read_from_device() { read_from_device(false); }
    inline void enqueue_write_to_device() { write_to_device(false); }
    inline void enqueue_read_from_device(const ulong offset, const ulong length) { read_from_device(offset, length, false); }
    inline void enqueue_write_to_device(const ulong offset, const ulong length) { write_to_device(offset, length, false); }
    inline void finish_queue() { cl_queue.finish(); }
    inline const cl::Buffer& get_cl_buffer() const { return device_buffer; }
};

template<typename T>
class LocalMemory {
private:
    ulong N = 0ll;
public:
    LocalMemory(ulong sz): N(sz) {}
    ulong get_size() const {return N * sizeof(T);}
};

class Kernel {
private:
    ulong N = 0ull; // kernel range
    uint number_of_parameters = 0u;
    cl::Kernel cl_kernel;
    cl::NDRange cl_range_global, cl_range_local;
    cl::CommandQueue cl_queue;
    template<typename T> inline void link_parameter(const uint position, const Memory<T>& memory) {
        cl_kernel.setArg(position, memory.get_cl_buffer());
    }
    template<typename T> inline void link_parameter(const uint position, const T& constant) {
        cl_kernel.setArg(position, sizeof(T), (void*)&constant);
    }
    template<typename T> inline void link_parameter(const uint position, const LocalMemory<T>& memory) {
        cl_kernel.setArg(position, memory.get_size(), NULL);
    }
    inline void link_parameters(const uint starting_position) {
        number_of_parameters = max(number_of_parameters, starting_position);
    }
    template<class T, class... U> inline void link_parameters(const uint starting_position, const T& parameter, const U&... parameters) {
        link_parameter(starting_position, parameter);
        link_parameters(starting_position+1u, parameters...);
    }
public:
    template<class... T> inline Kernel(const Device& device, const ulong N, const std::string& name, const T&... parameters) { // accepts Memory<T> objects and fundamental data type constants
        if(!device.is_initialized()) print_error("No Device selected. Call Device constructor.");
        cl_kernel = cl::Kernel(device.get_cl_program(), name.c_str());
        link_parameters(number_of_parameters, parameters...); // expand variadic template to link kernel parameters
        set_ranges(N);
        cl_queue = device.get_cl_queue();
    }
    template<class... T> inline Kernel(const Device& device, const ulong N, const uint workgroup_size, const std::string& name, const T&... parameters) { // accepts Memory<T> objects and fundamental data type constants
        if(!device.is_initialized()) print_error("No Device selected. Call Device constructor.");
        cl_kernel = cl::Kernel(device.get_cl_program(), name.c_str());
        link_parameters(number_of_parameters, parameters...); // expand variadic template to link kernel parameters
        set_ranges(N, (ulong)workgroup_size);
        cl_queue = device.get_cl_queue();
    }
    inline Kernel() {} // default constructor
    inline Kernel& set_ranges(const ulong N, const ulong workgroup_size=(ulong)WORKGROUP_SIZE) {
        this->N = N;
        cl_range_global = cl::NDRange(((N+workgroup_size-1ull)/workgroup_size)*workgroup_size); // make global range a multiple of local range
        cl_range_local = cl::NDRange(workgroup_size);
        return *this;
    }
    inline const ulong range() const { return N; }
    inline uint get_number_of_parameters() const { return number_of_parameters; }
    uint get_max_workgroup_size(const Device& device) {
        return cl_kernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(device.info.cl_device);
    }
    template<class... T> inline Kernel& add_parameters(const T&... parameters) { // add parameters to the list of existing parameters
        link_parameters(number_of_parameters, parameters...); // expand variadic template to link kernel parameters
        return *this;
    }
    template<class... T> inline Kernel& set_parameters(const uint starting_position, const T&... parameters) { // set parameters starting at specified position
        link_parameters(starting_position, parameters...); // expand variadic template to link kernel parameters
        return *this;
    }
    inline Kernel& enqueue_run(const uint t=1u, const uint o=0u) {
        for(uint i=0u; i<t; i++) {
            int res = cl_queue.enqueueNDRangeKernel(cl_kernel, o ? cl::NDRange(o) : cl::NullRange, cl_range_global, cl_range_local);
            if (res != CL_SUCCESS) throw OpenCLException(std::to_string(res));
        }
        return *this;
    }
    inline Kernel& finish_queue() {
        int res = cl_queue.finish();
        if (res != CL_SUCCESS) throw OpenCLException(std::to_string(res));
        return *this;
    }
    inline Kernel& run(const uint t=1u) {
        enqueue_run(t);
        finish_queue();
        return *this;
    }
    inline Kernel& operator()(const uint t=1u) {
        return run(t);
    }
};

}

#endif
