#ifndef __IMAGEDATA_H__
#define __IMAGEDATA_H__

#include <windows.h>
/////////////////////////////////////////////////////////////////////////////
// CImageData object

class ImageStats
{
   int min_;
   int max_;
   double mean_;
   double stddev_;

public:
   ImageStats(int min, int max, double mean, double stddev)
      : min_(min),max_(max),mean_(mean),stddev_(stddev){}


   int    GetMinValue()                       const { return min_; }
   int    GetMaxValue()                       const { return max_; }
   double         GetMeanValue()              const { return mean_; }
   double         GetStandardDeviationValue() const { return stddev_; }
};

class CImageData
{

   int m_imageWidth;
   int m_imageHeight;

   unsigned short* m_imageData;
public:
   float exposure;
   int bin_x, bin_y;
   

public:
   CImageData();
   CImageData(int imageWidth, int imageHeight, unsigned short* imageData = NULL);

   CImageData(const CImageData& rhs);
   CImageData& operator=(const CImageData& rhs);


   ~CImageData();

   void ClearImage();

   bool HasData() const { return m_imageData != NULL; }

   int    GetImageWidth()  const { return m_imageWidth; }
   int    GetImageHeight() const { return m_imageHeight;}

   ImageStats GetStats() const;



   const unsigned short* const GetImageData() const { return m_imageData; }
         unsigned short* const GetImageData()       { return m_imageData; }

   void Add(const CImageData& rsh);
   void ApplyBinning(int x, int y);
   void FlipHorizontal();
};

#endif // __IMAGEDATA_H__
