// CameraUnit.cpp : implementation file
//
#include "CameraUnit_PI.h"
#include "pvcam.h"


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
, roiLeft(0)
, roiRight(0)
, roiBottom(0)
, roiTop(0)
, exposure_(0)
, lastError_(0)
, m_initializationOK(false)
, requestShutterOpen_(true)
{
	// do initialization stuff
	short numcameras = 0;

	// initialize camera

	pl_pvcam_init();

	// get number of cameras and names

    pl_cam_get_total(&numcameras);
    if (numcameras == 0)
    {
       pl_pvcam_uninit();
       return;
    }
	for (int i = 0; i<numcameras; i++) {
		pl_cam_get_name(i,cam_name);
	}
   // printf("Cameras found: %d\n", numcameras);
	// Open the camera
	if (pl_cam_open(cam_name, &hCam, OPEN_EXCLUSIVE) == FALSE)
   {
      pl_pvcam_uninit();
      return;
   }
   // printf("Camera opened\n");

	// readout speed moved to its function below

	// gain settings
	int16 maxgain;
	pl_get_param(hCam, PARAM_GAIN_INDEX, ATTR_MAX, (void *)&maxgain);
	pl_set_param(hCam, PARAM_GAIN_INDEX, (void *)&maxgain);
   // printf("Max gain: %d\n", maxgain);
	// get chip size

	unsigned short xdimension = 0;
	unsigned short ydimension = 0;

	pl_ccd_get_par_size(hCam, &ydimension);
	pl_ccd_get_ser_size(hCam, &xdimension);

	CCDHeight_ = int(ydimension);
	CCDWidth_ = int(xdimension);
   // printf("CCD: %d x %d pixels\n", CCDHeight_, CCDWidth_);

	// --------------------------------------

   m_initializationOK = true;
}


CCameraUnit_PI::~CCameraUnit_PI()
{
   CriticalSection::Lock lock(criticalSection_);
   
    pl_cam_close(hCam);

	pl_pvcam_uninit();
}


void CCameraUnit_PI::CancelCapture()
{
   CriticalSection::Lock lock(criticalSection_);
   cancelCapture_ = true;

   // abort acquisition, close shutter, halt CCS, and put camera in idle
   pl_exp_abort(hCam,CCS_HALT_CLOSE_SHTR);
}


// ----------------------------------------------------------

CImageData CCameraUnit_PI::CaptureImage(long int& retryCount)
{
   // printf("Starting capture image\n");
   CriticalSection::Lock lock(criticalSection_);
   CImageData retVal;
   cancelCapture_ = false;

	uns32 datasize;
	uns16* pImgBuf;
	int16 status;
	uns32 not_needed;

   if (!m_initializationOK)
   {
      goto exit_err;
   }

   {
	    rgn_type region = { imageLeft_, imageRight_, binningX_, imageBottom_, imageTop_, binningY_ }; 

		// add acquisition code here
      // printf("Setting up exposure: %d, %d | %d %d | %d %d | %u ms\n", binningX_, binningY_, imageLeft_, imageRight_, imageBottom_, imageTop_, uns32(exposure_ * 1000));
      
		pl_exp_init_seq();
		pl_exp_setup_seq(hCam, 1, 1, &region, TIMED_MODE, uns32(exposure_ * 1000), &datasize);

      // // printf("Set up exposure, data size %u\n", datasize);
      if (!datasize)
         goto exit_err;
		// allocate memory

		pImgBuf = (uns16*)malloc(datasize);

		// take an image


		pl_exp_start_seq(hCam, pImgBuf);
      // // printf("Started exposure\n");


		pl_exp_check_status(hCam,&status,&not_needed);
      int counter = 0;
      // // printf("Status %d: %d\n", counter++, status);
		while ( status != READOUT_COMPLETE ) {
			Sleep(500);
			pl_exp_check_status(hCam,&status,&not_needed);
         // // printf("Status %d: %d\n", counter++, status);
         if (status == READOUT_FAILED)
            goto exit_;
		}


		pl_exp_finish_seq(hCam, pImgBuf, NULL);
		pl_exp_uninit_seq();
      // // printf("Finished exposure\n");

      int imageWidth = (imageRight_ - imageLeft_ + 1) / binningX_;
      int imageHeight = (imageTop_ - imageBottom_ + 1) / binningY_;

		retVal = CImageData(imageWidth, imageHeight);
      // printf("Got CImageData: ");
      // printf("%p %d %d\n", retVal.GetImageData(), retVal.GetImageWidth(), retVal.GetImageHeight());
		memcpy(retVal.GetImageData(), pImgBuf, datasize);
      free(pImgBuf);			// without this line of code, memory usage grows continuously!
      // printf("Finished memcpy\n");
	}  
exit_:
exit_err:
   // printf("Exiting capture\n");
   return retVal;
}


void CCameraUnit_PI::SetTemperature(double temperatureInCelcius)
{
   if (!m_initializationOK)
   {
      return;
   }

   int16 settemp;
   settemp = temperatureInCelcius * 100;
  
   pl_set_param(hCam, PARAM_TEMP_SETPOINT, (void *)&settemp);


}


// get temperature is done
double CCameraUnit_PI::GetTemperature() const
{
   if (!m_initializationOK)
   {
      return INVALID_TEMPERATURE;
   }

   int16 temperature = 0;
	pl_get_param(hCam, PARAM_TEMP, ATTR_CURRENT, (void *)&temperature);

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
   imageRight_ = x_max - 1;
   imageBottom_ = y_min;
   imageTop_ = y_max - 1;

   if (imageRight_ > GetCCDWidth() - 1)
      imageRight_ = GetCCDWidth() - 1;
   if (imageLeft_ < 0)
      imageLeft_ = 0;
   if (imageRight_ <= imageLeft_)
      imageRight_ = GetCCDWidth() - 1;

   if (imageTop_ > GetCCDHeight() - 1)
      imageTop_ = GetCCDHeight() - 1;
   if (imageBottom_ < 0)
      imageBottom_ = 0;
   if (imageTop_ <= imageBottom_)
      imageTop_ = GetCCDHeight() - 1;
   
   roiLeft = imageLeft_;
   roiRight = imageRight_ + 1;
   roiBottom = imageBottom_;
   roiTop = imageTop_ + 1;
   // printf("%d %d, %d %d | %d %d\n", binningX_, binningY_, imageLeft_, imageRight_, imageBottom_, imageTop_);
}

const ROI *CCameraUnit_PI::GetROI() const
{
   static ROI roi;
   roi.x_min = roiLeft;
   roi.x_max = roiRight;
   roi.y_min = roiBottom;
   roi.y_max = roiTop;
   return &roi;
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

	int16 minspeed = 0;
	int16 numspeeds;
	int16 maxspeed;
	pl_get_param(hCam, PARAM_SPDTAB_INDEX, ATTR_COUNT, (void *)&numspeeds);
	pl_get_param(hCam, PARAM_SPDTAB_INDEX, ATTR_MAX, (void *)&maxspeed);

	if (ReadSpeed == 0) { 
		pl_set_param(hCam, PARAM_SPDTAB_INDEX, (void *)&minspeed);
	} else {
		pl_set_param(hCam, PARAM_SPDTAB_INDEX, (void *)&maxspeed);
	}

}