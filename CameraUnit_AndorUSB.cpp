// CameraUnit.cpp : implementation file
//

#include "CameraUnit_ANDORUSB.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

bool CCameraUnit_ANDORUSB::HasError(unsigned int error) const
{
    switch (error)
    {
    default:
        printf("ANDOR Error\n");
        return true;

#define ANDOR_ERROR(x)                  \
    case x:                             \
        if (error != lastError_)        \
            printf("ANDOR error: " #x); \
        lastError_ = error;             \
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
    if (HasError(Initialize(aBuffer)))
    {
        return;
    }

    if (HasError(SetAcquisitionMode(1)))
    {
        return;
    }

    if (HasError(SetReadMode(4)))
    {
        return;
    }

    if (HasError(GetDetector(&CCDWidth_, &CCDHeight_)))
    {
        return;
    }

    if (HasError(CoolerON()))
    {
        return;
    }

    //SetShutterIsOpen(true);

    // CODE ADDED BY BOB FOR INITIALIZATION ---------------

    if (HasError(SetTriggerMode(0)))
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

    if (HasError(GetNumberVSSpeeds(&index)))
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
    if (HasError(SetVSSpeed(VSnumber)))
    {
        return;
    }

    // Set Horizontal Speed and AD channel number

    STemp = 0;
    HSnumber = 0;
    ADnumber = 0;
    if (HasError(GetNumberADChannels(&nAD)))
    {
        return;
    }
    if (nAD == 1)
    {

        ADnumber = 0;
        if (HasError(SetADChannel(ADnumber)))
        {
            return;
        }

        // April 2011: for ANDOR, changed to use the middle (1 MHz) readout speed. Index 0 is the fastest!
        // I will set it to the 2nd slowest speed. If nHSspeeds is 5, we want index 3.

        if (HasError(GetNumberHSSpeeds(ADnumber, 0, &nHSspeeds)))
        {
            return;
        }
        if (HasError(SetHSSpeed(0, nHSspeeds - 2)))
        {
            return;
        }
    }

    // Set Preamp gain to max

    STemp = 0;
    int AmpNumber = 0;
    if (HasError(GetNumberPreAmpGains(&index)))
    {
        return;
    }
    for (iSpeed = 0; iSpeed < index; iSpeed++)
    {
        if (HasError(GetPreAmpGain(iSpeed, &speed)))
        {
            return;
        }
        if (speed > STemp)
        {
            STemp = speed;
            AmpNumber = iSpeed;
        }
    }
    if (HasError(SetPreAmpGain(AmpNumber)))
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

    if (!m_initializationOK)
    {
        goto exit_;
    }

    // Without this Sleep the first capture is all black.
    // It only seems to happen in release code.
    // On my system I could not go much below 500 ms.
    // No idea why -- Jaap (5/26/2004)
    Sleep(200);

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
            if (HasError(::GetStatus(&status)))
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

    if (HasError(StartAcquisition()))
    {
        goto exit_;
    }

    if (cancelCapture_)
    {
        goto exit_;
    }

    { // allocate the buffer

        CImageData buffer(width_, height_);

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

            unsigned int status = GetAcquiredData16(buffer.GetImageData(), width_ * height_);
            switch (status)
            {
            default:
                HasError(status);
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
    SetStatus("");
    return retVal;
}

void CCameraUnit_ANDORUSB::SetTemperature(double temperatureInCelcius)
{
    if (!m_initializationOK)
    {
        return;
    }

    int minTemp, maxTemp;

    if (HasError(GetTemperatureRange(&minTemp, &maxTemp)))
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

    if (HasError(::SetTemperature(temperatureInCelcius)))
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

    int temperature = 0;
    unsigned int retVal = ::GetTemperature(&temperature);

    switch (retVal)
    {
    default:
        HasError(retVal);
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

    width_ = x_max;
    if (width_ > CCDWidth_)
        width_ = CCDWidth_;
    else if (width_ <= 0)
        width_ = CCDWidth_;
    height_ = y_max;
    if (height_ > CCDHeight_)
        height_ = CCDHeight_;
    else if (height_ <= 0)
        height_ = CCDHeight_;
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
        HasError(::SetShutter(TTL_HIGH, SHUTTER_AUTO, shutterDelayInMs_, shutterDelayInMs_));
    }
    else
    {
        HasError(::SetShutter(TTL_HIGH, SHUTTER_CLOSE, 0, 0));
    }
}

void CCameraUnit_ANDORUSB::SetShutterIsOpen(bool open)
{
    requestShutterOpen_ = open;
}

void CCameraUnit_ANDORUSB::SetExposure(float exposureInSeconds)
{
    if (!m_initializationOK)
    {
        return;
    }

    if (exposureInSeconds <= 0)
    {
        exposureInSeconds = 0.0;
    }

    if (HasError(SetExposureTime(exposureInSeconds)))
    {
        return;
    }

    exposure_ = exposureInSeconds;
}

float CCameraUnit_ANDORUSB::GetExposure() const
{
    if (!m_initializationOK)
    {
        return 0.0;
    }

    float exposure;
    float accumulate;
    float kinetic;
    if (HasError(GetAcquisitionTimings(&exposure, &accumulate, &kinetic)))
    {
        return 0.0;
    }
    return exposure;
}

const ROI &CCameraUnit_ANDORUSB::GetROI()
{
    static ROI roi;
    roi.x_min = 0;
    roi.x_max = width_;
    roi.y_min = 0;
    roi.y_max = height_;
    return roi;
}