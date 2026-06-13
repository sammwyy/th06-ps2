#include "Ps2Pad.hpp"
#include "Controller.hpp"
#include "utils.hpp"

#include <libpad.h>
#include <loadfile.h>

// The pad DMA buffer must be 64-byte aligned and 256 bytes long.
static char s_PadBuf[256] __attribute__((aligned(64)));
static bool s_Ready = false;

// Analog stick is 0..255 centered at 128; treat past this from center as a press.
#define STICK_DEADZONE 48

bool Ps2Pad::Init()
{
    // Use the newer X modules (paired with libpadx): the old rom0:PADMAN does
    // not bring its RPC server up reliably under emulation, which hangs padInit.
    if (SifLoadModule("rom0:XSIO2MAN", 0, NULL) < 0)
    {
        utils::DebugPrint2("Ps2Pad: failed to load XSIO2MAN");
        return false;
    }
    if (SifLoadModule("rom0:XPADMAN", 0, NULL) < 0)
    {
        utils::DebugPrint2("Ps2Pad: failed to load XPADMAN");
        return false;
    }

    padInit(0);
    int openRet = padPortOpen(0, 0, s_PadBuf);
    utils::DebugPrint2("Ps2Pad: padPortOpen -> %d", openRet);
    if (openRet == 0)
    {
        return false;
    }

    // The pad keeps negotiating (FINDPAD -> STABLE) for several frames after this,
    // so don't block here: mark it open and let GetButtons poll the state per frame.
    s_Ready = true;
    utils::DebugPrint2("Ps2Pad: port open, pad will settle during play");
    return true;
}

u16 Ps2Pad::GetButtons()
{
    if (!s_Ready)
    {
        return 0;
    }

    int state = padGetState(0, 0);
    if (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1)
    {
        return 0;
    }

    struct padButtonStatus pad;
    if (padRead(0, 0, &pad) == 0)
    {
        return 0;
    }

    // padlib reports buttons active-low (0 = pressed)
    u16 held = ~pad.btns;
    u16 out = 0;

    if (held & PAD_UP)
        out |= TH_BUTTON_UP;
    if (held & PAD_DOWN)
        out |= TH_BUTTON_DOWN;
    if (held & PAD_LEFT)
        out |= TH_BUTTON_LEFT;
    if (held & PAD_RIGHT)
        out |= TH_BUTTON_RIGHT;

    if (held & PAD_CROSS)
        out |= TH_BUTTON_SHOOT | TH_BUTTON_ENTER;
    if (held & PAD_CIRCLE)
        out |= TH_BUTTON_BOMB;
    if (held & PAD_TRIANGLE)
        out |= TH_BUTTON_BOMB;
    if (held & PAD_SQUARE)
        out |= TH_BUTTON_FOCUS;
    if (held & (PAD_L1 | PAD_R1 | PAD_L2 | PAD_R2))
        out |= TH_BUTTON_FOCUS;
    if (held & PAD_START)
        out |= TH_BUTTON_MENU;
    if (held & PAD_SELECT)
        out |= TH_BUTTON_SKIP;

    // Left analog stick mirrors the d-pad
    if (pad.mode >> 4 == 0x7 || pad.mode >> 4 == 0x5) // dualshock / analog
    {
        if (pad.ljoy_h < 128 - STICK_DEADZONE)
            out |= TH_BUTTON_LEFT;
        if (pad.ljoy_h > 128 + STICK_DEADZONE)
            out |= TH_BUTTON_RIGHT;
        if (pad.ljoy_v < 128 - STICK_DEADZONE)
            out |= TH_BUTTON_UP;
        if (pad.ljoy_v > 128 + STICK_DEADZONE)
            out |= TH_BUTTON_DOWN;
    }

    return out;
}
