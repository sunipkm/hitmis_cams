// CameraUnit.cpp : implementation file
//

#include "CameraUnit_ANDORUSB.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

bool CCameraUnit_ANDORUSB::HasError(unsigned int error, unsigned int line) const
{
    switch (error)
    {
    default:
        printf("%s, %d: ANDOR Error\n", __FILE__, line);
        fflush(stdout);
        return true;

// #define ANDOR_ERROR(x)                                               \
//     case x:                                                          \
//         if (error != lastError_)                                     \
//         {                                                            \
//             printf("%s, %d: ANDOR error: " #x "\n", __FILE__, line); \
//             fflush(stdout);                                          \
//         }                                                            \
//         lastError_ = error;                                          \
//         return true;
#define ANDOR_ERROR(x)                                           \
    case x:                                                      \
        printf("%s, %d: ANDOR error: " #x "\n", __FILE__, line); \
        fflush(stdout);                                          \
        return true;
#define NOT_ANDOR_ERROR(x) \
    case x:                \
        return false;

        ANDOR_ERROR(DRV_ERROR_CODES)
        NOT_ANDOR_ERROR(DRV_SUCCESS)
        ANDOR_ERROR(DRV_VXDNOTINSTALLED)
        ANDOR_ERROR(DRV_ERROR_SCAN)
        ANDOR_ERROR(DRV_ERROR_CHECK_SUM)
        ANDOR_ERROR(DRV_ERROR_FILELOAD)
        ANDOR_ERROR(DRV_UNKNOWN_FUNCTION)
        ANDOR_ERROR(DRV_ERROR_VXD_INIT)
        ANDOR_ERROR(DRV_ERROR_ADDRESS)
        ANDOR_ERROR(DRV_ERROR_PAGELOCK)
        ANDOR_ERROR(DRV_ERROR_PAGEUNLOCK)
        ANDOR_ERROR(DRV_ERROR_BOARDTEST)
        ANDOR_ERROR(DRV_ERROR_ACK)
        ANDOR_ERROR(DRV_ERROR_UP_FIFO)
        ANDOR_ERROR(DRV_ERROR_PATTERN)
        ANDOR_ERROR(DRV_ACQUISITION_ERRORS)
        ANDOR_ERROR(DRV_ACQ_BUFFER)
        ANDOR_ERROR(DRV_ACQ_DOWNFIFO_FULL)
        ANDOR_ERROR(DRV_PROC_UNKONWN_INSTRUCTION)
        ANDOR_ERROR(DRV_ILLEGAL_OP_CODE)
        ANDOR_ERROR(DRV_KINETIC_TIME_NOT_MET)
        ANDOR_ERROR(DRV_ACCUM_TIME_NOT_MET)
        ANDOR_ERROR(DRV_NO_NEW_DATA)
        ANDOR_ERROR(DRV_SPOOLERROR)
        ANDOR_ERROR(DRV_TEMP_CODES)
        ANDOR_ERROR(DRV_TEMP_OFF)
        ANDOR_ERROR(DRV_TEMP_NOT_STABILIZED)
        ANDOR_ERROR(DRV_TEMP_STABILIZED)
        ANDOR_ERROR(DRV_TEMP_NOT_REACHED)
        ANDOR_ERROR(DRV_TEMP_OUT_RANGE)
        ANDOR_ERROR(DRV_TEMP_NOT_SUPPORTED)
        ANDOR_ERROR(DRV_TEMP_DRIFT)
        ANDOR_ERROR(DRV_GENERAL_ERRORS)
        ANDOR_ERROR(DRV_INVALID_AUX)
        ANDOR_ERROR(DRV_COF_NOTLOADED)
        ANDOR_ERROR(DRV_FPGAPROG)
        ANDOR_ERROR(DRV_FLEXERROR)
        ANDOR_ERROR(DRV_GPIBERROR)
        ANDOR_ERROR(DRV_DATATYPE)
        ANDOR_ERROR(DRV_DRIVER_ERRORS)
        ANDOR_ERROR(DRV_P1INVALID)
        ANDOR_ERROR(DRV_P2INVALID)
        ANDOR_ERROR(DRV_P3INVALID)
        ANDOR_ERROR(DRV_P4INVALID)
        ANDOR_ERROR(DRV_INIERROR)
        ANDOR_ERROR(DRV_COFERROR)
        ANDOR_ERROR(DRV_ACQUIRING)
        ANDOR_ERROR(DRV_IDLE)
        ANDOR_ERROR(DRV_TEMPCYCLE)
        ANDOR_ERROR(DRV_NOT_INITIALIZED)
        ANDOR_ERROR(DRV_P5INVALID)
        ANDOR_ERROR(DRV_P6INVALID)
        ANDOR_ERROR(DRV_INVALID_MODE)
        ANDOR_ERROR(DRV_INVALID_FILTER)
        ANDOR_ERROR(DRV_I2CERRORS)
        ANDOR_ERROR(DRV_I2CDEVNOTFOUND)
        ANDOR_ERROR(DRV_I2CTIMEOUT)
        ANDOR_ERROR(DRV_P7INVALID)
        ANDOR_ERROR(DRV_IOCERROR)
        ANDOR_ERROR(DRV_VRMVERSIONERROR)
        ANDOR_ERROR(DRV_ERROR_NOCAMERA)
        ANDOR_ERROR(DRV_NOT_SUPPORTED)
#undef ANDOR_ERROR
#undef NOT_ANDOR_ERROR
#undef SILENT_ANDOR_ERROR
    }
}

CCameraUnit_ANDORUSB::CCameraUnit_ANDORUSB(int shutterDelayInMs, unsigned int readOutSpeed)
    : binningX_(1), binningY_(1), cancelCapture_(true), CCDHeight_(0), CCDWidth_(0), exposure_(0), lastError_(DRV_SUCCESS), m_initializationOK(false), requestShutterOpen_(true), shutterDelayInMs_(shutterDelayInMs)
{
    char aBuffer[256];

    GetCurrentDirectory(256, (LPWSTR)aBuffer); // Look in current working directory
                                               // for driver files

    // Initialize driver in current directory
    if (HasError(Initialize(aBuffer), __LINE__))
    {
        return;
    }

    HasError(GetHeadModel(_camera_name), __LINE__);

    if (HasError(SetAcquisitionMode(1), __LINE__))
    {
        return;
    }

    if (HasError(SetReadMode(4), __LINE__))
    {
        return;
    }

    if (HasError(GetDetector(&CCDWidth_, &CCDHeight_), __LINE__))
    {
        return;
    }

    if (HasError(CoolerON(), __LINE__))
    {
        return;
    }

    //SetShutterIsOpen(true);

    // CODE ADDED BY BOB FOR INITIALIZATION ---------------

    if (HasError(SetTriggerMode(0), __LINE__))
    {
        return;
    }

    // Set Vertical shift "time" to max
    float speed, STemp = 0;
    int iSpeed, nAD, index;
    int VSnumber; // Vertical Speed Index
    int HSnumber; // Horizontal Speed Index
    int ADnumber;
    int nHSspeeds;

    if (HasError(GetNumberVSSpeeds(&index), __LINE__))
    {
        return;
    }
    for (iSpeed = 0; iSpeed < index; iSpeed++)
    {
        GetVSSpeed(iSpeed, &speed);
        if (speed > STemp)
        {
            STemp = speed;
            VSnumber = iSpeed;
        }
    }
    if (HasError(SetVSSpeed(VSnumber), __LINE__))
    {
        return;
    }

    // Set Horizontal Speed and AD channel number

    STemp = 0;
    HSnumber = 0;
    ADnumber = 0;
    if (HasError(GetNumberADChannels(&nAD), __LINE__))
    {
        return;
    }
    if (nAD == 1)
    {

        ADnumber = 0;
        if (HasError(SetADChannel(ADnumber), __LINE__))
        {
            return;
        }

        // April 2011: for ANDOR, changed to use the middle (1 MHz) readout speed. Index 0 is the fastest!
        // I will set it to the 2nd slowest speed. If nHSspeeds is 5, we want index 3.

        if (HasError(GetNumberHSSpeeds(ADnumber, 0, &nHSspeeds), __LINE__))
        {
            return;
        }
        if (HasError(SetHSSpeed(0, nHSspeeds - 2), __LINE__))
        {
            return;
        }
    }

    // Set Preamp gain to max

    STemp = 0;
    int AmpNumber = 0;
    if (HasError(GetNumberPreAmpGains(&index), __LINE__))
    {
        return;
    }
    for (iSpeed = 0; iSpeed < index; iSpeed++)
    {
        if (HasError(GetPreAmpGain(iSpeed, &speed), __LINE__))
        {
            return;
        }
        if (speed > STemp)
        {
            STemp = speed;
            AmpNumber = iSpeed;
        }
    }
    if (HasError(SetPreAmpGain(AmpNumber), __LINE__))
    {
        return;
    }

    // --------------------------------------

    m_initializationOK = true;
}

CCameraUnit_ANDORUSB::~CCameraUnit_ANDORUSB()
{
    CriticalSection::Lock lock(criticalSection_);

    CoolerOFF(); // Switch off cooler (if used)
    ShutDown();
}

void CCameraUnit_ANDORUSB::CancelCapture()
{
    CriticalSection::Lock lock(criticalSection_);
    cancelCapture_ = true;
    AbortAcquisition();
}

void CCameraUnit_ANDORUSB::SetReadout(int ReadSpeed)
{
    if (!m_initializationOK)
    {
        return;
    }
}

CImageData CCameraUnit_ANDORUSB::CaptureImage(long int &retryCount)
{
    CriticalSection::Lock lock(criticalSection_);
    CImageData retVal;
    cancelCapture_ = false;
    static bool firstCapture = true;

    if (!m_initializationOK)
    {
        goto exit_;
    }

    // Without this Sleep the first capture is all black.
    // It only seems to happen in release code.
    // On my system I could not go much below 500 ms.
    // No idea why -- Jaap (5/26/2004)
    if (firstCapture)
    {
        Sleep(200);
        firstCapture = false;
    }

    { // wait for idle before starting acquisistion

        bool done = false;
        while (!done)
        {
            // give other threads access
            lock.Unlock();
            Sleep(100);
            lock.Relock();

            if (cancelCapture_)
            {
                goto exit_;
            }

            int status = DRV_IDLE;
            if (HasError(::GetStatus(&status), __LINE__))
            {
                goto exit_;
            }
            switch (status)
            {
            default:
                goto exit_; // unknown status is treated as error
            case DRV_IDLE:
                done = true;
                break;
            case DRV_ACQUIRING:
                SetStatus("Waiting on previous acquisition");
                break;
            case DRV_TEMPCYCLE:
                SetStatus("Waiting on temperature");
                break;
            }
        }
    }

    SetShutter();

    if (HasError(StartAcquisition(), __LINE__))
    {
        goto exit_;
    }

    if (cancelCapture_)
    {
        goto exit_;
    }

    { // allocate the buffer
        printf("Size = %d x %d\n", width_, height_);
        printf("CCD Size = %d x %d\n", CCDWidth_, CCDHeight_);
        CImageData buffer(width_ / binningX_, height_ / binningY_);

        bool done = false;
        while (!done)
        {
            // give other threads access
            lock.Unlock();
            Sleep(100);
            lock.Relock();

            if (cancelCapture_)
            {
                goto exit_;
            }

            unsigned int status = GetAcquiredData16(buffer.GetImageData(), width_ / binningX_ * height_ / binningY_);
            switch (status)
            {
            default:
                HasError(status, __LINE__);
                goto exit_; // unknown status is treated as error

            case DRV_SUCCESS:
                done = true;
                break;
            case DRV_ACQUIRING:
                SetStatus("Acquiring ");
                break;
            case DRV_TEMPCYCLE:
                SetStatus("Waiting on temperature");
                break;
            }
        }

        buffer.FlipHorizontal();
        retVal = buffer;
    }

exit_:
    return retVal;
}

void CCameraUnit_ANDORUSB::SetTemperature(double temperatureInCelcius)
{
    if (!m_initializationOK)
    {
        return;
    }

    int minTemp, maxTemp;

    if (HasError(GetTemperatureRange(&minTemp, &maxTemp), __LINE__))
    {
        return;
    }

    if (temperatureInCelcius < minTemp)
    {
        temperatureInCelcius = minTemp;
    }
    if (temperatureInCelcius > maxTemp)
    {
        temperatureInCelcius = maxTemp;
    }

    if (HasError(::SetTemperature(temperatureInCelcius), __LINE__))
    {
        return;
    }
}

double CCameraUnit_ANDORUSB::GetTemperature() const
{
    if (!m_initializationOK)
    {
        return INVALID_TEMPERATURE;
    }

    CriticalSection::Lock lock(criticalSection_);

    int temperature = 0;
    unsigned int retVal = ::GetTemperature(&temperature);

    switch (retVal)
    {
    default:
        HasError(retVal, __LINE__);
        return INVALID_TEMPERATURE;

    case DRV_ACQUIRING:
        return INVALID_TEMPERATURE;

    case DRV_SUCCESS:
    case DRV_TEMP_NOT_STABILIZED:
    case DRV_TEMP_STABILIZED:
    case DRV_TEMP_NOT_REACHED:
        return temperature;
    }
}

void CCameraUnit_ANDORUSB::SetBinningAndROI(int binX, int binY, int x_min, int x_max, int y_min, int y_max)
{
    if (!m_initializationOK)
    {
        return;
    }

    CriticalSection::Lock lock(criticalSection_);
    if (!m_initializationOK)
    {
        return;
    }
    lock.Unlock();

    if (binX < 1)
        binX = 1;
    if (binX > 64)
        binX = 64;

    if (binY < 1)
        binY = 1;
    if (binY > 64)
        binY = 64;

    // now store the changes

    binningX_ = binX;
    binningY_ = binY;

    if (x_min < 0)
        x_min = 0;
    if ((x_max <= x_min) || (x_max > CCDWidth_))
        x_max = CCDWidth_;

    if (y_min <= 0)
        y_min = 0;
    if ((y_max <= y_min) || (y_max > CCDHeight_))
        y_max = CCDHeight_;

    lock.Relock();
    width_ = x_max - x_min;
    // make sure width_ is consistent with binning
    width_ = (width_ / binningX_) * binningX_;
    height_ = y_max - y_min;
    // make sure height is consistent with binning
    height_ = (height_ / binningY_) * binningY_;
    xmin_ = x_min;
    xmax_ = x_min + width_;
    ymin_ = y_min;
    ymax_ = y_min + height_;

    unsigned int retVal = SetImage(binningX_,
                                   binningY_,
                                   x_min + 1,
                                   x_max,
                                   y_min + 1,
                                   y_max);

    switch (retVal)
    {
    default:
        HasError(retVal, __LINE__);
        return;

    case DRV_SUCCESS:
        break;

    case DRV_P1INVALID:
        printf("HFlip invalid\n");
        break;
    case DRV_P2INVALID:
        printf("VFlip invalid\n");
        return;
    };
}

const ROI *CCameraUnit_ANDORUSB::GetROI() const
{
    static ROI roi;
    roi.x_min = xmin_;
    roi.x_max = xmax_;
    roi.y_min = ymin_;
    roi.y_max = ymax_;
    return &roi;
}

void CCameraUnit_ANDORUSB::SetShutter()
{
    if (!m_initializationOK)
    {
        return;
    }

    const int TTL_HIGH = 1;
    const int TTL_LOW = 0;
    const int SHUTTER_AUTO = 0;
    const int SHUTTER_CLOSE = 2;

    if (requestShutterOpen_ && (exposure_ > 0.0))
    {
        HasError(::SetShutter(TTL_HIGH, SHUTTER_AUTO, shutterDelayInMs_, shutterDelayInMs_), __LINE__);
    }
    else
    {
        HasError(::SetShutter(TTL_HIGH, SHUTTER_CLOSE, 0, 0), __LINE__);
    }
}

void CCameraUnit_ANDORUSB::SetShutterIsOpen(bool open)
{
    requestShutterOpen_ = open;
}

void CCameraUnit_ANDORUSB::SetExposure(float exposureInSeconds)
{
    // printf("%s called\n", __func__);
    if (!m_initializationOK)
    {
        return;
    }

    // printf("%s: Received exposure request: %f\n", __func__, exposureInSeconds);

    if (exposureInSeconds <= 0)
    {
        exposureInSeconds = 0.0;
    }

    long int maxexposurems = exposureInSeconds * 1000;

    if (maxexposurems > 10 * 60 * 1000) // max exposure 10 minutes
        maxexposurems = 10 * 60 * 1000;

    exposureInSeconds = maxexposurems * 0.001; // 1 ms increments only

    if (exposure_ == exposureInSeconds)
    {
        // printf("%s: Exposure already set to %f\n", __func__, exposure_);
        return;
    }

    // printf("%s: Exposure to be set: %f\n", __func__, exposureInSeconds);

    CriticalSection::Lock lock(criticalSection_);

    // printf("%s: Calling set exposure\n", __func__);

    if (HasError(SetExposureTime(exposureInSeconds), __LINE__))
    {
        // printf("%s: Set exposure has error\n", __func__);
        // fflush(stdout);
        return;
    }
    exposure_ = exposureInSeconds;
    // printf("%s: Set exposure successful: %f\n", __func__, exposure_);
    // fflush(stdout);
}

float CCameraUnit_ANDORUSB::GetExposure() const
{
    if (!m_initializationOK)
    {
        return 0.0;
    }

    CriticalSection::Lock lock(criticalSection_);

    float exposure;
    float accumulate;
    float kinetic;
    if (HasError(GetAcquisitionTimings(&exposure, &accumulate, &kinetic), __LINE__))
    {
        return 0.0;
    }
    return exposure;
}
