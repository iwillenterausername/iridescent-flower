# Iridescent Flower — Custom Modules

Iridescent Flower loads up to **8 Windows DLLs** from:

```
Documents/Iridescent Flower/Custom Modules/
```

Right-click an empty slot → `LOAD / REPLACE FX`; loaded DLLs appear at the
bottom of the menu. Each DLL replaces one of the `CUSTOM 1…8` module types and
gets 8 automatable knobs in the host UI.

The example included in this folder is **BBD DELAY**, the Copperline BBD engine
wrapped as a user DLL. Build it with the `IridescentBbdCustom` CMake target; the
build copies `BbdDelay.dll` into the folder above.

## ABI (version 1)

The full header is [`Source/CustomModuleApi.h`](../Source/CustomModuleApi.h).
A module exports four C symbols:

```cpp
extern "C" __declspec(dllexport) const IFModuleInfo* if_get_module_info();
extern "C" __declspec(dllexport) void* if_create (double sampleRate, int maxBlockSize);
extern "C" __declspec(dllexport) void  if_destroy (void* instance);
extern "C" __declspec(dllexport) void  if_process (void* instance,
                                                   float* left,
                                                   float* right,
                                                   int numSamples,
                                                   const float* parameters,
                                                   int numParameters,
                                                   double bpm);
```

- `IFModuleInfo` contains the display name, tagline, ARGB colour and exactly
  **8 parameter names/defaults**.
- `parameters[]` is always normalized `0…1`. The module maps that to its own
  range, e.g. `timeMs = 30 + p[0] * 970`.
- `left` and `right` are non-interleaved `float*` buffers. `right` can be the
  same pointer as `left` for mono.
- `bpm` is the host tempo.
- Do not allocate/lock in `if_process`; create delay lines in `if_create`.
- The host creates one instance per chain that actually uses the module.

## Minimal module

```cpp
#include "CustomModuleApi.h"

struct Gain { float sr = 48000; };

extern "C" __declspec(dllexport) const IFModuleInfo* if_get_module_info()
{
    static const IFModuleInfo info {
        "GAIN", "SIMPLE USER GAIN", 0xff42e8ff,
        { { "GAIN", 0.5f }, { "PAN", 0.5f }, { "P2", 0.5f }, { "P3", 0.5f },
          { "P4", 0.5f }, { "P5", 0.5f }, { "P6", 0.5f }, { "P7", 0.5f } }
    };
    return &info;
}

extern "C" __declspec(dllexport) void* if_create (double sr, int) { return new Gain { (float) sr }; }
extern "C" __declspec(dllexport) void if_destroy (void* p) { delete (Gain*) p; }

extern "C" __declspec(dllexport) void if_process (void* raw, float* l, float* r,
                                                   int n, const float* p, int count, double)
{
    auto* g = (Gain*) raw;
    const float gain = p[0] * 2.0f;
    for (int i = 0; i < n; ++i) { l[i] *= gain; if (r != l) r[i] *= gain; }
}
```

Build with MSVC x64 as a **DLL**, copy to the folder, then re-open the plugin
(or re-open the slot menu). The host scans at construction time.

## Tips

- Return `nullptr` from `if_create` if the sample rate/block size is unusable.
- Keep the DLL x64; the plugin is x64.
- A DLL can be a VST3-style C++ engine as long as it exports the four symbols.
- If a module fails to load, the slot falls back to bypass and the UI shows
  `EMPTY DLL SLOT`.
