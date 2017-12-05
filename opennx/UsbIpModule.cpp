#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include <wx/filesys.h>
#include <wx/config.h>

#include "trace.h"
ENABLE_TRACE;

#include "MyXmlConfig.h"
#include "MySession.h"
#include "UsbIpModule.h"

class wxConfigBase;
// ----------------------------------------------------------------------------
static long SupportedNXProtocolVersion = 197890; /* 3.5.2 */
// ----------------------------------------------------------------------------
std::shared_ptr<IModule> UsbIpModule::create()
{
    auto usbip = std::make_shared<UsbIpModule>();
    return std::static_pointer_cast<IModule>(usbip);
}
// ----------------------------------------------------------------------------
UsbIpModule::UsbIpModule()
    :IModule(wxT("usbip"))
{
    wxString sysDir;
    wxConfigBase::Get()->Read(wxT("Config/SystemNxDir"), &sysDir);

    usbip = std::make_shared<wxFileName>(sysDir, wxEmptyString);
    usbip->AppendDir(wxT("bin"));
    usbip->SetName(wxT("nx-usbip-helper"));

    prunner = std::make_shared<wxFileName>(sysDir, wxEmptyString);
    prunner->AppendDir(wxT("bin"));
    prunner->SetName(wxT("prunner"));
}

// ----------------------------------------------------------------------------
bool UsbIpModule::exist() {
    return usbip->FileExists();
}
// ----------------------------------------------------------------------------
wxString UsbIpModule::getSessionExtraParam( const MyXmlConfig *pCfg, const MySession* pSess ) const
{
    if( !pCfg->bGetEnableUSBIP() || !usbip->FileExists() )
        return IModule::getSessionExtraParam(pCfg,pSess);

    if( pSess->lGetProtocolVersion() < SupportedNXProtocolVersion /* 3.5.2 */ )
    {
        ::myLogTrace(MYTRACETAG,
                     wxT("(usbip): ProtocolVersion='%s' not supported. Must be >= 3.5.2"),
                     to_c_str(pSess->sGetProtocolVersion() ));

        return IModule::getSessionExtraParam(pCfg,pSess);
    }

    wxString p;
    // format: ' --param1=val1 --param2=val2  ...'
    p << wxT(" --usbip=\"") << pCfg->iGetUsbIpPort() << wxT("\"");
    p << wxT(" --usbipdev=\"") << pCfg->sGetUsbIpDevices() << wxT("\"");

    return p;
}
// ----------------------------------------------------------------------------
wxString UsbIpModule::getNxProxyExtraParam( const MyXmlConfig* pCfg, const MySession* pSess ) const
{
    if( !pCfg->bGetEnableUSBIP() || !usbip->FileExists() )
        return IModule::getNxProxyExtraParam(pCfg, pSess);

    if( pSess->lGetProtocolVersion() < SupportedNXProtocolVersion /* 3.5.2 */ )
    {
        ::myLogTrace(MYTRACETAG,
                     wxT("(usbip): ProtocolVersion='%s' not supported. Must be >= 3.5.2"),
                     to_c_str(pSess->sGetProtocolVersion() ));

        return IModule::getNxProxyExtraParam(pCfg,pSess);
    }
    // format: ,param1=val1,param2=val2,...
    wxString p;
    p << wxT(",usbip=") << pCfg->iGetUsbIpPort();
    return p;
}
// ----------------------------------------------------------------------------
void UsbIpModule::runAfterNxSsh( const MyXmlConfig* pCfg, const MySession *pSess, int nxsshPID )
{
    if( !pCfg->bGetEnableUSBIP() || !usbip->FileExists() )
        return;

    if( pSess->lGetProtocolVersion() < SupportedNXProtocolVersion /* 3.5.2 */ )
    {
        ::myLogTrace(MYTRACETAG,
                     wxT("(usbip): ProtocolVersion='%s' not supported. Must be >= 3.5.2"),
                     to_c_str(pSess->sGetProtocolVersion() ));
        return;
    }

    wxString cmdBind = usbip->GetShortPath();
    cmdBind << wxT(" --bind '") << pCfg->sGetUsbIpDevices() << wxT("'")
            << wxT(" --port ") << pCfg->iGetUsbIpPort();

    if( prunner->FileExists() ) {

        wxString cmdUnbind = usbip->GetShortPath();
        cmdUnbind << wxT(" --unbind '") << pCfg->sGetUsbIpDevices() << wxT("'")
                  << wxT(" --port ") << pCfg->iGetUsbIpPort();

        wxString cmd = prunner->GetShortPath();
        cmd << wxT(" --monitor-pid ") << nxsshPID
            << wxT(" --run \"") << cmdBind << wxT("\"")
            << wxT(" --run-after \"") << cmdUnbind << wxT("\"");

        ::myLogTrace(MYTRACETAG, wxT("(usbip): executing %s"), to_c_str(cmd));
        wxExecute(cmd);

    } else {

        ::myLogTrace(MYTRACETAG, wxT("(usbip): executing %s"), to_c_str(cmdBind));
        wxExecute(cmdBind);
    }
}
// ----------------------------------------------------------------------------
