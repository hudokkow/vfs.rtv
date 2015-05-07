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

#include "kodi/libXBMC_addon.h"
#include "platform/threads/mutex.h"
#include <map>
#include <sstream>
#include <tinyxml.h>

#define FILE_ATTRIBUTE_DIRECTORY           0x00000010

ADDON::CHelper_libXBMC_addon *XBMC           = NULL;

extern "C" {

#include "kodi/kodi_vfs_dll.h"
#include "kodi/IFileTypes.h"
#include "interface.h"

//-- Create -------------------------------------------------------------------
// Called on load. Addon should fully initalize or return error status
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_Create(void* hdl, void* props)
{
  if (!XBMC)
    XBMC = new ADDON::CHelper_libXBMC_addon;

  if (!XBMC->RegisterMe(hdl))
  {
    delete XBMC, XBMC=NULL;
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  return ADDON_STATUS_OK;
}

//-- Stop ---------------------------------------------------------------------
// This dll must cease all runtime activities
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
void ADDON_Stop()
{
}

//-- Destroy ------------------------------------------------------------------
// Do everything before unload of this add-on
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
void ADDON_Destroy()
{
  XBMC=NULL;
}

//-- HasSettings --------------------------------------------------------------
// Returns true if this add-on use settings
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
bool ADDON_HasSettings()
{
  return false;
}

//-- GetStatus ---------------------------------------------------------------
// Returns the current Status of this visualisation
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_GetStatus()
{
  return ADDON_STATUS_OK;
}

//-- GetSettings --------------------------------------------------------------
// Return the settings for XBMC to display
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
unsigned int ADDON_GetSettings(ADDON_StructSetting ***sSet)
{
  return 0;
}

//-- FreeSettings --------------------------------------------------------------
// Free the settings struct passed from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------

void ADDON_FreeSettings()
{
}

//-- SetSetting ---------------------------------------------------------------
// Set a specific Setting value (called from XBMC)
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_SetSetting(const char *strSetting, const void* value)
{
  return ADDON_STATUS_OK;
}

//-- Announce -----------------------------------------------------------------
// Receive announcements from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
void ADDON_Announce(const char *flag, const char *sender, const char *message, const void *data)
{
}

struct RTVContext
{
  struct rtv_data* rtvd;
  int port;
  std::string hostname;
  std::string filename;
  int64_t size;
  int64_t pos;

  RTVContext()
  {
    rtvd = NULL;
    pos = 0;
  }
};

static bool Open_internal(RTVContext* ctx, const char* hostname,
                          const char* filename, unsigned int port)
{
  // Set up global variables.  Don't set m_filePos to 0 because we use it to SEEK!
  ctx->size = 0;
  ctx->hostname = hostname;
  ctx->filename = filename;
  ctx->port = port;

  // Allow for ReplayTVs on ports other than 80
  std::string strHostAndPort;
  strHostAndPort = hostname;
  if (port)
  {
    char buffer[10];
    sprintf(buffer,"%i",port);
    strHostAndPort += ':';
    strHostAndPort += buffer;
  }

  // Get the file size of strFileName.  If size is 0 or negative, file doesn't exist so exit.
  uint64_t size;
  size = rtv_get_filesize(strHostAndPort.c_str(), filename);
  if (!size)
  {
    XBMC->Log(ADDON::LOG_ERROR, "%s - Failed to get filesize of %s on %s", __FUNCTION__, hostname, filename);
    return false;
  }
  ctx->size = size;

  // Open a connection to strFileName stating at position m_filePos
  // Store the handle to the connection in m_rtvd.  Exit if handle invalid.
  ctx->rtvd = rtv_open_file(strHostAndPort.c_str(), filename, ctx->pos);
  if (!ctx->rtvd)
  {
    XBMC->Log(ADDON::LOG_ERROR, "%s - Failed to open %s on %s", __FUNCTION__, hostname, filename);
    return false;
  }

  XBMC->Log(ADDON::LOG_DEBUG, "%s - Opened %s on %s, Size %"PRIu64", Position %"PRIu64"", __FUNCTION__, hostname, filename, ctx->size, ctx->pos);
  return true;
}

void* Open(VFSURL* url)
{
  RTVContext* result = new RTVContext;

  if (Open_internal(result, url->hostname, url->filename, url->port))
    return result;

  delete result;
  return NULL;
}

bool Close(void* context)
{
  RTVContext* ctx = (RTVContext*)context;

  if (ctx->rtvd)
    rtv_close_file(ctx->rtvd);

  delete ctx;

  return true;
}

int64_t GetLength(void* context)
{
  RTVContext* ctx = (RTVContext*)context;

  return ctx->size;
}

//*********************************************************************************************
int64_t GetPosition(void* context)
{
  RTVContext* ctx = (RTVContext*)context;

  return ctx->pos;
}


int64_t Seek(void* context, int64_t iFilePosition, int iWhence)
{
  int64_t newpos;

  RTVContext* ctx = (RTVContext*)context;

  switch (iWhence)
  {
  case SEEK_SET:
    // cur = pos
    newpos = iFilePosition;
    break;
  case SEEK_CUR:
    // cur += pos
    newpos = ctx->pos + iFilePosition;
    break;
  case SEEK_END:
    // end += pos
    newpos = ctx->size + iFilePosition;
    break;
  default:
    return -1;
  }
  // Return offset from beginning
  if (newpos > ctx->size)
    newpos = ctx->size;

  if (ctx->pos != newpos)
  {
    ctx->pos = newpos;
    Open_internal(ctx, ctx->hostname.c_str(), ctx->filename.c_str(), ctx->port);
  }

  return ctx->pos;
}

bool Exists(VFSURL* url)
{
  return true;
}

int Stat(VFSURL* url, struct __stat64* buffer)
{
  return -1;
}

int IoControl(void* context, XFILE::EIoControl request, void* param)
{
  return -1;
}

void ClearOutIdle()
{
}

void DisconnectAll()
{
}

bool DirectoryExists(VFSURL* url)
{
  return false;
}

static int Replace(std::string &str, const std::string &oldStr, const std::string &newStr)
{
  if (oldStr.empty())
    return 0;

  int replacedChars = 0;
  size_t index = 0;

  while (index < str.size() && (index = str.find(oldStr, index)) != std::string::npos)
  {
    str.replace(index, oldStr.size(), newStr);
    index += newStr.size();
    replacedChars++;
  }

  return replacedChars;
}

void* GetDirectory(VFSURL* url, VFSDirEntry** items,
                   int* num_items, VFSCallbacks* callbacks)
{
  std::string strRoot = url->url;
  if (strRoot[strRoot.size()-1] != '/')
    strRoot += '/';

  std::string host(url->hostname);

  // Host name is "*" so we try to discover all ReplayTVs.  This requires some trickery but works.
  if (strcmp(url->hostname,"*") == 0)
  {
    // Check to see whether the URL's path is blank or "Video"
    if (strlen(url->filename) == 0u || strcmp(url->filename, "Video") == 0)
    {
      int iOldSize=0;
      struct RTV * rtv = NULL;
      int numRTV;

      // Request that all ReplayTVs on the LAN identify themselves.  Each ReplayTV
      // is given 3000ms to respond to the request.  rtv_discovery returns an array
      // of structs containing the IP and friendly name of all ReplayTVs found on the LAN.
      // For some reason, DVArchive doesn't respond to this request (probably only responds
      // to requests from an IP address of a real ReplayTV).
      numRTV = rtv_discovery(&rtv, 3000);

      // Run through the array and add the ReplayTVs found as folders in XBMC.
      // We must add the IP of each ReplayTV as if it is a file name at the end of a the
      // auto-discover URL--e.g. rtv://*/192.168.1.100--because XBMC does not permit
      // dyamically added shares and will not play from them.  This little trickery is
      // the best workaround I could come up with.

      std::vector<VFSDirEntry>* itms = new std::vector<VFSDirEntry>(numRTV);
      for (int i = 0; i < numRTV; i++)
      {
        (*itms)[i].path = strdup((strRoot+rtv[i].hostname).c_str());
        (*itms)[i].label = strdup(rtv[i].friendlyName);
        (*itms)[i].title = NULL;
        (*itms)[i].folder = true;
        (*itms)[i].properties = new VFSProperty;
        (*itms)[i].properties->name = strdup("propmisusepreformatted");
        (*itms)[i].properties->val = strdup("true");
        (*itms)[i].num_props = 1;
      }
      free(rtv);

      *items = &(*itms)[0];
      *num_items = itms->size();
      return itms;

      // Else the URL's path should be an IP address of the ReplayTV
    }
    else
    {
      std::string strURL, strRTV;
      size_t pos;

      // Isolate the IP from the URL and replace the "*" with the real IP
      // of the ReplayTV.  E.g., rtv://*/Video/192.168.1.100/ becomes
      // rtv://192.168.1.100/Video/ .  This trickery makes things work.
      size_t rpos = strRoot.rfind("/");
      strURL = strRoot.substr(0,rpos);
      pos = strURL.rfind('/');
      strRTV = strURL.substr(0, rpos + 1);
      Replace(strRTV, "*", strURL.substr(pos + 1));
      host = strURL.substr(pos+1);
      strRoot = strRTV;
    }
  }

  // Allow for ReplayTVs on ports other than 80
  std::string strHostAndPort;
  strHostAndPort = host;
  if (url->port != 0)
  {
    char buffer[10];
    sprintf(buffer,"%i", url->port);
    strHostAndPort += ':';
    strHostAndPort += buffer;
  }

  std::vector<VFSDirEntry>* itms = new std::vector<VFSDirEntry>();
  // No path given, list shows from ReplayGuide
  if (strlen(url->filename) == 0)
  {
    unsigned char * data = NULL;

    // Get the RTV guide data in XML format
    rtv_get_guide_xml(&data, strHostAndPort.c_str());

    // Begin parsing the XML data
    TiXmlDocument xmlDoc;
    xmlDoc.Parse( (const char *) data );
    if ( xmlDoc.Error() )
    {
      free(data);
      return NULL;
    }
    TiXmlElement* pRootElement = xmlDoc.RootElement();
    if (!pRootElement)
    {
      free(data);
      return NULL;
    }

    const TiXmlNode *pChild = pRootElement->FirstChild();
    while (pChild > 0)
    {
      std::string strTagName = pChild->Value();

      if ( !strcasecmp(strTagName.c_str(), "ITEM") )
      {
        const TiXmlNode *nameNode = pChild->FirstChild("DISPLAYNAME");
        const TiXmlNode *recordedNode = pChild->FirstChild("RECORDED");
        const TiXmlNode *pathNode = pChild->FirstChild("PATH");
        const TiXmlNode *sizeNode = pChild->FirstChild("SIZE");
        const TiXmlNode *atrbNode = pChild->FirstChild("ATTRIB");

        int64_t dwFileSize = 0;
        //SYSTEMTIME dtDateTime;
//        memset(&dtDateTime, 0, sizeof(dtDateTime));*/

        // DISPLAYNAME
        const char* szName = NULL;
        if (nameNode)
        {
          szName = nameNode->FirstChild()->Value() ;
        }
        else
        {
          // Something went wrong, the recording has no name
          free(data);
          delete itms;
          return NULL;
        }

        // RECORDED
        if (recordedNode)
        {
          std::string strRecorded = recordedNode->FirstChild()->Value();

          if (strRecorded.size() >= 19)
          {
            /* TODO: FIX THEM DATES */
/*            int iYear, iMonth, iDay;
            iYear = atoi(strRecorded.substr(0, 4).c_str());
            iMonth = atoi(strRecorded.substr(5, 2).c_str());
            iDay = atoi(strRecorded.substr(8, 2).c_str());
            dtDateTime.wYear = iYear;
            dtDateTime.wMonth = iMonth;
            dtDateTime.wDay = iDay;

            int iHour, iMin, iSec;
            iHour = atoi(strRecorded.substr(11, 2).c_str());
            iMin = atoi(strRecorded.substr(14, 2).c_str());
            iSec = atoi(strRecorded.substr(17, 2).c_str());
            dtDateTime.wHour = iHour;
            dtDateTime.wMinute = iMin;
            dtDateTime.wSecond = iSec;*/
          }
        }

        // PATH
        const char* szPath = NULL;
        if (pathNode)
        {
          szPath = pathNode->FirstChild()->Value() ;
        }
        else
        {
          // Something went wrong, the recording has no filename
          free(data);
          delete itms;
          return NULL;
        }

        // SIZE
        // NOTE: Size here is actually just duration in minutes because
        // filesize is not reported by the stripped down GuideParser I use
        if (sizeNode)
        {
          dwFileSize = atol( sizeNode->FirstChild()->Value() );
        }

        // ATTRIB
        // NOTE: Not currently reported in the XML guide data, nor is it particularly
        // needed unless someone wants to add the ability to sub-divide the recordings
        // into categories, as on a real RTV.
        int attrib = 0;
        if (atrbNode)
        {
          attrib = atoi( atrbNode->FirstChild()->Value() );
        }

        bool bIsFolder(false);
        if (attrib & FILE_ATTRIBUTE_DIRECTORY)
          bIsFolder = true;
        
        VFSDirEntry entry;
        entry.label = strdup(szName);
        entry.path = strdup((strRoot+szPath).c_str());
        entry.title = NULL;
        entry.size = dwFileSize*1000;
        entry.folder = bIsFolder;
        entry.properties = new VFSProperty;
        entry.properties->name = strdup("propmisusepreformatted");
        entry.properties->val = strdup("true");
        entry.num_props = 1;
        itms->push_back(entry);
      }

      pChild = pChild->NextSibling();
    }

    free(data);

    // Path given (usually Video), list filenames only
  }
  else
  {

    unsigned char * data;
    char * p, * q;
    unsigned long status;

    // Return a listing of all files in the given path
    status = rtv_list_files(&data, strHostAndPort.c_str(), url->filename);
    if (status == 0)
    {
      delete itms;
      return NULL;
    }

    // Loop through the file list using pointers p and q, where p will point to the current
    // filename and q will point to the next filename
    p = (char *) data;
    while (p)
    {
      // Look for the end of the current line of the file listing
      q = strchr(p, '\n');
      // If found, replace the newline character with the NULL terminator
      if (q)
      {
        *q = '\0';
        // Increment q so that it points to the next filename
        q++;
        // *p should be the current null-terminated filename in the list
        if (*p)
        {
          // Only display MPEG files in XBMC (but not circular.mpg, as that is the RTV
          // video buffer and XBMC may cause problems if it tries to play it)
          if (strstr(p, ".mpg") && !strstr(p, "circular"))
          {
            VFSDirEntry entry;
            entry.label = strdup(p);
            entry.path = strdup((strRoot+p).c_str());
            entry.title = NULL;
            entry.size = -1;
            entry.folder = false;
            entry.properties = new VFSProperty;
            entry.properties->name = strdup("propmisusepreformatted");
            entry.properties->val = strdup("true");
            entry.num_props = 1;
            itms->push_back(entry);
          }
        }
      }
      // Point p to the next filename in the list and loop
      p = q;
    }

    free(data);
  }

  *items = &(*itms)[0];
  *num_items = itms->size();
  return itms;
}

void FreeDirectory(void* items)
{
  std::vector<VFSDirEntry>& ctx = *(std::vector<VFSDirEntry>*)items;
  for (size_t i=0;i<ctx.size();++i)
  {
    free(ctx[i].label);
    for (size_t j=0;j<ctx[i].num_props;++j)
    {
      free(ctx[i].properties[j].name);
      free(ctx[i].properties[j].val);
    }
    delete ctx[i].properties;
    free(ctx[i].path);
  }
  delete &ctx;
}

bool CreateDirectory(VFSURL* url)
{
  return false;
}

bool RemoveDirectory(VFSURL* url)
{
  return false;
}

int Truncate(void* context, int64_t size)
{
  return -1;
}

ssize_t Write(void* context, const void* lpBuf, size_t uiBufSize)
{
  return -1;
}

bool Delete(VFSURL* url)
{
  return false;
}

bool Rename(VFSURL* url, VFSURL* url2)
{
  return false;
}

void* OpenForWrite(VFSURL* url, bool bOverWrite)
{
  return NULL;
}

void* ContainsFiles(VFSURL* url, VFSDirEntry** items, int* num_items, char* rootpath)
{
  return NULL;
}

int GetStartTime(void* ctx)
{
  return 0;
}

int GetTotalTime(void* ctx)
{
  return 0;
}

bool NextChannel(void* context, bool preview)
{
  return false;
}

bool PrevChannel(void* context, bool preview)
{
  return false;
}

bool SelectChannel(void* context, unsigned int channel)
{
  return false;
}

bool UpdateItem(void* context)
{
  return false;
}

int GetChunkSize(void* context)
{
  return 0;
}

}
