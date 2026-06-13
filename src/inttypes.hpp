#pragma once

#include <cstdint>

#ifdef _EE
// ps2sdk's tamtypes.h owns the unsigned typedefs on PS2; reuse them so both
// sets of headers can coexist in one translation unit
#include <tamtypes.h>
typedef s8 i8;
typedef s16 i16;
typedef s32 i32;
#else
typedef std::int8_t i8;
typedef std::uint8_t u8;
typedef std::int16_t i16;
typedef std::uint16_t u16;
typedef std::int32_t i32;
typedef std::uint32_t u32;
typedef std::uint64_t u64;
#endif

typedef std::intptr_t iptr;
typedef float f32;
typedef double f64;
