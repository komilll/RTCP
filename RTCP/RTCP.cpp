// RTCP.cpp - aplication entry point

#include "pch.h"
#include "Main.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pScmdline, int iCmdshow)
{
    HRESULT result = S_OK;

    CBuffer<SceneConstantBuffer> cb{};
    RaytracingResources* raytracing = new RaytracingResources();
    raytracing->CreateRaytracingPipeline(cb);

    return 0;
}

    //// Create device manager (needed to initialzie Main)
    //std::shared_ptr<DeviceManager> deviceManager = std::shared_ptr<DeviceManager>(new DeviceManager());

    //// Initialize window
    //std::shared_ptr<Main> main = std::shared_ptr<Main>(new Main());
    //result = main->InitializeWindow(hInstance, deviceManager);

    //if (SUCCEEDED(result))
    //{
    //    // Prepare device manager resources
    //    deviceManager->InitializeDeviceResponsibilities(main->GetWindowHandle());

    //    // Prepare renderer resources
    //    std::shared_ptr<Renderer> renderer = std::shared_ptr<Renderer>(new Renderer(deviceManager, main->GetWindowHandle()));
    //    renderer->OnInit(main->GetWindowHandle());

    //    // Run main loop
    //    main->Run(deviceManager, renderer);
    //}

    //return 0;
//}