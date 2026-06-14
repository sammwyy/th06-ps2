#include <SDL2/SDL.h>
#include <cstdio>

#include "AnmManager.hpp"
#include "Ps2Pad.hpp"
#include "MemAlloc.hpp"
#include "FileManager.hpp"
#include "StartScreen.hpp"
#include "Chain.hpp"
#include "FileSystem.hpp"
#include "GameErrorContext.hpp"
#include "GameWindow.hpp"
#include "SoundPlayer.hpp"
#include "Stage.hpp"
#include "Supervisor.hpp"
#include "ZunResult.hpp"
#include "i18n.hpp"
#include "utils.hpp"

int main(int argc, char *argv[])
{
    i32 renderResult = 0;

    // Unbuffered stdout so debug logs survive a crash / hang
    setbuf(stdout, NULL);

    // Resolve game data relative to wherever the .elf was launched from
    FileSystem::SetBasePath(argc > 0 ? argv[0] : NULL);

    // SDL's PS2 joystick backend goes through the multitap (mtapInit), which
    // hangs when the multitap RPC never comes up. Read joypad 1 natively instead.
    Ps2Pad::Init();

    if (!MemArenas::InitAll())
    {
        utils::DebugPrint2("boot: arena init failed");
        return -1;
    }

    g_FileManager.Init(StartScreen::Run());

    //    MSG msg;
    //    i32 waste1, waste2, waste3, waste4, waste5, waste6;

    //    if (utils::CheckForRunningGameInstance())
    //    {
    //        g_GameErrorContext.Flush();
    //
    //        return 1;
    //    }

    //    g_Supervisor.hInstance = hInstance;

    utils::DebugPrint2("boot: loading config %s\n", TH_CONFIG_FILE);
    if (g_Supervisor.LoadConfig(TH_CONFIG_FILE) != ZUN_SUCCESS)
    {
        utils::DebugPrint2("boot: LoadConfig failed, aborting\n");
        g_GameErrorContext.Flush();
        return -1;
    }

    //    if (GameWindow::InitD3dInterface())
    //    {
    //        g_GameErrorContext.Flush();
    //        return 1;
    //    }

    //    SystemParametersInfo(SPI_GETSCREENSAVEACTIVE, 0, &g_GameWindow.screenSaveActive, 0);
    //    SystemParametersInfo(SPI_GETLOWPOWERACTIVE, 0, &g_GameWindow.lowPowerActive, 0);
    //    SystemParametersInfo(SPI_GETPOWEROFFACTIVE, 0, &g_GameWindow.powerOffActive, 0);
    //    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 0, NULL, SPIF_SENDCHANGE);
    //    SystemParametersInfo(SPI_SETLOWPOWERACTIVE, 0, NULL, SPIF_SENDCHANGE);
    //    SystemParametersInfo(SPI_SETPOWEROFFACTIVE, 0, NULL, SPIF_SENDCHANGE);

restart:
    utils::DebugPrint2("boot: creating game window / renderer\n");
    GameWindow::CreateGameWindow();

    g_AnmManager = new AnmManager();

    utils::DebugPrint2("boot: init rendering\n");
    if (GameWindow::InitD3dRendering() != ZUN_SUCCESS)
    {
        utils::DebugPrint2("boot: InitD3dRendering failed, aborting\n");
        g_GameErrorContext.Flush();
        return 1;
    }

    utils::DebugPrint2("boot: init sound / input\n");
    g_SoundPlayer.InitializeDSound();
    Controller::GetJoystickCaps();
    Controller::ResetKeyboard();

    utils::DebugPrint2("boot: registering supervisor chain\n");
    if (Supervisor::RegisterChain() != ZUN_SUCCESS)
    {
        utils::DebugPrint2("boot: RegisterChain failed, stopping\n");
        goto stop;
    }

    utils::DebugPrint2("boot: entering main loop\n");
    g_GameWindow.curFrame = 0;

    while (true)
    {
        SDL_Event e;

        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
            {
                goto stop;
            }
        }

        renderResult = g_GameWindow.Render();
        if (renderResult != 0)
        {
            break;
        }

        //        SDL_Delay(1000.0f / 60.0f);

        //        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        //        {
        //            TranslateMessage(&msg);
        //            DispatchMessage(&msg);
        //        }
        //        else
        //        {
        //            testCoopLevelRes = g_Supervisor.d3dDevice->TestCooperativeLevel();
        //            if (testCoopLevelRes == D3D_OK)
        //            {
        //                renderResult = g_GameWindow.Render();
        //                if (renderResult != 0)
        //                {
        //                    goto stop;
        //                }
        //            }
        //            else if (testCoopLevelRes == D3DERR_DEVICENOTRESET)
        //            {
        //                g_AnmManager->ReleaseSurfaces();
        //                testResetRes = g_Supervisor.d3dDevice->Reset(&g_Supervisor.presentParameters);
        //                if (testResetRes != 0)
        //                {
        //                    goto stop;
        //                }
        //                GameWindow::InitD3dDevice();
        //                g_Supervisor.unk198 = 3;
        //            }
        //        }
    }

stop:
    g_Chain.Release();
    g_SoundPlayer.Release();

    delete g_AnmManager;
    g_AnmManager = NULL;

    if (g_GfxBackend != NULL)
        delete g_GfxBackend;
    SDL_Quit();

    if (renderResult == 2)
    {
        g_GameErrorContext.ResetContext();

        g_GameErrorContext.Log(TH_ERR_OPTION_CHANGED_RESTART);

        goto restart;
    }

    g_FileManager.Write(SAVE_FILE_CONFIG, &g_Supervisor.cfg, sizeof(g_Supervisor.cfg));

    MemArenas::DestroyAll();
    g_GameErrorContext.Flush();
    utils::DebugPrint2("boot: clean exit");
    return 0;
}
