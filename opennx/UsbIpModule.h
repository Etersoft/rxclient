#ifndef _UsbIpModule_H_
#define _UsbIpModule_H_

#include <memory>
#include <wx/string.h>
#include <wx/filename.h>
#include "IModule.h"

class MyXmlConfig;
class MySession;

//
// Module for support usb forwarding
//
class UsbIpModule:
        public IModule
{
private:

    std::shared_ptr<wxFileName>  usbip;
    std::shared_ptr<wxFileName>  prunner;

public:
    UsbIpModule();

    static std::shared_ptr<IModule> create();

    virtual bool exist() override;
    virtual wxString getSessionExtraParam( const MyXmlConfig* cfg, const MySession* sess ) const override;
    virtual wxString getNxProxyExtraParam( const MyXmlConfig* cfg, const MySession* sess ) const override;
    virtual void runAfterNxSsh( const MyXmlConfig* cfg, const MySession* sess, int nxsshPID ) override;
};
#endif
