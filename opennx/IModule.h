#ifndef _IMODULE_H_
#define _IMODULE_H_
#include <wx/string.h>

class MyXmlConfig;

class IModule
{
private:
    wxString name;

public:
    IModule( const wxString& _name )
        :name(_name)
    {}
    
    virtual ~IModule(){}

    virtual bool exist() = 0;

    // get module name
    wxString getName() const { return name; }

    // get command for run (exec file)
    // see details in https://bugs.etersoft.ru/..
    virtual wxString getNxSshCmd( const MyXmlConfig* cfg, const wxString& defaultCmd ) const
    {
        return defaultCmd;
    }

    virtual wxString getNxSshExtraParam( const MyXmlConfig* cfg ) const
    {
        return wxEmptyString;
    }

    // format: ' --param1=val1 --param2=val2  ...'
    virtual wxString getSessionExtraParam( const MyXmlConfig* cfg ) const
    {
        return wxEmptyString;
    }

    // format: ,param1=val1,param2=val2,...
    virtual wxString getNxProxyExtraParam( const MyXmlConfig* cfg ) const
    {
        return wxEmptyString;
    }

    // HOOKS
    virtual void runBeforeNxSsh( const MyXmlConfig* cfg ){}
    virtual void runAfterNxSsh( const MyXmlConfig* cfg, int nxsshPID ){}
};

#endif
