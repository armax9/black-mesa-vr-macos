#pragma once
#include <cstdint>
#include <cmath>
#define A_CPU 1
#include "ffx_a.h"
#include "ffx_fsr1.h"
namespace bmvr_fsr {
struct EasuConstants { uint32_t con[4][4]{}; float region[4]{0.5f,0.5f,0.6f,0.f}; uint32_t extent[4]{}; };
struct RcasConstants { uint32_t con[4]{}; float region[4]{0.5f,0.5f,0.6f,0.f}; uint32_t extent[4]{}; };
static_assert(sizeof(EasuConstants)==96, "EASU push constant ABI");
static_assert(sizeof(RcasConstants)==48, "RCAS push constant ABI");
}
