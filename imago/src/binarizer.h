/****************************************************************************
 * Copyright (C) 2009-2010 GGA Software Services LLC
 * 
 * This file is part of Imago toolkit.
 * 
 * This file may be distributed and/or modified under the terms of the
 * GNU General Public License version 3 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 * 
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 ***************************************************************************/

#ifndef _binarizer_h
#define _binarizer_h

#include "stl_fwd.h"
#include "image.h"

namespace imago
{
   class Binarizer
   {
   public:

      static void apply(Image &img, int threshold = -1);

   private:
	   static int _getThreshold(const Image &img);
      static bool _comparePair( IntPair a, IntPair b );
   };
}


#endif /* _binarizer_h */