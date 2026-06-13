#pragma once

#include "inttypes.hpp"

// Native PS2 pad (joypad 1, port 0 slot 0). Deliberately avoids the multitap,
// whose RPC init hangs on some emulator/console setups.
namespace Ps2Pad
{
// Loads the sio2man/padman IOP modules and opens port 0. Safe to call once.
bool Init();

// Reads joypad 1 and returns the TouhouButton bitmask for this frame.
u16 GetButtons();
}; // namespace Ps2Pad
