// Dear ImGui: standalone example application for DirectX 9
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs
#define ANDOR
#include "imgui/imgui.h"
#include "backend/imgui_impl_dx9.h"
#include "backend/imgui_impl_win32.h"
#include "implot/implot.h"
#include <windows.h>
#include <d3d9.h>
#include <tchar.h>
#include <strsafe.h>
#include <stdint.h>
#include <stdlib.h>
#include "atlbase.h"
#include "atlstr.h"
#include "comutil.h"
#include "CameraUnit_PI.h"
#include "CameraUnit_ANDORUSB.h"
#include "jpge.h"
#include "fitsio.h"
#include <chrono>

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

char GlobalCameraName[20];

bool done = false;

uint64_t getTime()
{
    return ((std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now())).time_since_epoch())).count());
}

typedef struct
{
    LPCRITICAL_SECTION lock;
    int size;
    uint8_t *data;
    int width;
    int height;
    bool data_avail;
    bool new_data;
} jpg_img;

typedef struct
{
    int height;
    int width;
    PDIRECT3DTEXTURE9 texture;
    void Reset()
    {
        height = 0;
        width = 0;
        if (texture)
            texture->Release();
        texture = 0;
    };
} d3d9_texture;

typedef struct
{
    LPCRITICAL_SECTION lock = 0;

    uint64_t tstamp;
    uint32_t exposure_ms;
    uint16_t binX;
    uint16_t binY;
    uint16_t x_min;
    uint16_t x_max;
    uint16_t y_min;
    uint16_t y_max;
    float ccdtemp;
    uint16_t *data;
    int size;
    int width;
    int height;
    bool data_avail;
    bool new_data;

    uint16_t data_min;
    uint16_t data_max;
    double data_average;

    float *histdata;
    int histdata_len;
    int histdata_min;
    int histdata_max;
    float histdata_maxcount;
    float *xdata;   // average of all Y points
    float *xpoints; // x axis
    int xdata_len;
    float *ydata;   // average of all X points
    float *ypoints; // y axis
    int ydata_len;

    void Init()
    {
        tstamp = 0;
        exposure_ms = 0;
        binX = 0;
        binY = 0;
        x_min = 0;
        x_max = 0;
        y_min = 0;
        y_max = 0;
        ccdtemp = 0;
        data = 0;
        size = 0;
        width = 0;
        height = 0;
        data_avail = 0;
        new_data = 0;
        data_min = 0;
        data_max = 0;
        data_average = 0;
        xdata = 0;
        ydata = 0;
        histdata = 0;
        xpoints = 0;
        ypoints = 0;
        xdata_len = 0;
        ydata_len = 0;
        histdata_len = 0;
        histdata_min = 0;
        histdata_max = 0;
        histdata_maxcount = 0;
    };

    void Reset()
    {
        if (data)
            delete[] data;
        if (histdata)
            delete[] histdata;
        if (xdata)
            delete[] xdata;
        if (ydata)
            delete[] ydata;
        if (xpoints)
            delete[] xpoints;
        if (ypoints)
            delete[] ypoints;
        Init();
    };
} raw_image;

typedef struct
{
    raw_image *raw = NULL;
    jpg_img *jpg = NULL;
    int min = 0;
    int max = 0;
    bool adjust = false;
} JpegLoaderInOut;

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

CCameraUnit *cam = NULL;
double cam_temperature = 0;
unsigned int Cadence = 1000;
bool CadenceChange = false;
bool TakeSingleShot = false;
bool exposure_mode = true; // true == Continuous, false == Single shot
int bin_roi[6] = {0, 0, 0, 0, 0, 0};
bool RoiUpdate = false;
bool RoiUpdated = false;
bool CapturingImage = false;

raw_image main_image[1];

bool SaveImageCommand = false;
int SaveImageNum = 0;
int CurrentSaveImageNum = 0;
char SaveImagePrefix[20] = "hitmis";
char SaveImageDir[100] = ".\\fits\\";

DWORD WINAPI ImageGenFunction(LPVOID _img)
{
    static int height = cam->GetCCDHeight();
    static int width = cam->GetCCDWidth();
    jpg_img *img = (jpg_img *)_img;
    img->data_avail = false;
    main_image->Init();
    // printf("Thread: Ptr %p\n", _img);
    while (!done)
    {
        long retryCount = 1;
        // cam->SetExposure(0.1); // 20 ms
        cam_temperature = cam->GetTemperature();
        if (RoiUpdate)
        {
            cam->SetBinningAndROI(bin_roi[0], bin_roi[1], bin_roi[2], bin_roi[3], bin_roi[4], bin_roi[5]);
            RoiUpdate = false;
            RoiUpdated = true;
        }
        // cam->SetReadout(1);
        if (exposure_mode || TakeSingleShot || (SaveImageCommand && (CurrentSaveImageNum < SaveImageNum)))
        {
            CapturingImage = true;
            CImageData imgdata = cam->CaptureImage(retryCount);
            // printf("Capture complete\n");
            if (!imgdata.HasData())
                goto wait;
            height = imgdata.GetImageHeight();
            width = imgdata.GetImageWidth();
            int img_sz = height * width;
            uint16_t *imgptr = imgdata.GetImageData();
            EnterCriticalSection(main_image->lock);
            main_image->Reset();
            main_image->data = new uint16_t[img_sz];
            main_image->size = img_sz;
            main_image->width = width;
            main_image->height = height;
            main_image->xdata = new float[width];
            main_image->xpoints = new float[width];
            main_image->xdata_len = width;
            memset(main_image->xdata, 0x0, width * sizeof(float));
            main_image->ydata = new float[height];
            main_image->ypoints = new float[height];
            main_image->ydata_len = height;
            memset(main_image->ydata, 0x0, height * sizeof(float));
            main_image->data_min = 0xffff; // max
            main_image->data_max = 0x0;    // min
            main_image->tstamp = getTime();
            main_image->exposure_ms = cam->GetExposure() * 1000;
            main_image->binX = cam->GetBinningX();
            main_image->binY = cam->GetBinningY();
            main_image->x_min = (cam->GetROI())->x_min;
            main_image->x_max = (cam->GetROI())->x_max;
            main_image->y_min = (cam->GetROI())->y_min;
            main_image->y_max = (cam->GetROI())->y_max;
            main_image->ccdtemp = cam->GetTemperature();
            printf("Capture size: %d x %d = %d\n", width, height, img_sz);
            for (int i = 0; i < img_sz; i++)
            {
                // memcpy and findmax
                (main_image->data)[i] = imgptr[i];
                if (imgptr[i] < main_image->data_min)
                    main_image->data_min = imgptr[i];
                if (imgptr[i] > main_image->data_max)
                    main_image->data_max = imgptr[i];
                // average
                main_image->data_average += imgptr[i];
                // cross-histogram
                (main_image->xdata)[i % width] += imgptr[i];
                (main_image->ydata)[i / width] += imgptr[i];
                // // full histogram
                // int hidx = (((float)imgptr[i]) / 0xffff) * (main_image->histdata_len - 1);
                // (main_image->histdata)[hidx]++;
            }
            // histogram normalization
            for (int i = 0; i < width; i++)
            {
                (main_image->xdata)[i] /= height;
                (main_image->xpoints)[i] = i;
            }
            for (int i = 0; i < height; i++)
            {
                (main_image->ydata)[i] /= width;
                (main_image->ypoints)[i] = i;
            }
            // full histogram
            {
                int min_bin = main_image->data_min, _min_bin;
                int max_bin = main_image->data_max, _max_bin;
                int bin_width = 0;
                int num_bins = 0;
                int hist_range = 0;
                do
                {
                    bin_width += 10;
                    _min_bin = (min_bin / bin_width) * bin_width;
                    _max_bin = (max_bin / bin_width + 1) * bin_width;
                    num_bins = (_max_bin - _min_bin) / bin_width;
                } while (num_bins > 100);
                min_bin = _min_bin;
                max_bin = _max_bin;
                main_image->histdata = new float[num_bins];
                main_image->histdata_len = num_bins;
                memset(main_image->histdata, 0x0, main_image->histdata_len * sizeof(float));
                hist_range = max_bin - min_bin;
                main_image->histdata_min = min_bin;
                main_image->histdata_max = max_bin;
                for (int i = 0; i < main_image->size; i++)
                {
                    int hidx = (((float)imgptr[i] - main_image->data_min) / hist_range) * (main_image->histdata_len - 1);
                    (main_image->histdata)[hidx]++;
                }
                for (int i = 0; i < main_image->histdata_len; i++)
                    if (main_image->histdata_maxcount < (main_image->histdata)[i])
                        main_image->histdata_maxcount = (main_image->histdata)[i];
            }
            main_image->data_average /= main_image->size;
            main_image->data_avail = true;
            main_image->new_data = true;
            LeaveCriticalSection(main_image->lock);
            // printf("Image conversion complete\n");

            if (TakeSingleShot)
                TakeSingleShot = false;
            CapturingImage = false;
            if (exposure_mode || (SaveImageCommand && (CurrentSaveImageNum < SaveImageNum)))
            {
                unsigned int _Cadence = Cadence - 40, sleepAmt = 0;
                while ((sleepAmt < _Cadence) && exposure_mode)
                {
                    if (CadenceChange)
                    {
                        _Cadence = Cadence - 40;
                        CadenceChange = false;
                        if (sleepAmt >= _Cadence)
                            break;
                    }
                    Sleep(40);
                    sleepAmt += 40;
                }
            }
        }
    wait:
        Sleep(40); // error case or single shot case
    }
    if (img->size)
        free(img->data);
    return NULL;
}

int _compare_uint16(const void *a, const void *b)
{
    return (*((unsigned short *)a) - *((unsigned short *)b));
}
static bool EnableAutoExposure = false;
static bool HaveNewAutoExposure = false;
static float AutoExposureVal = 0.001;
static int FindExposureMode = 90 * 10;
bool FindExposureMedian = false;
static float PixelTargetValue = 40000.0;
static float PixelTargetUncertainty = 5000.0;
static float MaxAllowedOptimumExposure = 10;

void FindOptimumExposure(raw_image *image)
{
    double result = image->exposure_ms * 0.001;
    double exposure = image->exposure_ms * 0.001;
    AutoExposureVal = exposure;
    printf("Input: %lf s\n", exposure);
    double val;
    uint16_t *picdata = new uint16_t[image->size];
    memcpy(picdata, image->data, image->size * sizeof(uint16_t));
    qsort(picdata, image->size, sizeof(unsigned short), _compare_uint16);

    if (FindExposureMedian)
    {
        if (image->size)
            val = (picdata[image->size / 2] + picdata[image->size / 2 + 1]) * 0.5;
        else
            val = picdata[image->size / 2];
    }
    else
    {
        bool direction;
        if (picdata[0] < picdata[image->size - 1])
            direction = true;
        else
            direction = false;
        unsigned int coord = floor((FindExposureMode * (image->size - 1) * 0.001));
        if (direction)
            val = picdata[coord];
        else
        {
            if (coord == 0)
                coord = 1;
            val = picdata[image->size - coord];
        }
    }

    printf("Uncertainty: %lf, Reference: %lf\n", fabs(PixelTargetValue - val), PixelTargetUncertainty);
    if (fabs(PixelTargetValue - val) < PixelTargetUncertainty)
    {
        goto ret;
    }

    /** If calculated median pixel is within PixelTargetValue +/- PixelTargetUncertainty, return current exposure **/

    result = ((double)PixelTargetValue) * exposure / ((double)val);

    if (result > MaxAllowedOptimumExposure)
        result = MaxAllowedOptimumExposure;
    // round to 1 ms
    result = ((int)(result * 1000)) * 0.001;
    AutoExposureVal = result;
ret:
    HaveNewAutoExposure = true;
}

int JpegQuality = 70;

char SaveFitsStatus[30];

void SaveFits(char *filePrefix, char *DirPrefix, int i, int n, raw_image *image)
{
    static char defaultFilePrefix[] = "hitmis";
    static char defaultDirPrefix[] = ".\\fits\\";
    if ((filePrefix == NULL) || (strlen(filePrefix) == 0))
        filePrefix = defaultFilePrefix;
    if ((DirPrefix == NULL) || (strlen(DirPrefix) == 0))
        DirPrefix = defaultFilePrefix;
    char fileName[256];
    _snprintf(fileName, sizeof(fileName), "%s\\%s_%ums_%d_%d_%llu.fit[compress]", DirPrefix, filePrefix, image->exposure_ms, i, n, image->tstamp);
    fitsfile *fptr;
    int status = 0, bitpix = USHORT_IMG, naxis = 2;
    int bzero = 32768, bscale = 1;
    long naxes[2] = {(long)(image->width), (long)(image->height)};
    unlink(fileName);
    if (!fits_create_file(&fptr, fileName, &status))
    {
        fits_create_img(fptr, bitpix, naxis, naxes, &status);
        fits_write_key(fptr, TSTRING, "PROGRAM", (void *)"hitmis_explorer", NULL, &status);
        fits_write_key(fptr, TSTRING, "CAMERA", (void *)GlobalCameraName, NULL, &status);
        fits_write_key(fptr, TULONGLONG, "TIMESTAMP", &(image->tstamp), NULL, &status);
        fits_write_key(fptr, TUSHORT, "BZERO", &bzero, NULL, &status);
        fits_write_key(fptr, TUSHORT, "BSCALE", &bscale, NULL, &status);
        fits_write_key(fptr, TFLOAT, "CCDTEMP", &(image->ccdtemp), NULL, &status);
        fits_write_key(fptr, TUINT, "EXPOSURE_MS", &(image->exposure_ms), NULL, &status);
        fits_write_key(fptr, TUSHORT, "BINX", &(image->binX), NULL, &status);
        fits_write_key(fptr, TUSHORT, "BINY", &(image->binY), NULL, &status);
        fits_write_key(fptr, TUSHORT, "MINX", &(image->x_min), NULL, &status);
        fits_write_key(fptr, TUSHORT, "MAXX", &(image->x_max), NULL, &status);
        fits_write_key(fptr, TUSHORT, "MINY", &(image->y_min), NULL, &status);
        fits_write_key(fptr, TUSHORT, "MAXY", &(image->y_max), NULL, &status);

        long fpixel[] = {1, 1};
        fits_write_pix(fptr, TUSHORT, fpixel, (image->width) * (image->height), image->data, &status);
        fits_close_file(fptr, &status);
        _snprintf(SaveFitsStatus, sizeof(SaveFitsStatus), "saved %d of %d", i, n);
    }
    else
    {
        _snprintf(SaveFitsStatus, sizeof(SaveFitsStatus), "failed %d of %d", i, n);
    }
}

bool ConvertRawToJpeg(raw_image *raw, jpg_img *jpg, uint16_t min, uint16_t max)
{
    bool retval = true;
    // source raw image
    int width = raw->width, height = raw->height;
    uint16_t *imgptr = raw->data;
    // temporary bitmap buffer
    uint8_t *data = new uint8_t[width * height * 3]; // 3 channels for RGB
    // Data conversion
    float scale = 0xffff / ((float)(max - min));
    for (int i = 0; i < raw->size; i++) // for each pixel in raw image
    {
        int idx = 3 * i;      // RGB pixel in JPEG source bitmap
        if (imgptr[i] >= max) // saturation
        {
            data[idx + 0] = 0xff;
            data[idx + 1] = 0x0;
            data[idx + 2] = 0x0;
        }
        else // scaling
        {
            uint8_t tmp = (imgptr[i] - min) * scale / 0x100;
            data[idx + 0] = tmp;
            data[idx + 1] = tmp;
            data[idx + 2] = tmp;
        }
    }
    // JPEG output buffer, has to be larger than expected JPEG size
    uint8_t *j_data = new uint8_t[width * height * 4 + 1024]; // extra room for JPEG conversion
    int j_data_sz = (height * width * 4 + 1024);
    // JPEG parameters
    jpge::params params;
    params.m_quality = JpegQuality;
    params.m_subsampling = static_cast<jpge::subsampling_t>(2); // 0 == grey, 2 == RGB
    // JPEG compression and image update
    if (!jpge::compress_image_to_jpeg_file_in_memory(j_data, j_data_sz, width, height, 3, data, params))
    {
        printf("Failed to compress image to jpeg in memory\n");
        retval = false;
    }
    else
    {
        EnterCriticalSection(jpg->lock);
        if (jpg->size > 0)
            delete[] jpg->data;

        jpg->data = new uint8_t[j_data_sz];
        memcpy(jpg->data, j_data, j_data_sz);
        jpg->size = j_data_sz;
        jpg->width = width;
        jpg->height = height;
        jpg->data_avail = true;
        jpg->new_data = true;
        LeaveCriticalSection(jpg->lock);
        printf("JPEG size: %d\n", jpg->size);
    }
    // Free memory
    delete[] data;
    delete[] j_data;
    return retval;
}

// Image Display InOut Struct
JpegLoaderInOut jpeg_inout[1];
bool AutoContrastEn = true;

DWORD WINAPI ImageConvertFunction(LPVOID _in)
{
    JpegLoaderInOut *inout = (JpegLoaderInOut *)_in; // recast
    while (!done)
    {
        if (inout->max <= inout->min)
            inout->max = 0xffff;
        if ((inout->raw == NULL) || (inout->jpg == NULL))
            goto sleep;
        EnterCriticalSection(inout->raw->lock);
        if (inout->raw->new_data) // new data available, make jpeg
        {
            inout->raw->new_data = false;
            if (!AutoContrastEn)
                ConvertRawToJpeg(inout->raw, inout->jpg, inout->min, inout->max);
            else
            {
                uint16_t min = inout->raw->data_min - 50;
                if (min < 0)
                    min = 0;
                uint16_t max = inout->raw->data_max + 50;
                if (max > 0xffff)
                    max = 0xffff;
                ConvertRawToJpeg(inout->raw, inout->jpg, min, max);
                inout->min = min;
                inout->max = max;
            }
            if (SaveImageCommand)
            {
                if (CurrentSaveImageNum < SaveImageNum)
                {
                    SaveFits(SaveImagePrefix, SaveImageDir, CurrentSaveImageNum, SaveImageNum, inout->raw);
                    CurrentSaveImageNum++;
                }
            }
            if (EnableAutoExposure)
            {
                FindOptimumExposure(inout->raw);
                if (HaveNewAutoExposure)
                {
                    if (AutoExposureVal < 0.001)
                        AutoExposureVal = 0.001;
                    cam->SetExposure(AutoExposureVal);
                    HaveNewAutoExposure = false;
                }
            }
        }
        else if (inout->raw->data_avail && inout->adjust) // old data available and adjust request received
        {
            inout->adjust = false;
            ConvertRawToJpeg(inout->raw, inout->jpg, inout->min, inout->max);
        }
    bil:
        LeaveCriticalSection(inout->raw->lock);
    sleep:
        Sleep(20);
    }
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

bool imgwindow_resized = true;
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
bool LoadTextureFromMemFile(jpg_img *jimg, /* const uint8_t *img, const int size, PDIRECT3DTEXTURE9 &out_texture, int &out_width, int &out_height, */ d3d9_texture *t)
{
    if (jimg == NULL)
        return false;
    EnterCriticalSection(jimg->lock);
    bool retval = false;
    uint8_t *img = jimg->data;
    int size = jimg->size;
    if (size == 0)
        goto exit;

    // scaling
    if (t->width && (!t->height)) // Height scaling
    {
        float scale = 1.0 * t->width / jimg->width;
        t->height = scale * jimg->height;
        // printf("Height scaling: Source %d x %d | Output %d x %d | Scale %f\n", jimg->width, jimg->height, t->width, t->height, scale);
    }

    if (t->height && (!t->width)) // Height scaling
    {
        float scale = 1.0 * t->height / jimg->height;
        t->width = scale * jimg->width;
        // printf("Height scaling: Source %d x %d | Output %d x %d | Scale %f\n", jimg->width, jimg->height, t->width, t->height, scale);
    }
    // if window has not been resized or data is not available or texture is valid, keep the old texture
    if ((!imgwindow_resized) && (!jimg->new_data) && (t->texture))
    {
        retval = true;
        goto exit;
    }

    PDIRECT3DTEXTURE9 texture = NULL; // local texture, will be released when this function is called the next time

    // HRESULT hr = D3DXCreateTextureFromFileInMemory(g_pd3dDevice, img, size, &texture); // load image file to texture
    HRESULT hr = D3DXLoadTextureFromFileMemEx(g_pd3dDevice, img, size, t->width, t->height, &texture); // load image file to texture
    if (hr != S_OK)
        goto exit;
    imgwindow_resized = false; // window size has been addressed
    jimg->new_data = false;    // data availability has been addressed

    if (t->texture) // if texture was loaded before
    {
        (t->texture)->Release(); // release the texture
        t->texture = NULL;
    }

    t->texture = texture; // assign texture to output

    D3DSURFACE_DESC img_desc;
    (t->texture)->GetLevelDesc(0, &img_desc);
    t->width = (int)img_desc.Width;
    t->height = (int)img_desc.Height;
    retval = true;

exit:
    LeaveCriticalSection(jimg->lock);
    return retval;
}

jpg_img img[1];
d3d9_texture img_texture[1];
ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

// image lock
CRITICAL_SECTION img_crit;
CRITICAL_SECTION raw_crit;
// Init Work InOut
typedef struct
{
    bool Done;
    bool CamInitCheck;
    bool CamInit;
    bool CaptureInit;
    HWND hwnd;
    HANDLE CaptureThread;
    HANDLE ConvertThread;
    char CameraName[256];
} InitWorkInOut;

// Init Work
DWORD WINAPI CameraInitFunction(LPVOID _inout)
{
    static bool firstRun = true;
    if (firstRun)
    {
        firstRun = false;
        InitWorkInOut *inout = (InitWorkInOut *)_inout;
        inout->CamInit = false;
        inout->CamInitCheck = false;
        inout->CaptureInit = false;
        inout->Done = false;
        // PiCam
        cam = new CCameraUnit_PI(); // try PI
        int cam_number = 0;
        bool cam_rdy = cam->CameraReady();
        if (!cam_rdy)
        {
            printf("PI Pixis not detected. Checking if Andor iKon is available.\n");
            delete cam;                       // not found
            cam = new CCameraUnit_ANDORUSB(); // try ANDOR
            cam_number = 1;
        }
        if (!cam->CameraReady())
        {
            delete cam; // not found
            printf("PI Pixis or Andor iKon-M cameras not detected. Check if cameras are connected, turned on, and appropriate drivers are installed.\n");
            inout->CamInitCheck = true;
            goto end;
        }
        size_t cam_name_sz = sprintf(inout->CameraName, "Camera: %s %s", cam_number ? "Andor iKon-M" : "PI Pixis", cam->CameraName()) + 1;
        _snprintf(GlobalCameraName, sizeof(GlobalCameraName), "%s %s", cam_number ? "Andor iKon-M" : "PI Pixis", cam->CameraName());
        inout->CamInit = true;
        inout->CamInitCheck = true;
        cam->SetTemperature(-60);
        cam->SetBinningAndROI(1, 1); // binning 1x1, full image
        cam->SetExposure(0.001);     // 0.01 ms
        cam->SetReadout(1);
        printf("%s\n", inout->CameraName);
        wchar_t *wcam_name = new wchar_t[cam_name_sz];
        mbstowcs(wcam_name, inout->CameraName, cam_name_sz);

        ::SetWindowText(inout->hwnd, wcam_name);
        ::UpdateWindow(inout->hwnd);
        delete[] wcam_name;

        printf("Main: Ptr %p\n", img);
        Sleep(1000);
        // Load Image Creator Thread
        DWORD threadId;
        inout->CaptureThread = CreateThread(NULL, 0, ImageGenFunction, img, 0, &threadId);

        if (inout->CaptureThread == NULL)
        {
            printf("Could not create thread\n");
            goto end;
        }
        inout->CaptureInit = true;

        // initialize texture
        img_texture->Reset();

        inout->ConvertThread = CreateThread(NULL, 0, ImageConvertFunction, jpeg_inout, 0, &threadId);
        if (inout->ConvertThread == NULL)
        {
            printf("Could not create convert thread\n");
            inout->CaptureInit = false;
            goto end;
        }
    end:
        inout->Done = true;
    }
    return NULL;
}

HWND hwnd;

BOOL WindowPositionGet(HWND h, RECT *rect)
{
    BOOL retval = true;
    RECT wrect;
    retval &= GetWindowRect(h, &wrect);
    RECT crect;
    retval &= GetClientRect(h, &crect);
    POINT lefttop = {crect.left, crect.top}; // Practicaly both are 0
    ClientToScreen(h, &lefttop);
    POINT rightbottom = {crect.right, crect.bottom};
    ClientToScreen(h, &rightbottom);

    // int left_border = lefttop.x - wrect.left;              // Windows 10: includes transparent part
    // int right_border = wrect.right - rightbottom.x;        // As above
    // int bottom_border = wrect.bottom - rightbottom.y;      // As above
    // int top_border_with_title_bar = lefttop.y - wrect.top; // There is no transparent part
    rect->left = lefttop.x;
    rect->right = rightbottom.x;
    rect->top = lefttop.y;
    rect->bottom = rightbottom.y;
    return retval;
}

// Windows
void InitWindow();
void MainWindow();
void ImageWindow(bool *active);

// Main code
int main(int, char **)
{
    // goto end2;
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEX wc = {sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("HiT&MIS Camera Monitor"), NULL};
    ::RegisterClassEx(&wc);
    hwnd = ::CreateWindow(wc.lpszClassName, _T("Camera: Searching"), WS_OVERLAPPEDWINDOW, 100, 100, 420, 470, NULL, NULL, wc.hInstance, NULL);

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
    ImPlot::CreateContext();
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
    InitWorkInOut Init_Params[1];
    Init_Params->hwnd = hwnd;
    memset(Init_Params->CameraName, 0x0, sizeof(Init_Params->CameraName));
    HANDLE InitThreadHandle;

    // Image object
    img->lock = &img_crit;
    main_image->lock = &raw_crit;
    InitializeCriticalSection(img->lock);
    InitializeCriticalSection(main_image->lock);
    img->data = NULL;
    img->size = 0;
    img->height = 0;
    img->width = 0;
    jpeg_inout->adjust = false;
    jpeg_inout->jpg = img;
    jpeg_inout->raw = main_image;
    jpeg_inout->min = 0;
    jpeg_inout->max = 0;

    static bool notDone = true;
    // Main loop
    while (!done)
    {
        static bool firstRun = true;
        static bool cameraInitWorkerRunning = false;
        done = !notDone;
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

        if (firstRun) // Run Init Thread
        {
            DWORD threadID;
            InitThreadHandle = CreateThread(NULL, 0, CameraInitFunction, Init_Params, 0, &threadID);
            cameraInitWorkerRunning = true;
            firstRun = false;
        }
        static int LoadingScreen = 0;
        static int ContinuePrompt = 0;
        static bool allSuccess = false;
        if (cameraInitWorkerRunning)
        {
            static char loadAnim[] = "\\\\\\\\\\|||||/////-----";
            RECT rect;
            int width, height;
            if (WindowPositionGet(hwnd, &rect))
            {
                width = rect.right - rect.left;
                height = rect.bottom - rect.top;
            }
            ImGui::Begin("Camera Initialization Status");
            ImGui::SetWindowSize(ImVec2(width, height), ImGuiCond_Always);
            ImGui::SetWindowPos(ImVec2(rect.left, rect.top), ImGuiCond_Always);
            bool buttonReady = false;
            // printf("Status: %d %d %d %d\n", Init_Params->CamInitCheck, Init_Params->CamInit, Init_Params->CaptureInit, Init_Params->Done);
            if (!(Init_Params->CamInitCheck))
            {
                ImGui::TextColored(ImVec4(0, 0, 1, 1), "Searching for camera");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1, 0, 1, 1), "%c", loadAnim[(LoadingScreen++) % strlen(loadAnim)]);
                goto button;
            }
            if ((Init_Params->CamInitCheck) && (!Init_Params->CamInit) && (Init_Params->Done))
            {
                ImGui::PushStyleColor(0, ImVec4(1, 0, 0, 1));
                ImGui::TextWrapped("Could not find a PI Pixis or Andor iKon-M Camera.\nPlease check power, USB connection and driver installations.\n");
                ImGui::PopStyleColor();
                buttonReady = true;
                goto button;
            }
            if (Init_Params->CamInit)
            {
                ImGui::PushStyleColor(0, ImVec4(20 / 255.0, 153 / 255.0, 33 / 255.0, 1));
                ImGui::Text("Found %s", Init_Params->CameraName);
                ImGui::PopStyleColor();
            }
            if (Init_Params->CamInit && (!Init_Params->CaptureInit))
            {
                ImGui::TextColored(ImVec4(0, 0, 1, 1), "Initializing Camera Capture Thread");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1, 0, 1, 1), "%c", loadAnim[(LoadingScreen++) % strlen(loadAnim)]);
            }
            else if (Init_Params->CamInit && Init_Params->CaptureInit && (!Init_Params->Done))
                ImGui::Text("Initialized Camera Capture Thread, waiting for worker to finish\n");
            else if (Init_Params->CamInit && Init_Params->CaptureInit && Init_Params->Done)
            {
                ImGui::TextColored(ImVec4(0, 204 / 255.0, 1, 1), "Initialization Complete");
                buttonReady = true;
            }
            else
                ImGui::Text("Unknown State");
        button:
            if (!buttonReady)
            {
                ImGui::PushStyleColor(0, ImVec4(0.5, 0.5, 0.5, 0.5));
                ImGui::Button("Continue");
            }
            if (buttonReady)
            {
                if (Init_Params->CamInitCheck && !Init_Params->CaptureInit)
                {
                    ImGui::PushStyleColor(0, ImVec4(0.8, 0, 0, 0.8));
                    if (ImGui::Button("Exit"))
                    {
                        cameraInitWorkerRunning = false;
                    }
                }
                else
                {
                    ImGui::PushStyleColor(0, ImVec4(0, 0.8, 0, 0.8));
                    ContinuePrompt++;
                    if (ImGui::Button("Continue") || ((ContinuePrompt / ImGui::GetIO().Framerate) > 2))
                    {
                        cameraInitWorkerRunning = false;
                        allSuccess = true;
                    }
                }
            }
            ImGui::PopStyleColor();
            ImGui::End();
        }
        else if (allSuccess)
            MainWindow();
        else
            done = 1;

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
    if (Init_Params->CaptureInit)
    {
        cam->CancelCapture();
        HANDLE openHandles[] = {Init_Params->CaptureThread, Init_Params->ConvertThread};
        WaitForMultipleObjects(IM_ARRAYSIZE(openHandles), openHandles, true, 1200);
        CloseHandle(Init_Params->CaptureThread);
        CloseHandle(Init_Params->ConvertThread);
    }
end:
    DeleteCriticalSection(img->lock);

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    WaitForMultipleObjects(1, &InitThreadHandle, true, 1200);
    CloseHandle(InitThreadHandle);

    printf("Exiting\n");
    return 0;
}

void MainWindow()
{
    static float CCDTempSet = -60.0f;
    static bool firstRun = true;
    static bool SaveExposureFiles = false;
    // get windows window position
    RECT rect;
    int width, height;
    if (WindowPositionGet(hwnd, &rect))
    {
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;
    }
    static int old_height = 0, old_width = 0, DisplaySizeInTitle = 0;
    // begin ImGui window
    if (DisplaySizeInTitle == 0)
        ImGui::Begin("Control Panel", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize); // Control Panel Window
    else if (DisplaySizeInTitle > 0)
    {
        DisplaySizeInTitle--;
        char MainWindowName[50];
        _snprintf(MainWindowName, sizeof(MainWindowName), "Control Panel | Window Size %d x %d", width, height);
        ImGui::Begin(MainWindowName, NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize); // Control Panel Window
    }
    else
    {
        DisplaySizeInTitle = 0;
        ImGui::Begin("Control Panel", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize); // Control Panel Window
    }
    // set ImGui window position
    if (height != old_height || width != old_width)
    {
        DisplaySizeInTitle = 3600;
    }
    ImGui::SetWindowSize(ImVec2(width, height), ImGuiCond_Always);
    old_width = width;
    old_height = height;
    ImGui::SetWindowPos(ImVec2(rect.left, rect.top), ImGuiCond_Always);
    // GetWindowRect(hwnd, &rect);
    // printf("%d %d\n", rect.right - rect.left, rect.bottom - rect.top);
    // Begin CCD Temperature Setting
    ImGui::Separator();
    ImGui::Columns(2, "Temp_Column", false);
    ImGui::PushItemWidth(50);
    if (ImGui::InputFloat("CCD Set Temperature", &CCDTempSet, 0, 0, "%.0f C", ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
    {
        // clamps
        if (CCDTempSet < -100.0)
            CCDTempSet = -100.0;
        if (CCDTempSet > 20.0)
            CCDTempSet = 20.0;
        cam->SetTemperature(CCDTempSet);
    }
    ImGui::PopItemWidth();
    ImGui::NextColumn();
    float CCDTempNow = cam_temperature;
    ImGui::PushItemWidth(70);
    if (fabs(CCDTempNow - CCDTempSet) > 5)
        ImGui::PushStyleColor(0, ImVec4(1, 0, 0, 1));
    else
        ImGui::PushStyleColor(0, ImVec4(0, 1, 0, 1));
    ImGui::InputFloat("##CCD Temperature", &CCDTempNow, 0, 0, "%.1f C", ImGuiInputTextFlags_ReadOnly);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::Text("CCD Temperature");
    ImGui::PopItemWidth();
    ImGui::Columns(1);
    // End CCD Temperature Setting

    // Begin Exposure Control
    ImGui::Separator();
    static double CCDExposure = 0;
    if (firstRun || EnableAutoExposure)
    {
        CCDExposure = cam->GetExposure();
    }
    // if (ImGui::InputDouble("Exposure", &CCDExposure, 0, 0, "%.3f s", EnableAutoExposure ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_EnterReturnsTrue))
    // {
    //     if (!CapturingImage)
    //         cam->SetExposure(CCDExposure);
    //     CCDExposure = cam->GetExposure();
    // }
    ImGui::InputDouble("Exposure", &CCDExposure, 0, 0, "%.3f s", EnableAutoExposure ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_EnterReturnsTrue);
    if (!CapturingImage && !EnableAutoExposure)
    {
        if (cam->GetExposure() != CCDExposure)
        {
            cam->SetExposure(CCDExposure);
            CCDExposure = cam->GetExposure();
        }
    }
    ImGui::Separator();
    static bool single_shot = false;
    ImGui::Columns(2, "ExposureControlMode");
    static float LocalCadence = 1.0;
    if (ImGui::RadioButton("Continuous Exposure", exposure_mode))
    {
        single_shot = false;
        exposure_mode = true;
        Cadence = ((unsigned int)(LocalCadence * 1000 / 40)) * 40;
        CadenceChange = true;
    }
    ImGui::NextColumn();
    ImGui::PushItemWidth(100);
    if (ImGui::InputFloat("Cadence", &LocalCadence, 0, 0, "%.3f s", exposure_mode || SaveExposureFiles ? ImGuiInputTextFlags_EnterReturnsTrue : ImGuiInputTextFlags_ReadOnly))
    {
        if (LocalCadence < 0)
            LocalCadence = 0.040; // 25 Hz
        else if (LocalCadence > 600)
            LocalCadence = 600;
        Cadence = ((unsigned int)(LocalCadence * 1000 / 40)) * 40;
        CadenceChange = true;
    }
    ImGui::PopItemWidth();
    ImGui::NextColumn();
    if (ImGui::RadioButton("Single Shot", single_shot))
    {
        exposure_mode = false;
        single_shot = true;
        Cadence = 0; // 40 ms loop
        CadenceChange = true;
        EnableAutoExposure = false;
    }
    ImGui::NextColumn();

    // if (!single_shot)
    //     ImGui::PushStyleColor(0, ImVec4(0.75, 0.75, 0.75, 1));
    // else if (!TakeSingleShot)
    //     ImGui::PushStyleColor(0, ImVec4(0, 1, 0, 1));
    // else
    //     ImGui::PushStyleColor(0, ImVec4(0, 1, 1, 1));
    // if (!TakeSingleShot)
    // {
    //     if (ImGui::Button("Capture", ImVec2(100, 0)) && (!CapturingImage))
    //         TakeSingleShot = true;
    // }
    // else if ((single_shot && TakeSingleShot))
    //     ImGui::Button("In Progress", ImVec2(100, 0));
    if (exposure_mode) // continuous
    {
        if (CapturingImage)
        {
            ImGui::PushStyleColor(0, ImVec4(0, 1, 1, 1));
            ImGui::Button("In Progress", ImVec2(100, 0));
        }
        else
        {
            ImGui::PushStyleColor(0, ImVec4(0.75, 0.75, 0.75, 1));
            ImGui::Button("Capture", ImVec2(100, 0));
        }
    }
    else if (single_shot)
    {
        if (!TakeSingleShot)
        {
            ImGui::PushStyleColor(0, ImVec4(0, 1, 0, 1));
            if (ImGui::Button("Capture", ImVec2(100, 0)) && (!CapturingImage))
                TakeSingleShot = true;
        }
        else
        {
            ImGui::PushStyleColor(0, ImVec4(0, 1, 1, 1));
            ImGui::Button("In Progress", ImVec2(100, 0));
        }
    }
    ImGui::PopStyleColor();
    ImGui::Columns(1);
    ImGui::Separator();
    ImGui::Columns(2, "AutoExposureControl", true);
    if (ImGui::Checkbox("Enable Auto Exposure", &EnableAutoExposure))
    {
        // TODO: Sanity check
    }
    if (!EnableAutoExposure)
        FindExposureMedian = false;
    ImGui::NextColumn();
    ImGui::Checkbox("Measure Median Pixel", &FindExposureMedian);
    ImGui::NextColumn();
    static int PercentileTarget = FindExposureMode / 10;
    ImGui::PushItemWidth(50);
    if (ImGui::InputInt("Percentile Pixel", &PercentileTarget, 0, 0, EnableAutoExposure ? ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_EnterReturnsTrue : ImGuiInputTextFlags_ReadOnly))
    {
        if (PercentileTarget < 10)
            PercentileTarget = 10;
        if (PercentileTarget > 100)
            PercentileTarget = 100;
        FindExposureMode = PercentileTarget * 10;
    }
    ImGui::PopItemWidth();
    ImGui::NextColumn();
    ImGui::PushItemWidth(50);
    if (ImGui::InputFloat("Max Exposure", &MaxAllowedOptimumExposure, 0, 0, "%.1f s", EnableAutoExposure ? ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_EnterReturnsTrue : ImGuiInputTextFlags_ReadOnly))
    {
        if (MaxAllowedOptimumExposure < 0.1)
            MaxAllowedOptimumExposure = 0.1;
        if (MaxAllowedOptimumExposure > 200)
            MaxAllowedOptimumExposure = 200;
    }
    ImGui::PopItemWidth();
    ImGui::NextColumn();
    ImGui::PushItemWidth(50);
    if (ImGui::InputFloat("Pixel Target", &PixelTargetValue, 0, 0, "%.0f", EnableAutoExposure ? ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_EnterReturnsTrue : ImGuiInputTextFlags_ReadOnly))
    {
        if (PixelTargetValue < 100)
            PixelTargetValue = 100;
        if (PixelTargetValue > 0xffff)
            PixelTargetValue = 0xffff;
    }
    ImGui::PopItemWidth();
    ImGui::NextColumn();
    ImGui::PushItemWidth(50);
    if (ImGui::InputFloat("Uncertainty", &PixelTargetUncertainty, 0, 0, "%.0f", EnableAutoExposure ? ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_EnterReturnsTrue : ImGuiInputTextFlags_ReadOnly))
    {
        if (PixelTargetValue < 100)
            PixelTargetValue = 100;
        if (PixelTargetValue > 20000)
            PixelTargetValue = 20000;
    }
    ImGui::PopItemWidth();
    ImGui::Columns(1);
    // End Exposure Control

    // Begin ROI Setup
    ImGui::Separator();
    if (firstRun || RoiUpdated)
    {
        bin_roi[0] = cam->GetBinningX();
        bin_roi[1] = cam->GetBinningY();
        bin_roi[2] = (cam->GetROI())->x_min;
        bin_roi[3] = (cam->GetROI())->x_max;
        bin_roi[4] = (cam->GetROI())->y_min;
        bin_roi[5] = (cam->GetROI())->y_max;
        RoiUpdated = false;
    }
    if (ImGui::InputInt2("Binning", bin_roi, ImGuiInputTextFlags_EnterReturnsTrue))
        RoiUpdate = true;
    if (ImGui::InputInt2("X Bounds", &(bin_roi[2]), ImGuiInputTextFlags_EnterReturnsTrue))
        RoiUpdate = true;
    if (ImGui::InputInt2("Y Bounds", &(bin_roi[4]), ImGuiInputTextFlags_EnterReturnsTrue))
        RoiUpdate = true;
    // End ROI Setup
    ImGui::Separator();
    // Begin Exposure Save Setup
    ImGui::InputText("Prefix", SaveImagePrefix, IM_ARRAYSIZE(SaveImagePrefix), (!SaveExposureFiles) || (SaveImageCommand) ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_AutoSelectAll);
    ImGui::InputText("Directory", SaveImageDir, IM_ARRAYSIZE(SaveImageDir) / 2, (!SaveExposureFiles) || (SaveImageCommand) ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_AutoSelectAll);
    static int LoadingScreen = 0;
    if (SaveExposureFiles && SaveImageCommand)
    {
        static char loadAnim[] = "\\\\\\\\\\|||||/////-----";
        ImGui::Text("%d out of %d exposures completed, running %c", CurrentSaveImageNum, SaveImageNum, loadAnim[(LoadingScreen++) % strlen(loadAnim)]);
        ImGui::Text("SaveFits: %s", SaveFitsStatus);
        if (CurrentSaveImageNum >= SaveImageNum)
        {
            SaveImageCommand = false;
            CurrentSaveImageNum = 0;
            SaveFitsStatus[0] = '\0';
        }
    }
    else
    {
        ImGui::Text("Not storing exposures to disk.");
        ImGui::Text("");
    }
    ImGui::Columns(3, "FileSaveSystem", true);
    ImGui::Checkbox("Save Exposures", &SaveExposureFiles);
    if (SaveExposureFiles)
    {
        exposure_mode = false;
        single_shot = true;
        // Cadence = 0; // 40 ms loop
        // CadenceChange = true;
    }
    ImGui::NextColumn();
    ImGui::PushItemWidth(40);
    if (ImGui::InputInt("Exposures", &SaveImageNum, 0, 0, (!SaveExposureFiles) || (SaveImageCommand) ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
    {
        if (SaveImageNum < 0)
            SaveImageNum = 0;
        if (SaveImageNum > 100)
            SaveImageNum = 100;
    }
    ImGui::PopItemWidth();
    ImGui::NextColumn();
    ImVec4 ButtonColor;
    if (!SaveExposureFiles)
    {
        ImGui::PushStyleColor(0, ImVec4(0.5, 0.5, 0.5, 1));
        ImGui::Button("Start Exposure", ImVec2(100, 0));
    }
    else if (!SaveImageCommand)
    {
        ImGui::PushStyleColor(0, ImVec4(0, 0.8, 0, 1));
        if (ImGui::Button("Start Exposure", ImVec2(130, 0)))
        {
            if (LocalCadence < 0)
                LocalCadence = 0.040; // 25 Hz
            else if (LocalCadence > 600)
                LocalCadence = 600;
            Cadence = ((unsigned int)(LocalCadence * 1000 / 40)) * 40;
            CadenceChange = true;
            SaveImageCommand = true;
            SaveFitsStatus[0] = '\0';
        }
    }
    else
    {
        ImGui::PushStyleColor(0, ImVec4(1, 0, 0, 1));
        if (ImGui::Button("Stop Exposure", ImVec2(130, 0)))
        {
            SaveImageNum = 0;
            SaveFitsStatus[0] = '\0';
        }
    }
    ImGui::PopStyleColor();
    ImGui::Columns(1);
    // End Exposure Save Setup
    static bool show_img_window = true;
    if (show_img_window)
        ImageWindow(&show_img_window);

    ImGui::Checkbox("Image Display", &show_img_window);

    // EnterCriticalSection(img->lock); // acquire lock before loading texture
    // if (img->size)                   // image available
    // {
    //     LoadTextureFromMemFile(img->data, img->size, img_texture, img_width, img_height);
    // }
    // LeaveCriticalSection(img->lock); // release lock

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();
    firstRun = false;
}

void ImageWindow(bool *active)
{
    ImGui::Begin("Image Window", active);
    static int old_win_width = 0;
    int win_width = ImGui::GetWindowSize().x;
    int win_height = ImGui::GetWindowSize().y;
    if (win_width != old_win_width)
        imgwindow_resized = true;
    if (win_width < 512)
        win_width = 512;
    if (win_height < 768)
        win_height = 768;
    win_width = (win_width / 16) * 16;
    win_height = (win_height / 16) * 16;
    old_win_width = win_width; // update old width
    ImGui::SetWindowSize(ImVec2(win_width, win_height), ImGuiCond_Always);
    img_texture->width = win_width - 128;
    img_texture->height = 0;
    bool texture_valid = LoadTextureFromMemFile(img, img_texture);
    ImGui::Text("Image: %d x %d pixels", img->width, img->height);
    PDIRECT3DTEXTURE9 texture = img_texture->texture;
    int width = img_texture->width;
    int height = img_texture->height;
    ImVec2 posbefore = ImGui::GetCursorPos();
    ImVec2 newpos = posbefore;
    newpos.x += width;
    if (texture_valid && texture) // image can be shown
    {
        ImGui::Image((void *)texture, ImVec2(width, height)); // show image
    }
    ImGui::SetCursorPos(newpos);
    // printf("%f %f\n", posbefore.x, posbefore.y);;
    ImPlot::SetNextPlotLimitsX(main_image->data_min, main_image->data_max, ImGuiCond_Always);
    ImPlot::SetNextPlotLimitsY(0, main_image->height, ImGuiCond_Always);
    if (ImPlot::BeginPlot("##Y Hist", NULL, NULL, ImVec2(96, height), 0, ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_Invert | ImPlotAxisFlags_NoTickLabels)) //, 0, sideflags, sideflags))
    {
        ImPlot::PlotLine("Y Axis", main_image->ydata, main_image->ypoints, main_image->ydata_len, 0, sizeof(float));
        ImPlot::EndPlot();
    }
    posbefore.y += height;
    ImGui::SetCursorPos(posbefore);
    ImPlot::SetNextPlotLimitsX(0, main_image->width, ImGuiCond_Always);
    ImPlot::SetNextPlotLimitsY(main_image->data_min, main_image->data_max, ImGuiCond_Always);
    if (ImPlot::BeginPlot("##X Hist", NULL, NULL, ImVec2(width, 96))) //, 0, sideflags, sideflags))
    {
        ImPlot::PlotLine("X Axis", main_image->xpoints, main_image->xdata, main_image->xdata_len, 0, sizeof(float));
        ImPlot::EndPlot();
    }
    static bool ShowHistogram = false;
    ImGui::Separator();
    ImGui::Checkbox("Auto Contrast", &AutoContrastEn);
    ImGui::Columns(2, "ImageSystemColums", true);
    // Minima and Maxima selector
    ImGui::PushItemWidth(100);
    if (ImGui::InputInt("Pixel Min", &(jpeg_inout->min), 1, 100, AutoContrastEn ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_EnterReturnsTrue))
    {
        if (jpeg_inout->min > 0xffff)
            jpeg_inout->min = jpeg_inout->max - 100;
        if (jpeg_inout->min < 0)
            jpeg_inout->min = 0;
        jpeg_inout->adjust = true;
    }
    ImGui::PopItemWidth();
    ImGui::NextColumn();
    ImGui::PushItemWidth(100);
    if (ImGui::InputInt("Pixel Max", &(jpeg_inout->max), 1, 100, AutoContrastEn ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_EnterReturnsTrue))
    {
        if (jpeg_inout->max < 0)
            jpeg_inout->max = 0;
        if (jpeg_inout->max <= jpeg_inout->min)
            jpeg_inout->max = jpeg_inout->min + 100;
        if (jpeg_inout->max > 0xffff)
            jpeg_inout->max = 0xffff;
        jpeg_inout->adjust = true;
    }
    ImGui::PopItemWidth();
    // End Minima and Maxima Selector
    ImGui::NextColumn();
    ImGui::Checkbox("Show Histogram", &ShowHistogram);
    ImGui::NextColumn();
    if (ImGui::InputInt("JPEG Quality", &(JpegQuality), 1, 10, ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_EnterReturnsTrue))
    {
        if (JpegQuality < 30)
            JpegQuality = 30;
        if (JpegQuality > 100)
            JpegQuality = 100;
    }
    ImGui::Columns(1);
    ImGui::Separator();
    if (ShowHistogram)
    {
        ImGui::Begin("Histogram Window", &ShowHistogram);
        ImGui::Text("Min: %u, Max: %u, Average: %lf", main_image->data_min, main_image->data_max, main_image->data_average);
        ImPlot::SetNextPlotLimitsX(main_image->histdata_min, main_image->histdata_max, ImGuiCond_Always);
        ImPlot::SetNextPlotLimitsY(0, main_image->histdata_maxcount * 1.1, ImGuiCond_Always);
        if (ImPlot::BeginPlot("Pixel Histogram", "Bins", "Counts", ImVec2(-1, -1)))
        {
            ImPlot::PlotStairs("##Histogram", main_image->histdata, main_image->histdata_len, ((double)(main_image->histdata_max - main_image->histdata_min)) / main_image->histdata_len, (double) main_image->histdata_min, 0, sizeof(float));
            ImPlot::EndPlot();
        }
        ImGui::End();
    }
    ImGui::End();
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

d3d9_texture *active_textures[] = {img_texture};

void ResetDevice()
{
    for (int i = 0; i < (sizeof(active_textures) / sizeof(d3d9_texture *)); i++)
    {
        active_textures[i]->Reset();
    }
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
