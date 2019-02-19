#ifndef _MODULEMANAGER_H_
#define _MODULEMANAGER_H_
#include <memory>
#include <unordered_map>
#include <wx/string.h>
#include "IModule.h"

class MyXmlConfig;
class MySession;

class ModuleManager {
private:
    ModuleManager();

    typedef std::unordered_map< std::string,std::shared_ptr<IModule> > ModulesMap;
    ModulesMap modules;

    ModuleManager(ModuleManager const&) {} //delete
public:

    static ModuleManager& instance();
    bool exists( const std::string& moduleName ) const;
    std::string modulesInfo() const;

    std::shared_ptr<IModule> getModule( const std::string& moduleName );

    // config
    static wxString getDefaultNxSshCmd();
    wxString getNxSshCmd( const std::string& moduleName, const MyXmlConfig* cfg ) const;

    wxString getNxSshExtraParam( const MyXmlConfig* cfg ) const;

    // format: ' --param1=val1 --param2=val2  ...'
    wxString getSessionExtraParam( const MyXmlConfig* cfg, const MySession* sess ) const;

    // format: ,param1=val1,param2=val2,...
    wxString getNxProxyExtraParam( const MyXmlConfig* cfg, const MySession* sess ) const;

    // -----------------------------------------
    // HOOKS (call for all registered modules)
    // -----------------------------------------
    void runBeforeNxSsh( const MyXmlConfig* cfg, const MySession* sess );
    void runAfterNxSsh( const MyXmlConfig* cfg, const MySession* sess, int nxsshPID );
};
#endif
