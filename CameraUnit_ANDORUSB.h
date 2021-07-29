#ifndef __CAMERAUNIT_ANDORUSB_H__
#define __CAMERAUNIT_ANDORUSB_H__


#include "CameraUnit.h"

#include "atmcd32d.h"
#include "CriticalSection.h"

/////////////////////////////////////////////////////////////////////////////
// CCameraUnit object

class CCameraUnit_ANDORUSB: public CCameraUnit
{
   bool m_initializationOK;
   CriticalSection criticalSection_;
   bool cancelCapture_;
   std::string status_;
   CriticalSection statusCriticalSection_;

   const int shutterDelayInMs_;
   float exposure_;
   bool requestShutterOpen_;

   int binningX_;
   int binningY_;

   int CCDWidth_;
   int CCDHeight_;

   int width_;
   int height_;

   mutable volatile unsigned int lastError_;


public:

   CCameraUnit_ANDORUSB(int shutterDelayInMs = 50, unsigned int readOutSpeed = 0);
   ~CCameraUnit_ANDORUSB();


   // Control
   CImageData CaptureImage(long int& retryCount);
   void CancelCapture();

   // Accessors
   void   SetExposure(float exposureInSeconds);
   float  GetExposure() const;

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
   bool HasError(unsigned int error) const;
   void SetShutter();

};

#endif // __CAMERAUNIT_ANDORUSB_H__

