#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include <wx/filename.h>
#include <wx/filesys.h>
#include <wx/config.h>

#ifdef APP_OPENNX
# include "opennxApp.h"
#endif
#ifdef APP_TRACELOG
# include "tracelogApp.h"
#endif
#include <memory>

#include "FakeModule.h"

class wxConfigBase;

wxString FakeModule::fileName = wxT("nxssh.ext.sh");

FakeModule::FakeModule() {
    wxString sysDir;
    wxConfigBase::Get()->Read(wxT("Config/SystemNxDir"), &sysDir);
    module.reset(new wxFileName(sysDir, wxEmptyString));
    module->AppendDir(wxT("bin"));
    module->SetName(fileName);
}

FakeModule& FakeModule::instance() {
    static FakeModule instance;
    return instance;
}

bool FakeModule::exists() {
    return module->FileExists();
}
