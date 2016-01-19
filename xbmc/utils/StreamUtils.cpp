/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "StreamUtils.h"

int StreamUtils::GetCodecPriority(AVCodecID codecId, int codecProfile)
{
  /*
   * Technically flac, truehd, and dtshd_ma are equivalently good as they're all lossless. However,
   * ffmpeg can't decode dtshd_ma losslessy yet.
   */
  if (codecId == AV_CODEC_ID_FLAC) // Lossless FLAC
    return 7;
  if (codecId == AV_CODEC_ID_TRUEHD) // Dolby TrueHD
    return 6;
  if (codecId == AV_CODEC_ID_DTS)
  {
    if (codecProfile == FF_PROFILE_DTS_HD_MA)  // DTS-HD Master Audio (previously known as DTS++)
      return 5;
    if (codecProfile == FF_PROFILE_DTS_HD_HRA) // DTS-HD High Resolution Audio
      return 4;
    
    return 2;
  }
  if (codecId == AV_CODEC_ID_EAC3) // Dolby Digital Plus
    return 3;
  if (codecId == AV_CODEC_ID_AC3) // Dolby Digital
    return 1;

  return 0;
}
