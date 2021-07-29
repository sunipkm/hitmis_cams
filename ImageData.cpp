#include "ImageData.h"

#include <math.h>
#include <algorithm>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


void CImageData::ClearImage()
{
   delete[] m_imageData;
   m_imageData = 0;

   m_imageWidth = 0;
   m_imageHeight = 0;
}

CImageData::CImageData()
: m_imageData(NULL)
{
   ClearImage();
}


CImageData::CImageData(int imageWidth, int imageHeight, unsigned short* imageData)
: m_imageData(NULL)
{
   ClearImage();

   if ((imageWidth <= 0) || (imageHeight <= 0))
   {
      return;
   }

   m_imageData = new unsigned short[imageWidth * imageHeight];
   if (m_imageData == NULL)
   {
      return;
   }

   if (imageData)
   {
      memcpy(m_imageData, imageData, imageWidth * imageHeight * sizeof(unsigned short));
   }
   else
   {
      memset(m_imageData, 0, imageWidth * imageHeight * sizeof(unsigned short));
   }
   m_imageWidth = imageWidth;
   m_imageHeight = imageHeight;
}


CImageData::CImageData(const CImageData& rhs)
: m_imageData(NULL)
{
   ClearImage();
   
   if (  (rhs.m_imageWidth == 0)
      || (rhs.m_imageHeight == 0)
      || (rhs.m_imageData == 0)   )
   {
      return;
   }


   m_imageData = new unsigned short[rhs.m_imageWidth * rhs.m_imageHeight];

   if (m_imageData == 0)
   {
      return;
   }

   memcpy(m_imageData, rhs.m_imageData, rhs.m_imageWidth * rhs.m_imageHeight * sizeof(unsigned short));
   m_imageWidth             = rhs.m_imageWidth            ;
   m_imageHeight            = rhs.m_imageHeight           ;
}


CImageData& CImageData::operator=(const CImageData& rhs)
{
   if (&rhs == this)
   { // self asignment
      return *this;
   }

   ClearImage();


   if (  (rhs.m_imageWidth == 0)
      || (rhs.m_imageHeight == 0)
      || (rhs.m_imageData == 0)   )
   {
      return *this;
   }


   m_imageData = new unsigned short[rhs.m_imageWidth * rhs.m_imageHeight];

   if (m_imageData == 0)
   {
      return *this;
   }

   memcpy(m_imageData, rhs.m_imageData, rhs.m_imageWidth * rhs.m_imageHeight * sizeof(unsigned short));
   m_imageWidth             = rhs.m_imageWidth            ;
   m_imageHeight            = rhs.m_imageHeight           ;
   return *this;
}


CImageData::~CImageData()
{
   delete[] m_imageData;
}



ImageStats CImageData::GetStats() const
{
   if (!m_imageData)
   {
      return ImageStats(0,0,0,0);
   }

   int min = 0xFFFF;
   int max = 0;
   double mean = 0.0;

   // Calculate min max and mean

   unsigned short* imageDataPtr = m_imageData;
   double rowDivisor = m_imageHeight * m_imageWidth;
   unsigned long rowSum;
   double rowAverage;
   unsigned short currentPixelValue;

   int rowIndex;
   int columnIndex;

   for (rowIndex = 0; rowIndex < m_imageHeight; rowIndex++)
   {
      rowSum = 0L;

      for (columnIndex = 0; columnIndex < m_imageWidth; columnIndex++)
      {
         currentPixelValue = *imageDataPtr;

         if (currentPixelValue < min)
         {
            min = currentPixelValue;
         }

         if (currentPixelValue > max)
         {
            max = currentPixelValue;
         }

         rowSum += currentPixelValue;

         imageDataPtr++;
      }

      rowAverage = static_cast<double>(rowSum) / rowDivisor;

      mean += rowAverage;
   }

   // Calculate standard deviation

   double varianceSum = 0.0;
   imageDataPtr = m_imageData;

   for (rowIndex = 0; rowIndex < m_imageHeight; rowIndex++)
   {
      for (columnIndex = 0; columnIndex < m_imageWidth; columnIndex++)
      {
         double tempValue = (*imageDataPtr) - mean;
         varianceSum += tempValue * tempValue;
         imageDataPtr++;
      }
   }

   double stddev = sqrt(varianceSum / static_cast<double>((m_imageWidth * m_imageHeight) - 1));

   return ImageStats(min,max,mean,stddev);
}


void CImageData::Add(const CImageData& rhs)
{
   unsigned short* sourcePixelPtr = rhs.m_imageData;
   unsigned short* targetPixelPtr = m_imageData;
   unsigned long newPixelValue;

   if ( ! rhs.HasData() ) return;

   // if we don't have data yet we simply copy the rhs data
   if ( ! this->HasData() )
   {
      *this = rhs;
      return;
   }

   // we do have data, make sure our size matches the new size
   if ( ( rhs.m_imageWidth != m_imageWidth) || (rhs.m_imageHeight != m_imageHeight) ) return;
   
   for (int pixelIndex = 0;
         pixelIndex < (m_imageWidth * m_imageHeight);
         pixelIndex++)
   {
      newPixelValue = *targetPixelPtr + *sourcePixelPtr;
      
      if (newPixelValue > 0xFFFF)
      {
         *targetPixelPtr = 0xFFFF;
      }
      else
      {
         *targetPixelPtr = static_cast<unsigned short>(newPixelValue);
      }
      
      sourcePixelPtr++;
      targetPixelPtr++;
   }
}

void CImageData::ApplyBinning(int binX, int binY)
{
   if ( ! HasData() ) return;
   if ((binX == 1) && (binY == 1))
   { // No binning to apply
      return;
   }

   short newImageWidth = GetImageWidth() / binX;
   short newImageHeight = GetImageHeight() / binY;

   short binSourceImageWidth = newImageWidth * binX;
   short binSourceImageHeight = newImageHeight * binY;

   unsigned short* newImageData = new unsigned short[newImageHeight * newImageWidth];

   memset(newImageData, 0, newImageHeight * newImageWidth * sizeof(unsigned short));

   // Bin the data into the new image space allocated
   for (int rowIndex = 0; rowIndex < binSourceImageHeight; rowIndex++)
   {
      const unsigned short* sourceImageDataPtr = GetImageData() + (rowIndex *  GetImageWidth());

      for (int columnIndex = 0; columnIndex < binSourceImageWidth; columnIndex++)
      {
         unsigned short* targetImageDataPtr = newImageData 
                                             + ( ((rowIndex / binY) * newImageWidth) +
                                                  (columnIndex / binX));

         unsigned long newPixelValue = *targetImageDataPtr + *sourceImageDataPtr;

         if (newPixelValue > 0xFFFF)
         {
            *targetImageDataPtr = 0xFFFF;
         }
         else
         {
            *targetImageDataPtr = static_cast<unsigned short>(newPixelValue);
         }

         sourceImageDataPtr++;
      }
   }

   delete[] m_imageData;
   m_imageData  = newImageData;
   m_imageWidth = newImageWidth;
   m_imageHeight = newImageHeight;
}

