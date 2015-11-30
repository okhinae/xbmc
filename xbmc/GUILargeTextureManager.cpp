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

#include "threads/SystemClock.h"
#include "GUILargeTextureManager.h"
#include "settings/Settings.h"
#include "guilib/Texture.h"
#include "threads/SingleLock.h"
#include "utils/TimeUtils.h"
#include "utils/JobManager.h"
#include "guilib/GraphicContext.h"
#include "utils/log.h"
#include "TextureCache.h"

#include <cassert>
#include "guilib/Gif.h"

CImageLoader::CImageLoader(const std::string &path, const bool useCache):
  m_path(path)
{
  m_texture = new CTextureArray();
  m_use_cache = useCache;
}

CImageLoader::~CImageLoader()
{
  delete(m_texture);
}

bool CImageLoader::DoWork()
{
  bool needsChecking = false;
  std::string loadPath;

  std::string texturePath = g_TextureManager.GetTexturePath(m_path);
  if (m_use_cache)
    loadPath = CTextureCache::GetInstance().CheckCachedImage(texturePath, true, needsChecking);
  else
    loadPath = texturePath;

  if (!loadPath.empty())
  {
    // direct route - load the image
    unsigned int start = XbmcThreads::SystemClockMillis();

    if (StringUtils::EndsWithNoCase(loadPath, ".gif"))
    {
#if defined(HAS_GIFLIB)
      Gif gif;
      if (!gif.LoadGif(loadPath.c_str()))
      {
        CLog::Log(LOGERROR, "Texture manager unable to load file: %s", loadPath.c_str());
        return nullptr;
      }

      unsigned int maxWidth = 0;
      unsigned int maxHeight = 0;

      for (auto frame : gif.GetFrames())
      {
        CTexture *glTexture = new CTexture();
        if (glTexture)
        {
          glTexture->LoadFromMemory(gif.Width(), gif.Height(), gif.GetPitch(), XB_FMT_A8R8G8B8, false, frame->m_pImage);
          m_texture->Add(glTexture, frame->m_delay);
          maxWidth = std::max(maxWidth, glTexture->GetWidth());
          maxHeight = std::max(maxHeight, glTexture->GetHeight());
        }
      }

      m_texture->m_width = ((int)maxWidth);
      m_texture->m_height = ((int)maxHeight);
      m_texture->m_loops = gif.GetNumLoops();

#endif//HAS_GIFLIB
    }

     CBaseTexture* tex = CBaseTexture::LoadFromFile(loadPath, g_graphicsContext.GetWidth(), g_graphicsContext.GetHeight());
     if (tex)
     {
	     m_texture->Add(tex, 0);
	     m_texture->m_width = tex->GetTextureWidth();
	     m_texture->m_height = tex->GetTextureHeight();
     } 
     else
     {
       CLog::Log(LOGERROR, "%s - Direct texture file loading failed for %s", __FUNCTION__, loadPath.c_str());
     }

    if (XbmcThreads::SystemClockMillis() - start > 100)
      CLog::Log(LOGDEBUG, "%s - took %u ms to load %s", __FUNCTION__, XbmcThreads::SystemClockMillis() - start, loadPath.c_str());

    if (m_texture)
    {
      if (needsChecking)
        CTextureCache::GetInstance().BackgroundCacheImage(texturePath);

      return true;
    }

    // Fallthrough on failure:
    CLog::Log(LOGERROR, "%s - Direct texture file loading failed for %s", __FUNCTION__, loadPath.c_str());
  }

  if (!m_use_cache)
    return false; // We're done

  // not in our texture cache or it failed to load from it, so try and load directly and then cache the result
  CTextureCache::GetInstance().CacheImage(texturePath, &m_texture);
  return (m_texture != NULL);
}

CGUILargeTextureManager::CLargeTexture::CLargeTexture(const std::string &path):
  m_path(path)
{
  m_refCount = 1;
  m_timeToDelete = 0;
}

CGUILargeTextureManager::CLargeTexture::~CLargeTexture()
{
  assert(m_refCount == 0);
  m_texture.Free();
}

void CGUILargeTextureManager::CLargeTexture::AddRef()
{
  m_refCount++;
}

bool CGUILargeTextureManager::CLargeTexture::DecrRef(bool deleteImmediately)
{
  assert(m_refCount);
  m_refCount--;
  if (m_refCount == 0)
  {
    if (deleteImmediately)
      delete this;
    else
      m_timeToDelete = CTimeUtils::GetFrameTime() + TIME_TO_DELETE;
    return true;
  }
  return false;
}

bool CGUILargeTextureManager::CLargeTexture::DeleteIfRequired(bool deleteImmediately)
{
  if (m_refCount == 0 && (deleteImmediately || m_timeToDelete < CTimeUtils::GetFrameTime()))
  {
    delete this;
    return true;
  }
  return false;
}

void CGUILargeTextureManager::CLargeTexture::SetTexture(CTextureArray* texture)
{
  assert(!m_texture.size());
  if (texture)
  {
    for (int i = 0; i < texture->m_textures.size(); ++i)
    {
      m_texture.Add(texture->m_textures[i], texture->m_delays[i]);
    }

    m_texture.m_width = texture->m_width;
    m_texture.m_height = texture->m_height;
    m_texture.m_orientation = texture->m_orientation;
  }
}

CGUILargeTextureManager::CGUILargeTextureManager()
{
}

CGUILargeTextureManager::~CGUILargeTextureManager()
{
}

void CGUILargeTextureManager::CleanupUnusedImages(bool immediately)
{
  CSingleLock lock(m_listSection);
  // check for items to remove from allocated list, and remove
  listIterator it = m_allocated.begin();
  while (it != m_allocated.end())
  {
    CLargeTexture *image = *it;
    if (image->DeleteIfRequired(immediately))
      it = m_allocated.erase(it);
    else
      ++it;
  }
}

// if available, increment reference count, and return the image.
// else, add to the queue list if appropriate.
bool CGUILargeTextureManager::GetImage(const std::string &path, CTextureArray &texture, bool firstRequest, const bool useCache)
{
  CSingleLock lock(m_listSection);
  for (listIterator it = m_allocated.begin(); it != m_allocated.end(); ++it)
  {
    CLargeTexture *image = *it;
    if (image->GetPath() == path)
    {
      if (firstRequest)
        image->AddRef();
      texture = image->GetTexture();
      return texture.size() > 0;
    }
  }

  if (firstRequest)
    QueueImage(path, useCache);

  return true;
}

void CGUILargeTextureManager::ReleaseImage(const std::string &path, bool immediately)
{
  CSingleLock lock(m_listSection);
  for (listIterator it = m_allocated.begin(); it != m_allocated.end(); ++it)
  {
    CLargeTexture *image = *it;
    if (image->GetPath() == path)
    {
      if (image->DecrRef(immediately) && immediately)
        m_allocated.erase(it);
      return;
    }
  }
  for (queueIterator it = m_queued.begin(); it != m_queued.end(); ++it)
  {
    unsigned int id = it->first;
    CLargeTexture *image = it->second;
    if (image->GetPath() == path && image->DecrRef(true))
    {
      // cancel this job
      CJobManager::GetInstance().CancelJob(id);
      m_queued.erase(it);
      return;
    }
  }
}

// queue the image, and start the background loader if necessary
void CGUILargeTextureManager::QueueImage(const std::string &path, bool useCache)
{
  CSingleLock lock(m_listSection);
  for (queueIterator it = m_queued.begin(); it != m_queued.end(); ++it)
  {
    CLargeTexture *image = it->second;
    if (image->GetPath() == path)
    {
      image->AddRef();
      return; // already queued
    }
  }

  // queue the item
  CLargeTexture *image = new CLargeTexture(path);
  unsigned int jobID = CJobManager::GetInstance().AddJob(new CImageLoader(path, useCache), this, CJob::PRIORITY_NORMAL);
  m_queued.push_back(std::make_pair(jobID, image));
}

void CGUILargeTextureManager::OnJobComplete(unsigned int jobID, bool success, CJob *job)
{
  // see if we still have this job id
  CSingleLock lock(m_listSection);
  for (queueIterator it = m_queued.begin(); it != m_queued.end(); ++it)
  {
    if (it->first == jobID)
    { // found our job
      CImageLoader *loader = (CImageLoader *)job;
      CLargeTexture *image = it->second;
      image->SetTexture(loader->m_texture);
      loader->m_texture = NULL; // we want to keep the texture, and jobs are auto-deleted.
      m_queued.erase(it);
      m_allocated.push_back(image);
      return;
    }
  }
}
