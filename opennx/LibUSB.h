// This code is based on code of Fritz Elfert (Copyright (C) 2006 The OpenNX Team)
//
// Author: Pavel Vainerman (Etersoft)
//
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

#ifndef _LIBUSB_H_
#define _LIBUSB_H_

#include <wx/dynarray.h>
#include <wx/string.h>
#include <wx/object.h>
#include <memory>
// ----------------------------------------------------------------------------
class USB;

class USBDevice : public wxObject {
    public:
        USBDevice(   const wxString& iBusId
                   , const wxString& iUsbId
                   , const wxString& sVendor
                   , const wxString& sProduct );

        wxString toString();
        wxString toShortString();

        const wxString GetVendor() const { return m_sVendor; }
        const wxString GetProduct() const { return m_sProduct; }
        const wxString GetBusID() const { return m_sBusId; }
        const wxString GetUsbID() const { return m_sUsbId; }

    private:
        const wxString m_sVendor;
        const wxString m_sProduct;

        const wxString m_sBusId;
        const wxString m_sUsbId;

        friend class USB;
};
// ----------------------------------------------------------------------------
WX_DECLARE_OBJARRAY(USBDevice, ArrayOfUSBDevices);

class USB {
    public:
        USB();
        bool IsAvailable();
        ArrayOfUSBDevices GetDevices();

    private:
        void loadDevicesFromUSBIP();
        wxString GetUSBIPPath();

        ArrayOfUSBDevices m_aDevices;
        bool m_bAvailable;
};
// ----------------------------------------------------------------------------
#endif
// _LIBUSB_H_
