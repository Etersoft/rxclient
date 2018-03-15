#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include <wx/config.h>

#include "trace.h"
ENABLE_TRACE;

#include "ModuleManager.h"

// ------------------
// list of modules
#include "PCSCModule.h"
// ----------------------------------------------------------------------------
ModuleManager::ModuleManager() {

    // build modules map
    modules["pcsc"] = PCSCModule::create();
}
// ----------------------------------------------------------------------------
ModuleManager& ModuleManager::instance() {
    static ModuleManager instance;
    return instance;
}
// ----------------------------------------------------------------------------
bool ModuleManager::exists( const std::string& name ) const {

    auto it = modules.find(name);
    if( it != modules.end() )
        return it->second->exist();

    return false;
}
// ----------------------------------------------------------------------------
std::shared_ptr<IModule> ModuleManager::getModule( const std::string &name )
{
    auto it = modules.find(name);
    if( it != modules.end() )
        return it->second;

    return nullptr;
}
// ----------------------------------------------------------------------------
wxString ModuleManager::getNxSshCmd( const std::string& name, const MyXmlConfig *cfg ) const
{
    auto it = modules.find(name);
    if( it != modules.end() )
        return it->second->getNxSshCmd(cfg, getDefaultNxSshCmd());

    return getDefaultNxSshCmd();
}
// ----------------------------------------------------------------------------
wxString ModuleManager::getNxSshExtraParam(const MyXmlConfig* cfg ) const
{
    wxString p = wxEmptyString;
    for( const auto& m: modules )
        p << m.second->getNxSshExtraParam(cfg);

    return p;
}
// ----------------------------------------------------------------------------
wxString ModuleManager::getSessionExtraParam(const MyXmlConfig *cfg ) const
{
    wxString p = wxEmptyString;
    for( const auto& m: modules )
        p << m.second->getSessionExtraParam(cfg);

    return p;
}
// ----------------------------------------------------------------------------
wxString ModuleManager::getNxProxyExtraParam( const MyXmlConfig *cfg, const MySession* sess ) const
{
    wxString p = wxEmptyString;
    for( const auto& m: modules )
        p << m.second->getNxProxyExtraParam(cfg, sess);

    return p;
}
// ----------------------------------------------------------------------------
wxString ModuleManager::getDefaultNxSshCmd()
{
#ifdef __WXMSW__
    return wxT("nxssh.exe");
#else
#if wxCHECK_VERSION(3,0,0)
    return wxT("nxssh");
#else
    return wxT("nxssh.sh");
#endif
#endif
}
// ----------------------------------------------------------------------------
void ModuleManager::runBeforeNxSsh( const MyXmlConfig* cfg, const MySession* sess )
{
    for( const auto& m: modules )
    {
        try {
            m.second->runBeforeNxSsh(cfg, sess);
        }
        catch( std::exception& ex ) {
            ::myLogTrace(MYTRACETAG, wxT("(runBeforeNxSsh): catch '%s'"), ex.what());
        }
    }
}
// ----------------------------------------------------------------------------
void ModuleManager::runAfterNxSsh( const MyXmlConfig* cfg, const MySession* sess, int nxsshPID )
{
    for( const auto& m: modules )
    {
        try {
            m.second->runAfterNxSsh(cfg, sess, nxsshPID);
        }
        catch( std::exception& ex ) {
            ::myLogTrace(MYTRACETAG, wxT("(runAfterNxSsh): catch '%s'"), ex.what());
        }
    }
}
// ----------------------------------------------------------------------------
