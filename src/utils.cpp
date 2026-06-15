#include <cstdarg>
#include <cstdio>

#include "ZunMath.hpp"
#include "i18n.hpp"
#include "utils.hpp"

namespace utils
{

static void EmitLog(const char *prefix, const char *fmt, std::va_list args)
{
    char tmpBuffer[512];
    std::vsnprintf(tmpBuffer, sizeof(tmpBuffer), fmt, args);

    std::printf("%s%s\n", prefix, tmpBuffer);
    std::fflush(stdout);
}

void DebugPrint(const char *fmt, ...)
{
    std::va_list args;
    va_start(args, fmt);
    EmitLog("[TH06] ", fmt, args);
    va_end(args);
}

f32 AddNormalizeAngle(f32 a, f32 b)
{
    i32 i;

    i = 0;
    a += b;
    while (a > ZUN_PI)
    {
        a -= ZUN_2PI;
        if (i++ > 16)
            break;
    }
    while (a < -ZUN_PI)
    {
        a += ZUN_2PI;
        if (i++ > 16)
            break;
    }
    return a;
}

void Rotate(ZunVec3 *outVector, const ZunVec3 *point, f32 angle)
{
    f32 sinOut;
    f32 cosOut;

    sinOut = ZUN_SINF(angle);
    cosOut = ZUN_COSF(angle);
    outVector->x = cosOut * point->x + sinOut * point->y;
    outVector->y = cosOut * point->y - sinOut * point->x;
}

#ifdef DEBUG
void DebugPrint2(const char *fmt, ...)
{
    std::va_list args;
    va_start(args, fmt);
    EmitLog("[TH06/Debug] ", fmt, args);
    va_end(args);
}
#endif
}; // namespace utils
