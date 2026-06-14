#include "StartScreen.hpp"
#include "Controller.hpp"
#include "Ps2Pad.hpp"
#include "utils.hpp"

#include <dmaKit.h>
#include <gsKit.h>
#include <gsToolkit.h>

namespace
{
const char *const OPTION_LABELS[] = {
    "Memory Card 1   (mc0:/TH06)",
    "Memory Card 2   (mc1:/TH06)",
    "Do not save or load",
};

const StorageTarget OPTION_TARGETS[] = {
    StorageTarget::MemoryCard1,
    StorageTarget::MemoryCard2,
    StorageTarget::None,
};

const int OPTION_COUNT = 3;
} // namespace

StorageTarget StartScreen::Run()
{
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8,
                1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    GSGLOBAL *gs = gsKit_init_global();
    gsKit_init_screen(gs);
    gsKit_mode_switch(gs, GS_ONESHOT);
    gsKit_TexManager_init(gs);

    GSFONTM *fontm = gsKit_init_fontm();
    gsKit_fontm_upload(gs, fontm);

    u64 background = GS_SETREG_RGBAQ(0x10, 0x10, 0x20, 0x80, 0x00);
    u64 titleColor = GS_SETREG_RGBAQ(0xFF, 0xFF, 0xFF, 0x80, 0x00);
    u64 promptColor = GS_SETREG_RGBAQ(0xC0, 0xC0, 0xC0, 0x80, 0x00);
    u64 normalColor = GS_SETREG_RGBAQ(0x90, 0x90, 0x90, 0x80, 0x00);
    u64 selectedColor = GS_SETREG_RGBAQ(0xFF, 0xD0, 0x40, 0x80, 0x00);

    int selected = 0;
    u16 prevButtons = 0;

    while (true)
    {
        u16 buttons = Ps2Pad::GetButtons();
        u16 pressed = buttons & ~prevButtons;
        prevButtons = buttons;

        if (pressed & TH_BUTTON_UP)
        {
            selected = (selected + OPTION_COUNT - 1) % OPTION_COUNT;
        }
        if (pressed & TH_BUTTON_DOWN)
        {
            selected = (selected + 1) % OPTION_COUNT;
        }
        if (pressed & TH_BUTTON_ENTER)
        {
            break;
        }

        gsKit_clear(gs, background);

        gsKit_fontm_print_scaled(gs, fontm, 96.0f, 70.0f, 3, 0.70f, titleColor, "Touhou 6  -  PS2 Port");
        gsKit_fontm_print_scaled(gs, fontm, 96.0f, 130.0f, 3, 0.50f, promptColor,
                                 "Where do you want to save your files?");

        for (int i = 0; i < OPTION_COUNT; i++)
        {
            u64 color = (i == selected) ? selectedColor : normalColor;
            float y = 200.0f + i * 46.0f;
            gsKit_fontm_print_scaled(gs, fontm, 120.0f, y, 3, 0.55f, color, (i == selected) ? ">" : " ");
            gsKit_fontm_print_scaled(gs, fontm, 150.0f, y, 3, 0.55f, color, OPTION_LABELS[i]);
        }

        gsKit_fontm_print_scaled(gs, fontm, 96.0f, 400.0f, 3, 0.42f, promptColor,
                                 "Up / Down to choose, Cross to confirm");

        gsKit_queue_exec(gs);
        gsKit_sync_flip(gs);
    }

    utils::DebugPrint2("StartScreen: selected option %d", selected);
    return OPTION_TARGETS[selected];
}
