#ifndef _MODULEMANAGER_H_
#define _MODULEMANAGER_H_
#include <memory>
#include <unordered_map>
#include <wx/string.h>
#include "IModule.h"

class MyXmlConfig;

class ModuleManager {
private:
    ModuleManager();

    typedef std::unordered_map< std::string,std::shared_ptr<IModule> > ModulesMap;
    ModulesMap modules;

    ModuleManager(ModuleManager const&) {} //delete
    ModuleManager& operator= (ModuleManager const&) {} //delete
public:

    static ModuleManager& instance();
    bool exists( const std::string& moduleName ) const;

    std::shared_ptr<IModule> getModule( const std::string& moduleName );

    // config
    static wxString getDefaultNxSshCmd();
    wxString getNxSshCmd( const std::string& moduleName, const MyXmlConfig* cfg ) const;

    wxString getNxSshExtraParam( const MyXmlConfig* cfg ) const;

    // format: ' --param1=val1 --param2=val2  ...'
    wxString getSessionExtraParam( const MyXmlConfig* cfg ) const;

    // format: ,param1=val1,param2=val2,...
    wxString getNxProxyExtraParam( const MyXmlConfig* cfg ) const;

    // -----------------------------------------
    // HOOKS (call for all registered modules)
    // -----------------------------------------
    void runBeforeNxSsh( const MyXmlConfig* cfg );
    void runAfterNxSsh( const MyXmlConfig* cfg, int nxsshPID );
};
#endif
