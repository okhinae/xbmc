/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://www.xbmc.org
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

#include "RawPicture.h"
#include "utils/log.h"

RawPicture::RawPicture()
{
  m_dll.Load();
  m_raw_data = NULL;
  if (m_dll.IsLoaded()) 
  {
    m_raw_data =  m_dll.libraw_init(0);
  }
  m_strMimeType = "";
  m_thumbnailbuffer = NULL;

}

RawPicture::~RawPicture()
{
  if (m_dll.IsLoaded()) 
  {
    m_dll.Unload();
    m_dll.libraw_close(m_raw_data);
  }
}

bool RawPicture::LoadImageFromMemory(unsigned char* buffer, unsigned int bufSize, unsigned int width, unsigned int height)
{
  if (!m_dll.IsLoaded() || m_raw_data == NULL)
    return false;

  std::string strExt = m_strMimeType;
  int nPos = strExt.find('/');
  if (nPos > -1)
    strExt.erase(0, nPos + 1);

  if(m_dll.libraw_open_buffer(m_raw_data, buffer, bufSize) != LIBRAW_SUCCESS)
  {
    CLog::Log(LOGERROR, "Texture manager unable to load image from memory");
    return false;
  }
  int err = 0;
  if ( (err = m_dll.libraw_unpack(m_raw_data)) != LIBRAW_SUCCESS)
  {
    CLog::Log(LOGERROR, "Texture manager unable to load image from memory");
    return false;
  }

  if ( (err = m_dll. libraw_dcraw_process(m_raw_data)) != LIBRAW_SUCCESS)
  {
    CLog::Log(LOGERROR, "Texture manager unable to load image from memory");
    return false;
  }
  m_width = m_raw_data->sizes.width;
  m_height = m_raw_data->sizes.height;

  if ( (err = m_dll.libraw_unpack_thumb(m_raw_data)) != LIBRAW_SUCCESS)
  {
    CLog::Log(LOGERROR, "Texture manager unable to load image from memory");
    return false;
  }

  return true;
}

bool RawPicture::Decode(const unsigned char *pixels, unsigned int pitch, unsigned int format)
{
  if (m_raw_data == NULL || m_raw_data->sizes.width == 0 || m_raw_data->sizes.height == 0 || !m_dll.IsLoaded())
    return false;

  int err = 0;
  m_raw_data->sizes.flip = 3;
  libraw_processed_image_t * image = m_dll.libraw_dcraw_make_mem_image(m_raw_data, &err);

#if 0
  unsigned int dstPitch = pitch;
  unsigned int srcPitch = ((m_width + 1)* 3 / 4) * 4; // bitmap row length is aligned to 4 bytes

  unsigned char *dst = (unsigned char*)pixels;
  unsigned char *src = (unsigned char*)image->data + (m_height - 1) * srcPitch;

  for (unsigned int y = 0; y < m_height; y++)
  {
    unsigned char *dst2 = dst;
    unsigned char *src2 = src;
    for (unsigned int x = 0; x < m_width; x++, dst2 += 4, src2 += 3)
    {
      dst2[0] = src2[2];
      dst2[1] = src2[1];
      dst2[2] = src2[0];
      dst2[3] = 0xff;
    }
    src -= srcPitch;
    dst += dstPitch;
  }
#else
  unsigned int dstPitch = pitch;
  unsigned int srcPitch = ((m_width + 1)* 3 / 4) * 4; // bitmap row length is aligned to 4 bytes

  unsigned char *dst = (unsigned char*)pixels;
  unsigned char *src = (unsigned char*)image->data;

  for (unsigned int y = 0; y < m_height; y++)
  {
    unsigned char *dst2 = dst;
    unsigned char *src2 = src;
    for (unsigned int x = 0; x < m_width; x++, dst2 += 4, src2 += 3)
    {
      dst2[0] = src2[2];
      dst2[1] = src2[1];
      dst2[2] = src2[0];
      dst2[3] = 0xff;
    }
    src += srcPitch;
    dst += dstPitch;
  }
#endif
  m_dll.libraw_dcraw_clear_mem(image);
  return true;
}
bool RawPicture::CreateThumbnailFromSurface(unsigned char* bufferin, unsigned int width, unsigned int height, unsigned int format, unsigned int pitch, const CStdString& destFile, 
                                unsigned char* &bufferout, unsigned int &bufferoutSize){return false;}
void RawPicture::ReleaseThumbnailBuffer(){return;}
