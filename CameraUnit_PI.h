#ifndef __CAMERAUNIT_PI_H__
#define __CAMERAUNIT_PI_H__


#include "CameraUnit.h"

#include "CriticalSection.h"
#include "pvcam.h"

/////////////////////////////////////////////////////////////////////////////
// CCameraUnit object

class CCameraUnit_PI: public CCameraUnit
{
   short hCam;

   bool m_initializationOK;
   CriticalSection criticalSection_;
   bool cancelCapture_;
   std::string status_;
   CriticalSection statusCriticalSection_;

   float exposure_;
   bool exposure_updated_;

   bool requestShutterOpen_;
   bool shutter_updated_;

   int binningX_;
   int binningY_;

   int imageLeft_;
   int imageRight_;
   int imageTop_;
   int imageBottom_;
   bool roi_updated_;

   int CCDWidth_;
   int CCDHeight_;
   int CCDgain_;
   bool gain_updated_;

   mutable volatile unsigned int lastError_;

   char cam_name[100];


public:

   CCameraUnit_PI();
   ~CCameraUnit_PI();


   // Control
   CImageData CaptureImage(long int& retryCount);
   void CancelCapture();

   // Accessors
   bool   CameraReady() const { return m_initializationOK;}
   const char *CameraName() const { return cam_name;}

   void   SetExposure(float exposureInSeconds);
   float  GetExposure() const { return exposure_;}

   void   SetShutterIsOpen(bool open);

   void SetReadout(int ReadSpeed);

   void   SetTemperature(double temperatureInCelcius);
   double GetTemperature() const;

   void   SetBinningAndROI(int x, int y, int x_min = 0, int x_max = 0, int y_min = 0, int y_max = 0);
   int    GetBinningX() const { return binningX_;}
   int    GetBinningY() const { return binningY_;}
   const ROI& GetROI() const;

   std::string GetStatus() const {CriticalSection::Lock lock(statusCriticalSection_); return status_;}

   int   GetCCDWidth()  const { return CCDWidth_;}
   int   GetCCDHeight() const { return CCDHeight_;}

private:
   void SetStatus(std::string newVal) {CriticalSection::Lock lock(statusCriticalSection_);  status_ = newVal;}
   bool StatusIsIdle();
   void SetShutter();

};

#endif // __CAMERAUNIT_PI_H__

