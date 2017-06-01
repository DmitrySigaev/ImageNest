#pragma once
#include <math.h>
#include <vector>
#include <algorithm>
#include "Image.h"

namespace gga
{
    struct  ImageFilterParameters
    {
        bool    StretchImage;
        double  UnsharpMaskRadius;
        double  UnsharpMaskAmount;
        double  UnsharpMaskAmount2;
        double  UnsharpMaskThreshold;
        double  UnsharpMaskThreshold2;   //after stretch histogram
        size_t  CropBorder;
        size_t  RadiusBlur1;
        size_t  RadiusBlur2;
        size_t  VignettingHoleDistance;
        size_t  SmallDirtSize;
    public:
        inline ImageFilterParameters() : StretchImage(true), UnsharpMaskRadius(64), UnsharpMaskAmount(7), UnsharpMaskThreshold(3), UnsharpMaskThreshold2(130)
                                       , CropBorder(8), RadiusBlur1(4), VignettingHoleDistance(31), SmallDirtSize(4) {}
    };

    class ImageFilter
    {
        gga::Image& Image;
    public:
        ImageFilterParameters  Parameters;
        inline ImageFilter(gga::Image& img) : Image(img)
        {
            // compute optimal default parameters based on image resiolution
            Parameters.UnsharpMaskRadius = std::min(100, int(std::min(Image.getWidth(), Image.getHeight())/2));
            Parameters.UnsharpMaskAmount = 7.;
            Parameters.UnsharpMaskThreshold = 90;
            Parameters.UnsharpMaskAmount2 = 7.;
            Parameters.UnsharpMaskThreshold2= 130;
            Parameters.CropBorder   = 8;
            Parameters.RadiusBlur1  = 2;
            Parameters.RadiusBlur2  = 4;
            Parameters.SmallDirtSize= 2;   //4
            Parameters.VignettingHoleDistance = std::min(32, (int)Image.getWidth()/4);
        }
        void  prepareImageForVectorization();
        Coord computeLineWidthHistogram   (std::vector<size_t>* histogram, size_t size = -1);
    };

//==============================================================================
// Image processing routines
//==============================================================================

    static inline size_t makeHistogram  (const Image& img, std::vector<size_t>* histogram)
    {
        size_t maxh = 0;
        histogram->resize(256);
        memset(&(*histogram)[0], 0, 256);
        for (size_t y = 0; y < img.getHeight(); y++)
         for(size_t x = 0; x < img.getWidth() ; x++)
        {
            size_t v = ++(*histogram)[ img.getPixel(x, y).Value];
            if (maxh < v)
                maxh = v;
        }
        return maxh;
    }

    static inline void stretchImageHistogram(Image* img, size_t minColor, size_t maxColor)
    {
        size_t normalization = (255<<16)/(maxColor-minColor+1);
        for (size_t y = 0; y < img->getHeight(); y++)
         for(size_t x = 0; x < img->getWidth() ; x++)
        {
            size_t v = img->getPixel(x, y).Value;
            if(v < minColor)
                v = 0;
            else if(v > maxColor)
                v = 255;
            else
                v = (normalization*(v - minColor) + 0x7FFF)>>16;
            img->setPixel(x, y, Pixel(v));
        }
    }

    static inline void stretchImageHistogram(Image* img, size_t nmin)
    {
        std::vector<size_t> histogram;
        makeHistogram (*img, &histogram);
        size_t minColor, maxColor;
        for(minColor = 0; minColor < histogram.size() && histogram[minColor] < nmin; minColor++)
        {}
        for(maxColor= histogram.size()-1; maxColor > minColor && histogram[maxColor] < nmin; maxColor--)
        {}

        stretchImageHistogram(img, minColor, maxColor);
    }

    static inline void blurImage(Image* img, size_t r)
    {
        for (size_t y = 0; y < img->getHeight(); y++)
         for(size_t x = 0; x < img->getWidth() ; x++)
        {
            size_t px = 0;//img->getPixel(x, y).Value * (r+1)*(r+1);
            size_t weight = 0; //(r+1)*(r+1)
            size_t nx = r, ny = r;
            if( x+nx > img->getWidth())
                nx = img->getWidth() - x;
            if( y+ny > img->getHeight())
                ny = img->getHeight() - y;
            for (size_t iy = 0; iy < ny; iy++)
             for(size_t ix = 0; ix < nx; ix++) // average area
             {
                //int dw = (int)r+1 - (int)sqrt(double(ix*ix + iy*iy));   //linear weight
                int dw = int((r+1)*(r+1) - ix*ix + iy*iy);            //very similar to Gaussian blur matrix, if r is small
                if(dw > 0)
                    px += img->getPixel(x+ix, y+iy).Value * dw;
                weight += dw;
             }
            px /= weight;
            img->setPixel(x, y, Pixel(px));
        }
    }

/* unsharpMaskImage()
%
%  A description of each parameter follows:
%
%    o image: the image.
%
%    o channel: the channel type.
%
%    o radius: the radius of the Gaussian, in pixels, not counting the center
%      pixel.
%
%    o sigma: the standard deviation of the Gaussian, in pixels.
%
%    o amount: the percentage of the difference between the original and the
%      blur image that is added back into the original.
%
%    o threshold: the threshold in pixels needed to apply the diffence amount.
%
%    o exception: return any errors or warnings in this structure.
%
*/
    void unsharpMaskImage(Image* img, const double radius, const double sigma, const double amount,const int threshold);

    static inline size_t getBackgroundValue(const Image& img)
    {
        std::vector<size_t> histogram;
        size_t maxh = makeHistogram (img, &histogram);
        for(size_t v = 2; v < histogram.size()-2; v++)
        {
            if(histogram[v-2] + histogram[v-1] + histogram[v] >= maxh/3)    // area of light background
            {
                for(v--; v > 0; v--)
                {
                    if(histogram[v] <= histogram[v+1]+12 || histogram[v] <= histogram[v+2]+12)
                        break;
//                    if(histogram[v] + histogram[v+1] + histogram[v+2] < maxh/64)    // exact begin of light background
//                        break;
                }
                return v;
            }
        }
        return 256/3;   //not found - return some middle value.
    }

    static inline void convertGrayscaleToBlackWhite (Image* img, const Pixel level)
    {
        for (size_t y = 0; y < img->getHeight(); y++)
         for(size_t x = 0; x < img->getWidth() ; x++)
        {
            Pixel px = img->getPixel(x, y);
            px.Value = (px.Value >= level.Value) ? BACKGROUND : INK;
            img->setPixel(x, y, px);
        }
    }

    static inline size_t clearSolidLine (Image* img, size_t xo, size_t xend, size_t y, size_t r)
    {
        size_t n = 0;
        if(xo <= xend)
        {
            for(size_t x = xo; x <= xend; x++)
            {
                if(img->getPixel(x, y).isBackground())   // hole or end of dark area
                {
                    size_t i;
                    for(i = 1; i <= r && x <= xend; i++, x++)
                        if(!img->getPixel(x, y).isBackground())
                        {
                            n++;
                            img->setPixel(x, y, BACKGROUND);
                            break;
                        }
                    if(i > r)
                        return n;  //stop on big hole
                }
                else //clean solid area
                {
                    n++;
                    img->setPixel(x, y, BACKGROUND);
                }
/*
                bool clean;
                do
                {
                    clean = false;
                    for(size_t i = 0; i <= r && !clean && x+i <= xend; i++)
                        if(!img.getPixel(x+i, y).isBackground())
                            clean = true;
                    if(clean)
                    {
                        for(size_t i = 0; i <= r && x+i <= xend; i++)
                        {
                            n++;
                            img.setPixel(x+i, y, BACKGROUND);
                        }
                        x += r - 1;
                    }
                    else
                        return n;
                } while(clean);
*/
            }
        }
        else    // reverse direction (from rigth to left
        {
            for(size_t x = xo; x >= xend; x--)
            {
                if(img->getPixel(x, y).isBackground())   // hole or end of dark area
                {
                    size_t i;
                    for(i = 1; i <= r && x >= xend; i++, x--)
                        if(!img->getPixel(x, y).isBackground())
                        {
                            n++;
                            img->setPixel(x, y, BACKGROUND);
                            break;
                        }
                    if(i > r)
                        return n;  //stop on big hole
                }
                else //clean solid area
                {
                    n++;
                    img->setPixel(x, y, BACKGROUND);
                }
/*
                bool clean;
                do
                {
                    clean = false;
                    for(size_t i = r+1; i > 0 && !clean && x-(i-1) >= xend; i--)
                        if(!img.getPixel(x-i-1, y).isBackground())
                            clean = true;
                    if(clean)
                    {
                        for(size_t i = r+1; i > 0 && x-(i-1) >= xend; i--)
                        {
                            n++;
                            img.setPixel(x-(i-1), y, BACKGROUND);
                        }
                        x -= r;
                    }
                    else
                        return n;
                } while(clean);
*/
            }
        }
        return n;
    }

    static inline void clearCorners (Image* img, size_t r)
    {
        for (size_t y = 0; y < img->getHeight()/2; y++)                  // left top corner
            if(0 == clearSolidLine (img, 0, img->getWidth()/2, y, r))
                {}//break;
        for (size_t y = img->getHeight()-1; y >= img->getHeight()/2; y--)  // left bottom corner
            if(0 == clearSolidLine (img, 0, img->getWidth()/2, y, r))
                {}//break;
        for (size_t y = 0; y < img->getHeight()/2; y++)                  // right top corner
            if(0 == clearSolidLine (img, img->getWidth()-1, img->getWidth() - img->getWidth()/2, y, r))
                {}//break;
        for (size_t y = img->getHeight()-1; y >= img->getHeight()/2; y--)  // right bottom  corner
            if(0 == clearSolidLine (img, img->getWidth()-1, img->getWidth() - img->getWidth()/2, y, r))
            {}//break;
    }

    static inline void cropImageToPicture(Image* img)
    {
        size_t xend=0, yend=0;
        size_t xo=img->getWidth()-1, yo=img->getHeight()-1;

        for (size_t y = 0; y < img->getHeight(); y++)
        {
            bool includeLine = false;
            for(size_t x = 0; x < img->getWidth(); x++)
            {
                Pixel px = img->getPixel(x, y);
                if(px.Value < BACKGROUND/2)  //!px.isBackground())
                {
                    includeLine = true;
                    if(x < xo)
                        xo = x;
                    if(x > xend)
                        xend = x;
                }
            }
            if(includeLine)
            {
                if(y < yo)
                    yo = y;
                if(y > yend)
                    yend = y;
            }
        }

        if (xo > xend)
        {
            size_t tmp = xo;
            xo = xend;
            xend = tmp;
        }
        if (yo > yend)
        {
            size_t tmp = yo;
            yo = yend;
            yend = tmp;
        }

        // keep small white border
        if (xo >= 4)
            xo -= 4;
        else 
            xo = 0;
        if (yo >= 4)
            yo -= 4;
        else 
            yo = 0;
        if (xend <= img->getWidth()-1 - 4)
            xend += 4;
        else 
            xend = img->getWidth()-1;
        if (yend <= img->getHeight()-1 - 4)
            yend += 4;
        else 
            yend = img->getHeight()-1;

        img->crop(xo, yo, xend, yend);
    }

    void rotateImage(const Image& img, double angle, Image* out);

    static inline void resampleImageBiCubic(const Image& img, size_t cx, size_t cy, Image* out)
    {

        //out->setSize(cx, cy, img.getType());
    }

}
