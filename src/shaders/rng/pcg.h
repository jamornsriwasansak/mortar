#ifndef PCG_H
#define PCG_H

#include "../cpp_compatible.h"

#define PCG_UINT32_MAX 4294967295u

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
#define PCG32_INITIALIZER_INC   0xda3e39cb94b95bdbUL

struct pcg32_random_t
{                   // Internals are *Private*.
    uint64_t state; // RNG state.  All values are possible.
    uint64_t inc;   // Controls which RNG sequence (stream) is
                    // selected. Must *always* be odd.
};

// static pcg32_random_t pcg32_global = PCG32_INITIALIZER;

// pcg32_random()
// pcg32_random_r(rng)
//     Generate a uniformly distributed 32-bit random number

uint pcg32_random_r(INOUT(pcg32_random_t) rng)
{
    uint64_t oldstate = rng.state;
    rng.state         = oldstate * 6364136223846793005ul + rng.inc;
    uint xorshifted   = uint(((oldstate >> 18u) ^ oldstate) >> 27u);
    uint rot          = uint(oldstate >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

// pcg32_srandom(initstate, initseq)
// pcg32_srandom_r(rng, initstate, initseq):
//     Seed the rng.  Specified in two parts, state initializer and a
//     sequence selection constant (a.k.a. stream id)

void
pcg32_srandom_r(INOUT(pcg32_random_t) rng, uint64_t initstate, uint64_t initseq)
{
    rng.state = 0U;
    rng.inc   = (initseq << 1u) | 1u;
    pcg32_random_r(rng);
    rng.state += initstate;
    pcg32_random_r(rng);
}

// PCG helper function

// light weight pcg struct

struct PcgRng
{
    uint64_t m_state;
    uint64_t m_rng_inc;

    // A single iteration of Bob Jenkins' One-At-A-Time hashing algorithm.
    uint
    hash(uint x)
    {
        x += (x << 10u);
        x ^= (x >> 6u);
        x += (x << 3u);
        x ^= (x >> 11u);
        x += (x << 15u);
        return x;
    }

    void
    init(const uint seed, const uint seq)
    {
        m_state   = 0U;
        m_rng_inc = get_inc(seq);
        random_r(m_rng_inc);
        m_state += seed;
        random_r(m_rng_inc);
    }

    uint
    random_r(const uint64_t inc)
    {
        uint64_t oldstate = m_state;
        m_state           = oldstate * 6364136223846793005ul + inc;
        uint xorshifted   = uint(((oldstate >> 18u) ^ oldstate) >> 27u);
        uint rot          = uint(oldstate >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
    }

    uint64_t
    get_inc(const uint init_seq)
    {
        return (hash(init_seq) << 1u) | 1u;
    }

    float
    next_float()
    {
        uint rnd = random_r(m_rng_inc);
        return float(rnd) / float(PCG_UINT32_MAX);
    }

    float2
    next_float2()
    {
        uint rnd0 = random_r(m_rng_inc);
        uint rnd1 = random_r(m_rng_inc);
        return float2(rnd0, rnd1) / float(PCG_UINT32_MAX);
    }
};

#endif
