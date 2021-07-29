// CameraUnit.cpp : implementation file
//
#include "CameraUnit_PI.h"
#include "picam.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CCameraUnit_PI::CCameraUnit_PI()
: binningX_(1)
, binningY_(1)
, cancelCapture_(true)
, CCDHeight_(0)
, CCDWidth_(0)
, imageLeft_(0)
, imageRight_(0)
, imageTop_(0)
, imageBottom_(0)
, exposure_(0)
, lastError_(0)
, m_initializationOK(false)
, requestShutterOpen_(true)
{
	// do initialization stuff

	short numcameras;
	char cam_name[100];

	// initialize camera

   int err;
	if ((err = Picam_InitializeLibrary()))
   {
      printf("Error initializing picam library %d\n", err);
      return;
   }
   const PicamCameraID *cams;
   piint count;
   Picam_GetAvailableCameraIDs(&cams, &count);
   if (count > 0)
   {
      printf("picam %d cameras found\n", count);
   }
   else
   {
      printf("Cameras not found\n");
      return;
   }


	// Open the camera
   // PicamHandle *cam;
   err = Picam_OpenFirstCamera(&hCam);
	if ((err))
   {
      printf("error opening camera %d\n", err);
      Picam_UninitializeLibrary();
      return;
   }
   // hCam = *cam;
   printf("opened camera successfully\n");

	// readout speed moved to its function below

	// gain settings
	piint maxgain;
   Picam_GetParameterIntegerDefaultValue(hCam, PicamParameter_EMIccdGain, &maxgain);
   Picam_SetParameterIntegerValue(hCam, PicamParameter_EMIccdGain, maxgain);
	Picam_SetParameterIntegerValue(hCam, PicamParameter_EMIccdGainControlMode, PicamEMIccdGainControlMode_Optimal); // optimal gain

	// // get chip size

	piint xdimension = 0;
	piint ydimension = 0;

	Picam_GetParameterIntegerValue(hCam, PicamParameter_SensorActiveHeight, &ydimension);
	Picam_GetParameterIntegerValue(hCam, PicamParameter_SensorActiveWidth, &xdimension);

	CCDHeight_ = int(ydimension);
	CCDWidth_ = int(xdimension);
   printf("Chip size: %d x %d\n", CCDWidth_, CCDHeight_);

	// --------------------------------------

   m_initializationOK = true;
}


CCameraUnit_PI::~CCameraUnit_PI()
{
   CriticalSection::Lock lock(criticalSection_);
   
   Picam_CloseCamera(hCam);

	Picam_UninitializeLibrary();
}


void CCameraUnit_PI::CancelCapture()
{
   CriticalSection::Lock lock(criticalSection_);
   cancelCapture_ = true;

   // abort acquisition, close shutter, halt CCS, and put camera in idle
   Picam_StopAcquisition(hCam);
}


// ----------------------------------------------------------

CImageData CCameraUnit_PI::CaptureImage(long int& retryCount)
{
   CriticalSection::Lock lock(criticalSection_);
   CImageData retVal;
   cancelCapture_ = false;

// 	uns32 datasize;
// 	uns16* pImgBuf;
// 	int16 status;
// 	uns32 not_needed;

//    if (!m_initializationOK)
//    {
//       goto exit_;
//    }

//    {
// 	    rgn_type region = { imageLeft_, imageRight_, binningX_, imageBottom_, imageTop_, binningY_ }; 

// 		// add acquisition code here

// 		pl_exp_init_seq();
// 		pl_exp_setup_seq(hCam, 1, 1, &region, TIMED_MODE, uns32(exposure_ * 1000), &datasize);
	
// 		// allocate memory

// 		pImgBuf = (uns16*)malloc(datasize);

// 		// take an image


// 		pl_exp_start_seq(hCam, pImgBuf);


// 		pl_exp_check_status(hCam,&status,&not_needed);

// 		while ( status != READOUT_COMPLETE ) {
// 			Sleep(500);
// 			pl_exp_check_status(hCam,&status,&not_needed);
// 		}


// 		pl_exp_finish_seq(hCam, pImgBuf, NULL);
// 		pl_exp_uninit_seq();

//       int imageWidth = imageRight_ - imageLeft_;
//       int imageHeight = imageTop_ - imageBottom_;

// 		retVal = CImageData(imageWidth, imageHeight);

// 		memcpy(retVal.GetImageData(), pImgBuf, datasize);
// 	}

   
// exit_:
//    SetStatus("");
//    free(pImgBuf);			// without this line of code, memory usage grows continuously!
   return retVal;
}


void CCameraUnit_PI::SetTemperature(double temperatureInCelcius)
{
   if (!m_initializationOK)
   {
      return;
   }

   piint settemp;
   settemp = temperatureInCelcius * 100;
  
   // pl_set_param(hCam, PARAM_TEMP_SETPOINT, (void *)&settemp);


}


// get temperature is done
double CCameraUnit_PI::GetTemperature() const
{
   if (!m_initializationOK)
   {
      return INVALID_TEMPERATURE;
   }

   piint temperature = 0;
	// pl_get_param(hCam, PARAM_TEMP, ATTR_CURRENT, (void *)&temperature);

   double retVal;

   retVal = double(temperature)/100;
   return retVal;
}

//
void CCameraUnit_PI::SetBinningAndROI(int binX, int binY, int x_min, int x_max, int y_min, int y_max)
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

   if (binX < 1 ) binX = 1;
   if (binX > 16) binX = 16;

   binningX_ = binX;

   if (binY < 1 ) binY = 1;
   if (binY > 16) binY = 16;

   binningY_ = binY;

   imageLeft_ = x_min;
   imageRight_ = x_max;
   imageBottom_ = y_min;
   imageTop_ = y_max;

   if (imageRight_ > GetCCDWidth())
      imageRight_ = GetCCDWidth();
   if (imageLeft_ < 0)
      imageLeft_ = 0;
   if (imageRight_ <= imageLeft_)
      imageRight_ = GetCCDWidth();

   if (imageTop_ > GetCCDHeight())
      imageTop_ = GetCCDHeight();
   if (imageBottom_ < 0)
      imageBottom_ = 0;
   if (imageTop_ <= imageBottom_)
      imageTop_ = GetCCDHeight();

}



void CCameraUnit_PI::SetShutter()
{
   if (!m_initializationOK)
   {
      return;
   }

   // do stuff to shutter?

}

void CCameraUnit_PI::SetShutterIsOpen(bool open)
{
   requestShutterOpen_ = true;
}


// exposure time done
void CCameraUnit_PI::SetExposure(float exposureInSeconds)
{
   if (!m_initializationOK)
   {
      return;
   }

   if ( exposureInSeconds <= 0 )
   {
      exposureInSeconds = 0.0;
   }

   long int maxexposurems = exposureInSeconds * 1000;

   if (maxexposurems > 10 * 60 * 1000) // max exposure 10 minutes
      maxexposurems = 10 * 60 * 1000;

   exposure_ = maxexposurems * 0.001; // 1 ms increments only
}

void CCameraUnit_PI::SetReadout(int ReadSpeed)
{
	if (!m_initializationOK)
    {
       return;
    }

	// readout speeds! I tested and there is only one readout port.
	// I am reading in a parameter from the PRF file which gives readspeed_, which for
	// PI should be either 0 (low) or 1 (high)

	// int16 minspeed = 0;
	// int16 numspeeds;
	// int16 maxspeed;
	// pl_get_param(hCam, PARAM_SPDTAB_INDEX, ATTR_COUNT, (void *)&numspeeds);
	// pl_get_param(hCam, PARAM_SPDTAB_INDEX, ATTR_MAX, (void *)&maxspeed);

	// if (ReadSpeed == 0) { 
	// 	pl_set_param(hCam, PARAM_SPDTAB_INDEX, (void *)&minspeed);
	// } else {
	// 	pl_set_param(hCam, PARAM_SPDTAB_INDEX, (void *)&maxspeed);
	// }

}

const ROI& CCameraUnit_PI::GetROI() const
{
   static ROI roi;
   roi.x_min = imageLeft_;
   roi.x_max = imageRight_;
   roi.y_min = imageBottom_;
   roi.y_max = imageTop_;
   return roi;
}