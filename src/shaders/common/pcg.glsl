#extension GL_GOOGLE_include_directive : enable
#include "constant.glsl"

#extension GL_ARB_gpu_shader_int64 : require

// a slight modification from 

/*
 * PCG Random Number Generation for C.
 *
 * Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For additional information about the PCG random number generation scheme,
 * including its license and other licensing options, visit
 *
 *       http://www.pcg-random.org
 */

 /*
  * This code is derived from the full C implementation, which is in turn
  * derived from the canonical C++ PCG implementation. The C++ version
  * has many additional features and is preferable if you can use C++ in
  * your project.
  */

// state for global RNGs

#define PCG32_INITIALIZER_STATE 0x853c49e6748fea9bUL
#define PCG32_INITIALIZER_INC 0xda3e39cb94b95bdbUL

struct pcg32_random_t
{                               // Internals are *Private*.
    uint64_t state;             // RNG state.  All values are possible.
    uint64_t inc;               // Controls which RNG sequence (stream) is
                                // selected. Must *always* be odd.
};

//static pcg32_random_t pcg32_global = PCG32_INITIALIZER;

// pcg32_random()
// pcg32_random_r(rng)
//     Generate a uniformly distributed 32-bit random number

uint
pcg32_random_r(inout pcg32_random_t rng)
{
    uint64_t oldstate = rng.state;
    rng.state = oldstate * 6364136223846793005ul + rng.inc;
    uint xorshifted = uint(((oldstate >> 18u) ^ oldstate) >> 27u);
    uint rot = uint(oldstate >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

// pcg32_srandom(initstate, initseq)
// pcg32_srandom_r(rng, initstate, initseq):
//     Seed the rng.  Specified in two parts, state initializer and a
//     sequence selection constant (a.k.a. stream id)

void
pcg32_srandom_r(inout pcg32_random_t rng,
                uint64_t initstate,
                uint64_t initseq)
{
    rng.state = 0U;
    rng.inc = (initseq << 1u) | 1u;
    pcg32_random_r(rng);
    rng.state += initstate;
    pcg32_random_r(rng);
}

// PCG helper function

struct Pcg32
{
    pcg32_random_t pcg_rng;
};

Pcg32
Pcg_create(uint64_t seed,
           uint64_t stream_id)
{
    Pcg32 rng;
    pcg32_srandom_r(rng.pcg_rng,
                    seed,
                    stream_id);
    return rng;
}

float
Pcg_next_float(inout Pcg32 rng)
{
    const uint v = pcg32_random_r(rng.pcg_rng);
    return float(v) / float(UINT32_MAX);
}

vec2
Pcg_next_vec2(inout Pcg32 rng)
{
    const uint u = pcg32_random_r(rng.pcg_rng);
    const uint v = pcg32_random_r(rng.pcg_rng);
    return vec2(u, v) / float(UINT32_MAX);
}

vec3
Pcg_next_vec3(inout Pcg32 rng)
{
    const uint u = pcg32_random_r(rng.pcg_rng);
    const uint v = pcg32_random_r(rng.pcg_rng);
    const uint w = pcg32_random_r(rng.pcg_rng);
    return vec3(u, v, w) / float(UINT32_MAX);
}

Pcg32 GLOBAL_PCG;

#ifdef USE_PCG

void
srand(uvec2 pixel_index,
      uvec2 image_size,
      uint sample_index)
{
    uint seed = pixel_index.x + image_size.x * pixel_index.y;
    // note: we have to modify sample_index to make sure that error distribution looks nice and not very correlated
	GLOBAL_PCG = Pcg_create(seed,
                            sample_index * image_size.x * image_size.y + 1);
}

float
rand()
{
    return Pcg_next_float(GLOBAL_PCG);
}

vec2
rand2()
{
    return Pcg_next_vec2(GLOBAL_PCG);
}

vec3
rand3()
{
    return Pcg_next_vec3(GLOBAL_PCG);
}
#endif
