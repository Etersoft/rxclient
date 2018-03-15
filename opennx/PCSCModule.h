#ifndef _PCSCModule_H_
#define _PCSCModule_H_

#include <memory>
#include <wx/string.h>
#include <wx/filename.h>
#include "IModule.h"

class MyXmlConfig;
class MySession;

//
// Module for support pcscd socket forwarding
//
class PCSCModule:
        public IModule
{
private:

    std::shared_ptr<wxFileName>  smartcard;
    std::shared_ptr<wxFileName>  pcsc;

public:
    PCSCModule();

    static std::shared_ptr<IModule> create();

    virtual bool exist() override;
    virtual wxString getNxSshCmd( const MyXmlConfig* cfg, const wxString& defaultName ) const override;
    virtual wxString getNxSshExtraParam( const MyXmlConfig* cfg ) const override;
    virtual wxString getSessionExtraParam( const MyXmlConfig* cfg ) const override;
    virtual wxString getNxProxyExtraParam( const MyXmlConfig* cfg, const MySession* sess ) const override;
    virtual void runAfterNxSsh( const MyXmlConfig* cfg, const MySession* sess, int nxsshPID ) override;
};
#endif
