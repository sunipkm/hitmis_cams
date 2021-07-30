#ifndef __CAMERAUNIT_H__
#define __CAMERAUNIT_H__


#include "ImageData.h"
#include <string>

typedef struct
{
   int x_min;
   int x_max;
   int y_min;
   int y_max;
} ROI;

const double INVALID_TEMPERATURE = -273.0;
class CCameraUnit
{
public:
   virtual ~CCameraUnit(){}; 

   // Control
   virtual CImageData CaptureImage(long int& retryCount)=0;
   virtual void CancelCapture() =0;

   // Accessors
   virtual bool   CameraReady() const =0;

   virtual const char *CameraName() const =0;

   virtual void   SetExposure(float exposureInMs) =0;
   virtual float  GetExposure() const =0;

   virtual void   SetShutterIsOpen(bool open) =0;

   virtual void   SetReadout(int ReadSpeed) = 0;

   virtual void   SetTemperature(double temperatureInCelcius)=0;
   virtual double GetTemperature() const=0;

   virtual void   SetBinningAndROI(int x, int y, int x_min = 0, int x_max = 0, int y_min = 0, int y_max = 0) =0;
   virtual int    GetBinningX() const=0;
   virtual int    GetBinningY() const=0;
   virtual const ROI&   GetROI()      const=0;

   virtual std::string GetStatus() const=0; // should return empty string when idle

   virtual int    GetCCDWidth() const  =0;
   virtual int    GetCCDHeight() const =0;
};

#endif // __CAMERAUNIT_H__

