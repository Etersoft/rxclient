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
wxString ModuleManager::getNxSshCmd( const std::string& name ) const
{
    auto it = modules.find(name);
    if( it != modules.end() )
        return it->second->getNxSshCmd(getDefaultNxSshCmd());

    return getDefaultNxSshCmd();
}
// ----------------------------------------------------------------------------
wxString ModuleManager::getNxSshExtraParam( const std::string &name, const MyXmlConfig* cfg ) const
{
    auto it = modules.find(name);
    if( it != modules.end() )
        return it->second->getNxSshExtraParam(cfg);

    return wxEmptyString;
}
// ----------------------------------------------------------------------------
wxString ModuleManager::getSessionExtraParam( const std::string &name, const MyXmlConfig *cfg ) const
{
    auto it = modules.find(name);
    if( it != modules.end() )
        return it->second->getSessionExtraParam(cfg);

    return wxEmptyString;
}
// ----------------------------------------------------------------------------
wxString ModuleManager::getNxProxyExtraParam(const std::string &name, const MyXmlConfig *cfg) const
{
    auto it = modules.find(name);
    if( it != modules.end() )
        return it->second->getNxProxyExtraParam(cfg);

    return wxEmptyString;
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
void ModuleManager::runBeforeNxSsh( const MyXmlConfig* cfg )
{
    for( const auto& m: modules )
    {
        try {
            m.second->runBeforeNxSsh(cfg);
        }
        catch( std::exception& ex ) {
            ::myLogTrace(MYTRACETAG, wxT("(runBeforeNxSsh): catch '%s'"), ex.what());
        }
    }
}
// ----------------------------------------------------------------------------
void ModuleManager::runAfterNxSsh( const MyXmlConfig* cfg, int nxsshPID )
{
    for( const auto& m: modules )
    {
        try {
            m.second->runAfterNxSsh(cfg, nxsshPID);
        }
        catch( std::exception& ex ) {
            ::myLogTrace(MYTRACETAG, wxT("(runAfterNxSsh): catch '%s'"), ex.what());
        }
    }
}
// ----------------------------------------------------------------------------
