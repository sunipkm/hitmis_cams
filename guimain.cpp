// Dear ImGui: standalone example application for DirectX 9
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#include "imgui/imgui.h"
#include "backend/imgui_impl_dx9.h"
#include "backend/imgui_impl_win32.h"
#include <windows.h>
#include <d3d9.h>
#include <tchar.h>
#include <strsafe.h>
#include <stdint.h>
#include <stdlib.h>
#include "CameraUnit_PI.h"
#include "jpge.h"

#include <D3dx9tex.h>
#pragma comment(lib, "D3dx9")

// Data
static LPDIRECT3D9 g_pD3D = NULL;
static LPDIRECT3DDEVICE9 g_pd3dDevice = NULL;
static D3DPRESENT_PARAMETERS g_d3dpp = {};

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void ResetDevice();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool done = false;

typedef struct
{
    LPCRITICAL_SECTION lock;
    int size;
    uint8_t *data;
} jpg_img;

#define W(x) W_(x)
#define W_(x) L##x
#define N(x) x
#define STR(x, t) STR_(x, t)
#define STR_(x, t) t(#x)
#define LOCATION_(t) t(__FILE__) t("(") STR(__LINE__, t) t(")")
#define LOCATION LOCATION_(N)
#define WLOCATION LOCATION_(W)

#define printts(str, ...)                                                                   \
    {                                                                                       \
        TCHAR msg[1024];                                                                    \
        size_t msg_len;                                                                     \
        DWORD out_len;                                                                      \
        StringCchPrintf(msg, sizeof(msg), TEXT("%d: " W(str)), __LINE__, ##__VA_ARGS__);    \
        StringCchLength(msg, sizeof(msg), &msg_len);                                        \
        WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), msg, (DWORD)msg_len, &out_len, NULL); \
    }

CCameraUnit *cam;
double cam_temperature = 0;

DWORD WINAPI ImageGenFunction(LPVOID _img)
{
    static int height = cam->GetCCDHeight();
    static int width = cam->GetCCDWidth();
    jpg_img *img = (jpg_img *)_img;
    // printf("Thread: Ptr %p\n", _img);
    while (!done)
    {
        long retryCount = 1;
        // cam->SetExposure(0.1); // 20 ms
        cam_temperature = cam->GetTemperature();
        cam->SetReadout(1);
        CImageData imgdata = cam->CaptureImage(retryCount);
        // printf("Capture complete\n");
        if (!imgdata.HasData())
            goto wait;
        height = imgdata.GetImageHeight();
        width = imgdata.GetImageWidth();
        int img_sz = height * width;
        uint16_t *imgptr = imgdata.GetImageData();
        uint8_t *data = (uint8_t *)malloc(img_sz * 3); // 3 channels
        printf("Capture size: %d x %d = %d, imgptr: %p, dataptr: %p\n", width, height, img_sz, imgptr, data);
        for (int i = 0; i < img_sz; i++)
        {
            int idx = 3 * i;
            uint8_t tmp = imgptr[i] / 0x100;
            data[idx] = tmp;
            if (imgptr[i] == 0xffff) // saturated
            {
                data[idx + 1] = 0;
                data[idx + 2] = 0;
            }
            else
            {
                data[idx + 1] = tmp;
                data[idx + 2] = tmp;
            }
        }
        // printf("Image conversion complete\n");
        // JPEG output buffer, has to be larger than expected JPEG size
        uint8_t *j_data = (uint8_t *)malloc(height * width * 4 + 1024);
        int j_data_sz = (height * width * 4 + 1024);
        // JPEG parameters
        jpge::params params;
        params.m_quality = 100;
        params.m_subsampling = static_cast<jpge::subsampling_t>(0); // grey
        // JPEG compression and image update
        if (!jpge::compress_image_to_jpeg_file_in_memory(j_data, j_data_sz, width, height, 1, data, params))
        {
            printf("Failed to compress image to jpeg in memory\n");
        }
        else
        {
            EnterCriticalSection(img->lock);
            if (img->size > 0)
                free(img->data);

            img->data = (uint8_t *)malloc(j_data_sz);
            memcpy(img->data, j_data, j_data_sz);
            img->size = j_data_sz;

            LeaveCriticalSection(img->lock);
            printf("Image size: %d\n", img->size);
        }
        // Free memory
        free(data);
        free(j_data);
wait:
        Sleep(1000); // every second
    }
    if (img->size)
        free(img->data);
    return NULL;
}

#define D3DXLoadTextureFromFileMemEx(dev, img, size, width, height, ptexture) \
    D3DXCreateTextureFromFileInMemoryEx(dev,                                  \
                                        img,                                  \
                                        size,                                 \
                                        width,                                \
                                        height,                               \
                                        D3DX_DEFAULT,                         \
                                        0,                                    \
                                        D3DFMT_FROM_FILE,                     \
                                        D3DPOOL_DEFAULT,                      \
                                        D3DX_FILTER_POINT,                    \
                                        D3DX_DEFAULT,                         \
                                        0,                                    \
                                        NULL,                                 \
                                        NULL,                                 \
                                        ptexture)

/**
 * @brief Load texture from file in memory
 * 
 * @param img Pointer to file in memory
 * @param size Size of file in memory
 * @param out_texture Texture output. MUST be initialized to NULL.
 * @param out_width int width
 * @param out_height int height
 * @return true Succes
 * @return false Failure
 */
bool LoadTextureFromMemFile(const uint8_t *img, const int size, PDIRECT3DTEXTURE9 &out_texture, int &out_width, int &out_height)
{

    PDIRECT3DTEXTURE9 texture = NULL; // local texture, will be released when this function is called the next time

    // HRESULT hr = D3DXCreateTextureFromFileInMemory(g_pd3dDevice, img, size, &texture); // load image file to texture
    HRESULT hr = D3DXLoadTextureFromFileMemEx(g_pd3dDevice, img, size, out_width, out_height, &texture); // load image file to texture
    if (hr != S_OK)
        return false;

    if (out_texture) // if texture was loaded before
    {
        out_texture->Release(); // release the texture
        out_texture = NULL;
    }

    out_texture = texture; // assign texture to output

    D3DSURFACE_DESC img_desc;
    out_texture->GetLevelDesc(0, &img_desc);
    out_width = (int)img_desc.Width;
    out_height = (int)img_desc.Height;
    return true;
}
// Main code
int main(int, char **)
{
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEX wc = {sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("ImGui Example"), NULL};
    ::RegisterClassEx(&wc);
    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("Dear ImGui DirectX9 Image Example"), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // PiCam
    cam = new CCameraUnit_PI();  // open camera
    if (!cam->CameraReady())
        goto end2;
    cam->SetTemperature(-60);
    cam->SetBinningAndROI(1, 1); // binning 1x1, full image
    cam->SetExposure(0.001);      // 0.01 ms
    // cam->SetReadout(1);
    // goto end2;

    // Image object
    jpg_img img[1];
    CRITICAL_SECTION img_crit;
    img->lock = &img_crit;
    InitializeCriticalSection(img->lock);
    img->data = NULL;
    img->size = 0;
    printf("Main: Ptr %p\n", img);
    Sleep(1000);
    // Load Image Creator Thread
    DWORD threadId;
    HANDLE hThread = CreateThread(NULL, 0, ImageGenFunction, img, 0, &threadId);

    if (hThread == NULL)
    {
        printf("Could not create thread\n");
        goto end;
    }

    // Main loop
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Start the Dear ImGui frame
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);             // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float *)&clear_color); // Edit 3 floats representing a color
            
            ImGui::Text("CCD Temperature: %lf", cam_temperature);

            static int img_width = 0;
            static int img_height = 0;
            static PDIRECT3DTEXTURE9 img_texture = NULL;

            EnterCriticalSection(img->lock); // acquire lock before loading texture
            if (img->size)                   // image available
            {
                LoadTextureFromMemFile(img->data, img->size, img_texture, img_width, img_height);
            }
            LeaveCriticalSection(img->lock); // release lock
            ImGui::Text("Image: %d x %d pixels", img_width, img_height);
            if (img_height > 0) // image can be shown
            {
                ImGui::Image((void *)img_texture, ImVec2(img_width, img_height)); // show image
            }
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // Rendering
        ImGui::EndFrame();
        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clear_color.x * clear_color.w * 255.0f), (int)(clear_color.y * clear_color.w * 255.0f), (int)(clear_color.z * clear_color.w * 255.0f), (int)(clear_color.w * 255.0f));
        g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
        if (g_pd3dDevice->BeginScene() >= 0)
        {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }

        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        HRESULT result = g_pd3dDevice->Present(NULL, NULL, NULL, NULL);

        // Handle loss of D3D9 device
        if (result == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
            ResetDevice();
    }
    cam->CancelCapture();
    WaitForMultipleObjects(1, &hThread, true, 1200);
    CloseHandle(hThread);
end:
    DeleteCriticalSection(img->lock);
end2:
    printf("Exiting\n");
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL)
        return false;

    // Create the D3DDevice
    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE; // Present with vsync
    //g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;   // Present without vsync, maximum unthrottled framerate
    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
        return false;

    return true;
}

void CleanupDeviceD3D()
{
    if (g_pd3dDevice)
    {
        g_pd3dDevice->Release();
        g_pd3dDevice = NULL;
    }
    if (g_pD3D)
    {
        g_pD3D->Release();
        g_pD3D = NULL;
    }
}

void ResetDevice()
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    if (hr == D3DERR_INVALIDCALL)
        IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0 // From Windows SDK 8.1+ headers
#endif

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            g_d3dpp.BackBufferWidth = LOWORD(lParam);
            g_d3dpp.BackBufferHeight = HIWORD(lParam);
            ResetDevice();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    case WM_DPICHANGED:
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports)
        {
            //const int dpi = HIWORD(wParam);
            //printf("WM_DPICHANGED to %d (%.0f%%)\n", dpi, (float)dpi / 96.0f * 100.0f);
            const RECT *suggested_rect = (RECT *)lParam;
            ::SetWindowPos(hWnd, NULL, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        break;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
