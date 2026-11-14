#pragma once

// Iridescent Flower custom module ABI (version 1).
// A module is a Windows DLL exporting the symbols below. It lives in:
//   Documents/Iridescent Flower/Custom Modules/
//
// A module has exactly 8 float parameters so the host UI can stay compact.
// Parameters are always passed in 0..1 normalized form; the module's own
// info block supplies the display names.

#include <stdint.h>

#ifdef _WIN32
  #define IF_MODULE_EXPORT extern "C" __declspec(dllexport)
#else
  #define IF_MODULE_EXPORT extern "C" __attribute__((visibility("default")))
#endif

struct IFModuleParam
{
    const char* name;
    float defaultValue; // 0..1
};

struct IFModuleInfo
{
    const char* name;
    const char* tagline;
    uint32_t colour;    // ARGB
    IFModuleParam params[8];
};

using IFGetInfoFn = const IFModuleInfo* (*)();
using IFCreateFn = void* (*) (double sampleRate, int maxBlockSize);
using IFDestroyFn = void (*) (void* instance);
using IFProcessFn = void (*) (void* instance,
                             float* left,
                             float* right,
                             int numSamples,
                             const float* parameters,
                             int numParameters,
                             double bpm);

IF_MODULE_EXPORT const IFModuleInfo* if_get_module_info();
IF_MODULE_EXPORT void* if_create (double sampleRate, int maxBlockSize);
IF_MODULE_EXPORT void if_destroy (void* instance);
IF_MODULE_EXPORT void if_process (void* instance,
                                  float* left,
                                  float* right,
                                  int numSamples,
                                  const float* parameters,
                                  int numParameters,
                                  double bpm);
