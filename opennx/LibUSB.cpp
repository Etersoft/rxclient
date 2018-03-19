// This code is based on code of Fritz Elfert (Copyright (C) 2006 The OpenNX Team)
//
// Author: Pavel Vainerman (Etersoft)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this program; if not, write to the
// Free Software Foundation, Inc.,
// 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// ----------------------------------------------------------------------------
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
// ----------------------------------------------------------------------------
#include <iostream>
#include <map>
#include <wx/process.h>
#include <wx/regex.h>
#include <wx/file.h>
#include <wx/textfile.h>
#include <wx/arrstr.h>
using namespace std;
#ifndef USBIDS_FILE
#ifdef __WXMSW__
// \todo FILL FOR WINDOWS
#define USBIDS_FILE "?????"
#else
#define USBIDS_FILE "/usr/share/misc/usb.ids"
//#define USBIDS_FILE wxT("./usb.ids")
#endif
#endif


// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/defs.h"
#endif

#include "LibUSB.h"

#include <wx/wfstream.h>
#include <wx/txtstrm.h>
#include <wx/log.h>
#include <wx/arrimpl.cpp>

#include "trace.h"
ENABLE_TRACE;
// ----------------------------------------------------------------------------
namespace {
    struct USBInfo
    {
        wxString m_sVendorID;
        wxString m_sVendorName;
        wxString m_sDeviceID;
        wxString m_sDeviceName;

        wxString key() const {
            return key(m_sVendorID,m_sDeviceID);
        }

        static wxString key( const wxString& vendorID, const wxString& deviceID ) {
            wxString k;
            return k << vendorID << wxT(":") << deviceID;
        }

        void reset() {
            m_sVendorID = wxT("");
            m_sVendorName = wxT("");
            m_sDeviceID = wxT("");
            m_sDeviceName = wxT("");
        }

        operator bool() const {
            return !m_sVendorID.empty();
        }
    };
}

typedef std::map<wxString, USBInfo> IDSMap;

// devices info (from IDS file). loaded once (!)
static IDSMap dbDeviceInfo;

static void loadIDSFile();
static bool addDeviceInfo( const USBInfo& u );
static USBInfo findDeviceInfo( const wxString& vendorID, const wxString& deviceID );
// ----------------------------------------------------------------------------
WX_DEFINE_OBJARRAY(ArrayOfUSBDevices);
// ----------------------------------------------------------------------------
USBDevice::USBDevice(   const wxString& idBus
                      , const wxString& idUsb
                      , const wxString& sVendor
                      , const wxString& sProduct
                      )
    : m_sVendor(sVendor)
    , m_sProduct(sProduct)
    , m_sBusId(idBus)
    , m_sUsbId(idUsb)
{
}
// ----------------------------------------------------------------------------
wxString USBDevice::toString() {

// #if wxMAJOR_VERSION == 2 && wxMINOR_VERSION == 8
#if wxMAJOR_VERSION < 3
    wxString ret = wxString::Format(wxT("busid %s (%s)"),m_sBusId.c_str(),m_sUsbId.c_str());
#else
    wxString ret = wxString::Format(wxT("busid %s (%s)"),m_sBusId,m_sUsbId);
#endif

    if (!m_sVendor.IsEmpty())
        ret.Append(wxT(" ")).Append(m_sVendor);
    if (!m_sProduct.IsEmpty())
        ret.Append(wxT(" ")).Append(m_sProduct);

    return ret;
}
// ----------------------------------------------------------------------------
wxString USBDevice::toShortString() {
    wxString ret = m_sVendor;
    if ((!ret.IsEmpty()) && (!m_sProduct.IsEmpty()))
        ret.Append(wxT(" "));
    ret.Append(m_sProduct);

    if ( !m_sUsbId.IsEmpty())
        ret << wxT("(") << m_sUsbId << wxT(")");

    return ret;
}
// ----------------------------------------------------------------------------
USB::USB() {
    m_bAvailable = false;
    wxLogNull noerrors;
    loadDevicesFromUSBIP();
}
// ----------------------------------------------------------------------------
ArrayOfUSBDevices USB::GetDevices() {
    return m_aDevices;
}
// ----------------------------------------------------------------------------
bool USB::IsAvailable() {
    ::myLogTrace(MYTRACETAG, wxT("USB::IsAvailable() = %s"), m_bAvailable ? wxT("true") : wxT("false"));
    return m_bAvailable;
}
// ----------------------------------------------------------------------------
wxString USB::GetUSBIPPath()
{
#ifdef __WXMSW__
    // \todo It has not been tested under windows yet!
    return wxT("usbip.exe")
#else
    wxString cmd;

    // \todo It is necessary to add a path to PATH, and here to use simply 'usbip'
    return cmd << wxFileName::GetPathSeparator() << wxT("usr")
               << wxFileName::GetPathSeparator() << wxT("sbin")
               << wxFileName::GetPathSeparator() << wxT("usbip");
#endif
}
// ----------------------------------------------------------------------------
void USB::loadDevicesFromUSBIP()
{
    // \todo Сделать проверку на наличие usbip
    m_bAvailable = true;
    wxArrayString out;
    wxString cmd;
    cmd << GetUSBIPPath() << wxT(" list --parsable --local");

    long ret = wxExecute(cmd, out);
    if( ret == -1 )
    {
        m_bAvailable = false;

// #if wxMAJOR_VERSION == 2 && wxMINOR_VERSION == 8
#if wxMAJOR_VERSION < 3
        ::myLogTrace(MYTRACETAG,wxT("(loadDevicesFromUSPIP): Error run command '%s'\n"),cmd.c_str());
#else
        ::myLogTrace(MYTRACETAG,wxT("(loadDevicesFromUSPIP): Error run command '%s'\n"),cmd);
#endif
        return;
    }

    wxRegEx re;
    bool reOk = re.Compile(wxT("^busid=(.*)#usbid=([0-9abcdef]{4}):([0-9abcdef]{4})#"));
    if( !reOk )
    {
        m_bAvailable = false;
        ::myLogTrace(MYTRACETAG, wxT("(loadDevicesFromUSPIP): Error compile regexp..'\n"));
        return;
    }

    for( int i=0; i<out.size(); i++ )
    {
        if( re.Matches(out[i]) )
        {
            wxString sBusId( re.GetMatch(out[i],1) );
            wxString sVendorId( re.GetMatch(out[i],2) );
            wxString sDeviceId( re.GetMatch(out[i],3) );
            wxString sUsbId;
            sUsbId << sVendorId << wxT(":") << sDeviceId;

            USBInfo u = findDeviceInfo(sVendorId,sDeviceId);
            if( !u ) {
                u.m_sVendorID = sVendorId;
                u.m_sDeviceID = sDeviceId;
                u.m_sDeviceName << wxT("unknown product (") << sUsbId << wxT(")");
                u.m_sVendorName << wxT("unknown vendor (") << sUsbId << wxT(")");
            }

            USBDevice d( sBusId
                         , sUsbId
                         , u.m_sVendorName
                         , u.m_sDeviceName );

            m_aDevices.push_back(d);
        }
    }
}
// ----------------------------------------------------------------------------
static void loadIDSFile()
{
    wxTextFile ifile( wxT(USBIDS_FILE) );
    if( !ifile.Exists() || !ifile.Open() )
    {
        ::myLogTrace(MYTRACETAG, wxT("(loadIDSFile): WARNING can't load file %s..\n"), USBIDS_FILE);
        return;
    }
// -------------------------------------
// IDS FILE
// -------------------------------------
// # Syntax:
// # vendor  vendor_name
// #       device  device_name                             <-- single tab
// #               interface  interface_name               <-- two tabs

// 03e7  Intel
//         2150  Myriad VPU [Movidius Neural Compute Stick]
// 03e8  EndPoints, Inc.
//         0004  SE401 Webcam
//         0008  101 Ethernet [klsi]
//         0015  ATAPI Enclosure

    wxRegEx reVendor;
    wxRegEx reDevice;
    if( !reVendor.Compile(wxT("^([0-9abcdef]{4})\\ \\ (.*)$")) )
    {
        ::myLogTrace(MYTRACETAG, wxT("(loadIDSFile): WARNING: compile 'vendor' regexp error..\n"));
        return;
    }

    if( !reDevice.Compile(wxT("^\t([0-9abcdef]{4})\\ \\ (.*)$")) )
    {
        ::myLogTrace(MYTRACETAG, wxT("(loadIDSFile): WARNING: compile 'device' regexp error..\n"));
        return;
    }

    wxString line;
    USBInfo usb;

    for (line = ifile.GetFirstLine(); !ifile.Eof(); line = ifile.GetNextLine()) {

        if( reVendor.Matches(line) )
        {
            // clean previous
            if( !usb.m_sVendorID.empty() )
                usb.reset();

            usb.m_sVendorID = reVendor.GetMatch(line,1);
            usb.m_sVendorName = reVendor.GetMatch(line,2);
            addDeviceInfo(usb);
        }
        else if( reDevice.Matches(line) )
        {
            if( !usb.m_sVendorID.empty() )
            {
                // add the previous
                if( !usb.m_sDeviceID.empty() )
                    addDeviceInfo(usb);

                usb.m_sDeviceID = reDevice.GetMatch(line,1);
                usb.m_sDeviceName = reDevice.GetMatch(line,2);
            }
        }
    }
}
// ----------------------------------------------------------------------------
static bool addDeviceInfo( const USBInfo& u )
{
    if( u.m_sVendorID.empty() )
        return false;

    dbDeviceInfo[u.key()] = u;
}
// ----------------------------------------------------------------------------
static USBInfo findDeviceInfo( const wxString &vendorID, const wxString &deviceID )
{
    if( dbDeviceInfo.empty() )
        loadIDSFile();

    IDSMap::const_iterator i = dbDeviceInfo.find( USBInfo::key(vendorID,deviceID));
    // find with vendorID and deviceID
    if( i != dbDeviceInfo.end() )
        return i->second;

    // find with vendorID
    i = dbDeviceInfo.find( USBInfo::key(vendorID,wxT("")));
    if( i != dbDeviceInfo.end() )
        return i->second;

    return USBInfo();
}
// ----------------------------------------------------------------------------
