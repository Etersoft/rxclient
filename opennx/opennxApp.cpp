// $Id: opennxApp.cpp 713 2012-07-02 15:56:54Z felfert $
//
// Copyright (C) 2006 The OpenNX Team
// Author: Fritz Elfert
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

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif
#ifdef __WXMSW__
#include <shlobj.h>
#endif

#ifdef __UNIX__
#include <unistd.h>
#endif
#include <wx/cmdline.h>
#include <wx/xrc/xmlres.h>
#include <wx/image.h>
#include <wx/config.h>
#include <wx/fs_zip.h>
#include "wx/fs_mem.h"
#include <wx/sysopt.h>
#include <wx/tokenzr.h>
#include <wx/wfstream.h>
#include <wx/mimetype.h>
#include <wx/utils.h>
#include <wx/stdpaths.h>
#include <wx/apptrait.h>
#include <wx/socket.h>
#include <wx/regex.h>
#include <wx/dir.h>
#include <wx/cshelp.h>

#include "opennxApp.h"
#include "SessionAdmin.h"
#include "LoginDialog.h"
#include "LibOpenSC.h"
#include "MyWizard.h"
#include "MyIPC.h"
#include "MyXmlConfig.h"
#include "PanicDialog.h"
#include "ResumeDialog.h"
#include "QuitDialog.h"
#include "ForeignFrame.h"
#include "Icon.h"
#include "LibUSB.h"
#include "LibOpenSC.h"
#include "osdep.h"
#include "xh_richtext.h"
#include "CardWaiterDialog.h"
#include "SupressibleMessageDialog.h"
#include "ModuleManager.h"

#include "memres.h"

#include "trace.h"
ENABLE_TRACE;
DECLARE_TRACETAGS;

// Create a new application object: this macro will allow wxWindows to create
// the application object during program execution (it's better than using a
// static object for many reasons) and also declares the accessor function
// wxGetApp() which will return the reference of the right type (i.e. opennxApp and
// not wxApp)

IMPLEMENT_APP(opennxApp);

    opennxApp::opennxApp()
    : wxApp()
    ,m_pCfg(NULL)
    ,m_pSessionCfg(NULL)
    ,m_nPcsdPort(-1)
    ,m_nNxSshPID(-1)
    ,m_iReader(-1)
    ,m_bNxSmartCardSupport(false)
    ,m_bRunproc(false)
    ,m_bRequireWatchReader(false)
    ,m_bRequireStartUsbIp(false)
    ,m_bTestCardWaiter(false)
    ,m_bAutoLogin(false)
    ,m_bConfigReadOnly(false)
    ,m_bAutoResume(false)
    ,m_bKillErrors(false)
    ,m_bSilent(false)
    ,m_bNoProgressBar(false)
    ,m_bNoGui(false)
    ,m_pLoginDialog(NULL)
{
    SetAppName(wxT("RX Client"));
#ifdef __WXMSW__
    m_pCfg = new wxConfig(wxT("RX Client"), wxT("Etersoft"));
#else
# ifdef __WXMAC__
    m_pCfg = new wxConfig(wxT("RX Client"), wxT("Etersoft"), wxT("RX Client Preferences"), wxT("RX Client Preferences"));
# else
    m_pCfg = new wxConfig(wxT("RX Client"), wxT("Etersoft"), wxT(".opennx"), wxT("opennx.conf"));
# endif 
#endif
    wxConfigBase::Set(m_pCfg);

#ifdef __WXMSW__
    DWORD dummy;
    DWORD viSize;
    LPVOID vi;
    TCHAR mySelf[MAX_PATH];
    VS_FIXEDFILEINFO *vsFFI;
    UINT vsFFIlen;
    m_sVersion = wxT("?.?");

    if (GetModuleFileName(NULL, mySelf, sizeof(mySelf))) {
        viSize = GetFileVersionInfoSize(mySelf, &dummy);
        if (viSize) {
            vi = (LPVOID)malloc(viSize);
            if (vi) {
                if (GetFileVersionInfo(mySelf, dummy, viSize, vi)) {
                    if (VerQueryValueA(vi, (LPSTR)"\\", (LPVOID *)&vsFFI, &vsFFIlen)) {
                        m_sVersion = wxString::Format(wxT("%d.%d"), HIWORD(vsFFI->dwFileVersionMS),
                                LOWORD(vsFFI->dwFileVersionMS));
                        if (vsFFI->dwFileVersionLS)
                            m_sVersion += wxString::Format(wxT(".%d"), HIWORD(vsFFI->dwFileVersionLS));
                        if (LOWORD(vsFFI->dwFileVersionLS))
                            m_sVersion += wxString::Format(wxT(".%d"), LOWORD(vsFFI->dwFileVersionLS));
                    }

                }
                free(vi);
            }
        }
    }
#else
    m_sVersion = wxT(PACKAGE_VERSION);
    while (m_sVersion.Freq(wxT('.')) < 2)
        m_sVersion.Append(wxT(".0"));
    m_sVersion.Append(wxT(".")).Append(wxT(SVNREV));

    // Language overrides from KDE - only applied if running inside a KDE session. 
    if (inKdeSession != 0) {
        wxLogNull dummy;

        // If KDE_LANG is set, then it has precedence over kdeglobals.
        wxString lang;
        if (wxGetEnv(wxT("KDE_LANG"), &lang)) {
            myLogDebug(wxT("Overriding LANG from KDE_LANG environment to: '%s'"), to_c_str(lang));
            wxSetEnv(wxT("LANG"), lang);
        } else {
            // Try to get KDE language settings and override locale accordingly
            wxFileInputStream fis(wxGetHomeDir() +
                    wxFileName::GetPathSeparator() + wxT(".kde") + 
                    wxFileName::GetPathSeparator() + wxT("share") + 
                    wxFileName::GetPathSeparator() + wxT("config") + 
                    wxFileName::GetPathSeparator() + wxT("kdeglobals"));
            if (fis.IsOk()) {
                wxFileConfig cfg(fis);
                wxString country = cfg.Read(wxT("Locale/Country"), wxEmptyString);
                wxString lang = cfg.Read(wxT("Locale/Language"), wxEmptyString);
                if ((!lang.IsEmpty()) && (!country.IsEmpty())) {
                    if (lang.Contains(wxT(":")))
                        lang = lang.BeforeFirst(wxT(':'));
                    if (lang.Length() < 3)
                        lang << wxT("_") << country.Upper();
                    lang << wxT(".UTF-8");
                    myLogDebug(wxT("Overriding LANG from kdeglobals to: '%s'"), to_c_str(lang));
                    wxSetEnv(wxT("LANG"), lang);
                }
            }
        }
    }
#endif
}

opennxApp::~opennxApp()
{
    if (m_pCfg)
        delete m_pCfg;
    if (m_pSessionCfg)
        delete m_pSessionCfg;
}

void
opennxApp::EnableContextHelp(wxWindow *w)
{
    if (NULL == w)
        return;
    wxAcceleratorEntry entries[1];
    entries[0].Set(wxACCEL_SHIFT, WXK_F1, wxID_CONTEXT_HELP);
    wxAcceleratorTable accel(1, entries);
    w->SetAcceleratorTable(accel);
    w->SetFocus();
}

    wxString
opennxApp::LoadFileFromResource(const wxString &loc, bool bUseLocale /* = true */)
{
    bool tryloop = true;
    wxString ret;
    wxString cloc = bUseLocale ? m_cLocale.GetCanonicalName() : wxT("");
    wxFileSystem fs;
    wxFSFile *f;

    do {
        wxString tryloc = loc;

        if (!cloc.IsEmpty()) {
            tryloc = wxFileName(loc).GetPath(wxPATH_GET_SEPARATOR|wxPATH_GET_VOLUME)
                + cloc + wxT("_") + wxFileName(loc).GetFullName();
            cloc = cloc.BeforeLast(wxT('_'));
        } else
            tryloop = false;

        // try plain loc first
        f = fs.OpenFile(tryloc);

        if (!f)
            f = fs.OpenFile(GetResourcePrefix() + tryloc);

        if (f) {
            wxInputStream *is = f->GetStream();
            size_t size = is->GetSize();
            char *buf = new char[size+2];
            is->Read(buf, size);
            delete f;
            buf[size] = buf[size+1] = 0;
            ret = wxConvLocal.cMB2WX(buf);
            delete []buf;
        }
    } while (ret.IsEmpty() && tryloop);
    return ret;
}

#if (!(defined(__WXMSW__) || defined(__WXMAC__)))
static const wxChar *desktopDirs[] = {
    wxT("Desktop"), wxT("KDesktop"), wxT(".gnome-desktop"), NULL
};
#endif

    bool
opennxApp::CreateDesktopEntry(MyXmlConfig *cfg)
{
    bool ret = false;

    wxString appDir;
    wxConfigBase::Get()->Read(wxT("Config/SystemNxDir"), &appDir);
#ifdef __WXMSW__
    TCHAR dtPath[MAX_PATH];
    if (SHGetSpecialFolderPath(NULL, dtPath, CSIDL_DESKTOPDIRECTORY, FALSE)) {
        wxString linkPath = wxString::Format(wxT("%s\\%s.lnk"), dtPath, cfg->sGetName().c_str());
        wxString targetPath = GetSelfPath();
        wxString desc = _("Launch NX Session");
        wxString args = wxString::Format(wxT("--session=\"%s\""), cfg->sGetFileName().c_str());
        HRESULT hres;
        IShellLink* psl;

        hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                IID_IShellLink, (LPVOID *) &psl);
        if (SUCCEEDED(hres)) {
            IPersistFile* ppf;
            psl->SetPath(targetPath.c_str());
            psl->SetWorkingDirectory(appDir.c_str());
            psl->SetDescription(desc.c_str());
            psl->SetArguments(args.c_str());
            psl->SetIconLocation(targetPath, 1);
            hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);
            if (SUCCEEDED(hres)) {
                hres = ppf->Save(wxConvLocal.cWX2WC(linkPath), TRUE);
                ppf->Release();
                ret = true;
            }
            psl->Release();
        }
    }
#endif
#ifdef __UNIX__
# ifdef __WXMAC__
    wxFileName fn(cfg->sGetFileName());
    fn.MakeAbsolute();
    wxString src = fn.GetFullPath();
    wxString dst = wxGetHomeDir() + wxT("/Desktop/") + cfg->sGetName();
    ret = (0 == (symlink(src.fn_str(), dst.fn_str())));
# else
    wxString dtEntry = wxT("[Desktop Entry]\n");
    dtEntry += wxT("Encoding=UTF-8\n");
    dtEntry += wxT("Type=Application\n");
    dtEntry += wxT("MimeType=\n");
    dtEntry += wxT("StartupNotify=true\n");
    dtEntry += wxT("Terminal=false\n");
    dtEntry += wxT("TerminalOptions=\n");
    dtEntry += wxT("Comment=Launch RX Session\n");
    dtEntry += wxT("Comment[de]=Starte RX Sitzung\n");
    dtEntry += wxT("Name=") + cfg->sGetName() + wxT("\n");
    dtEntry += wxT("GenericName=RX Client\n");
    dtEntry += wxT("GenericName[de]=RX Client\n");
    dtEntry += wxT("Exec=\"") + GetSelfPath() + wxT("\" --session=\"")
        + cfg->sGetFileName() + wxT("\"\n");
    dtEntry += wxT("Icon=rx-desktop\n");

    const wxChar **p = desktopDirs;
    while (*p) {
        wxString path = wxGetHomeDir() + wxT("/") + *p;
        if (wxDirExists(path)) {
            wxFile f;
            wxString fn = path + wxT("/") + cfg->sGetName() + wxT(".desktop");
            ::myLogTrace(MYTRACETAG, wxT("Creating '%s'"), to_c_str(fn));
            if (f.Create(fn, true,
                        wxS_IRUSR|wxS_IWUSR|wxS_IXUSR|wxS_IRGRP|wxS_IROTH)) {
                f.Write(dtEntry);
                f.Close();
                ret = true;
            }
        }
        p++;
    }
# endif // !__WXMAC__
#endif // __UNIX__
    if (!ret)
        wxLogError(_("Could not create link on Desktop."));
    return ret;
}

    bool
opennxApp::RemoveDesktopEntry(MyXmlConfig *cfg)
{
#ifdef __WXMSW__
    TCHAR dtPath[MAX_PATH];
    if (SHGetSpecialFolderPath(NULL, dtPath, CSIDL_DESKTOPDIRECTORY, FALSE)) {
        wxString lpath = wxString::Format(wxT("%s\\%s.lnk"),
                dtPath, cfg->sGetName().c_str());
        ::myLogTrace(MYTRACETAG, wxT("Removing '%s'"), to_c_str(lpath));
        wxRemoveFile(lpath);
    }
#endif
#ifdef __UNIX__
# ifdef __WXMAC__
    wxString fn = wxGetHomeDir() + wxT("/Desktop/") + cfg->sGetName();
    wxRemoveFile(fn);
# else
    const wxChar **p = desktopDirs;

    while (*p) {
        wxRemoveFile(wxString::Format(wxT("%s/%s/%s.desktop"),
                    wxGetHomeDir().c_str(), *p,cfg->sGetName().c_str()));
        p++;
    }
# endif
#endif
    return true;
}

    bool
opennxApp::CheckDesktopEntry(MyXmlConfig *cfg)
{
    bool ret = false;
#ifdef __WXMSW__
    TCHAR dtPath[MAX_PATH];
    if (SHGetSpecialFolderPath(NULL, dtPath, CSIDL_DESKTOPDIRECTORY, FALSE)) {
        wxString lpath = wxString::Format(wxT("%s\\%s.lnk"),
                dtPath, cfg->sGetName().c_str());
        return wxFileName::FileExists(lpath);
    }
#endif
#ifdef __UNIX__
# ifdef __WXMAC__
    wxString fn = wxGetHomeDir() + wxT("/Desktop/") + cfg->sGetName();
    return wxFileName::FileExists(fn);
# else
    const wxChar **p = desktopDirs;

    while (*p) {
        ret |= wxFileName::FileExists((wxString::Format(wxT("%s/%s/%s.desktop"),
                    wxGetHomeDir().c_str(), *p,cfg->sGetName().c_str())));
        p++;
    }
# endif
#endif
    return ret;
}

    void
opennxApp::setUserDir()
{
    wxString tmp;
    //sergeym
    //if (!wxConfigBase::Get()->Read(wxT("Config/UserNxDir"), &tmp))
    tmp = wxGetHomeDir() + wxFileName::GetPathSeparator() + wxT(".nx");
    wxFileName::Mkdir(tmp, 0750, wxPATH_MKDIR_FULL);
    wxFileName fn(tmp);
    wxConfigBase::Get()->Write(wxT("Config/UserNxDir"), fn.GetFullPath());
    wxFileName::Mkdir(tmp +  wxFileName::GetPathSeparator() + wxT("config"), 0750,
            wxPATH_MKDIR_FULL);
}

    bool
opennxApp::setSelfPath()
{
    wxString tmp;
    wxFileName fn;
#define NotImplemented
#if defined(__WXMSW__) || defined(__WXMAC__) || defined(__LINUX__) || defined(__FREEBSD__)
    // On these platforms, wxAppTraits::GetExecutablePath() returns
    // the *actual* path of the running executable, regardless of
    // where it is installed.
    fn.Assign(GetTraits()->GetStandardPaths().GetExecutablePath());
#undef NotImplemented
#endif
#if defined(__OPENBSD__) 
    // FIXME: How to get one's own exe path on OpenBSD?
    // for now, we resemble sh's actions
    tmp = this->argv[0];
    if (!wxIsAbsolutePath(tmp)) {
        if (tmp.StartsWith(wxT("."))) {
            // a relative path
            fn.Assign(tmp);
            fn.MakeAbsolute();
            tmp = fn.GetFullPath();
        } else {
            bool found = false;
            wxGetEnv(wxT("PATH"), &tmp);
            if (tmp.IsEmpty()) {
                wxLogError(_("Could not get PATH environment"));
                return false;
            }
            wxStringTokenizer st(tmp, wxT(":"));
            while (st.HasMoreTokens()) {
                tmp = st.GetNextToken();
                fn.Assign(tmp, argv[0]);
                if (fn.IsFileExecutable()) {
                    tmp = fn.GetFullPath();
                    found = true;
                    break;
                }
            }
            if (!found) {
                tmp = argv[0];
                wxLogError(_("Could not find %s in PATH"), tmp.c_str());
                return false;
            }
        }
    }
    int ret;
    char ldst[PATH_MAX+1];
    while (true) {
        struct stat st;
        if (lstat(tmp.fn_str(), &st) != 0) {
            wxLogSysError(_("Could not stat %s"), tmp.c_str());
            return false;
        }
        if (S_ISLNK(st.st_mode)) {
            ret = readlink(tmp.fn_str(), ldst, PATH_MAX);
            if (ret == -1) {
                wxLogSysError(_("Could not read link %s"), tmp.c_str());
                return false;
            }
            ldst[ret] = '\0';
            if (ldst[0] == '/')
                tmp = wxConvLocal.cMB2WX(ldst);
            else {
                fn.Assign(tmp);
                tmp = fn.GetPathWithSep() + wxConvLocal.cMB2WX(ldst);
            }
        } else {
            fn.Assign(tmp);
            fn.MakeAbsolute();
            break;
        }
    }
# undef NotImplemented
#endif
#ifdef NotImplemented
# error Missing Implementation for this OS
#endif
    m_sSelfPath = fn.GetFullPath();
    return true;
}

wxString opennxApp::findExecutable(wxString name)
{
    wxString ret = wxEmptyString;
    wxString path;
    if (wxGetEnv(wxT("PATH"), &path)) {
        if (path.IsEmpty())
            return ret;
        wxStringTokenizer t(path, wxT(":"));
        while (t.HasMoreTokens()) {
            wxFileName fn(t.GetNextToken(), name);
            if (fn.IsFileExecutable()) {
                ret = fn.GetFullPath();
                return ret;
            }
        }
    }
    return ret;
}

    bool
opennxApp::preInit()
{
    initWxTraceTags();
    setUserDir();
    if (!setSelfPath())
        return false;

    wxString appver;
    wxString thisver(wxT(PACKAGE_VERSION));
    thisver.Append(wxT(".")).Append(wxT(SVNREV));
    //sergeym
    wxConfigBase::Get()->Read(wxT("Config/CurrentVersion"), &appver, thisver);
    //if (!thisver.IsSameAs(appver)) {
        wxFileName fn(GetSelfPath());
        if (fn.GetDirs().Last().IsSameAs(wxT("bin")))
            fn.RemoveLastDir();
        fn.SetFullName(wxEmptyString);
        wxString thissysdir = fn.GetFullPath();
        wxString rest;
        wxString sep = wxFileName::GetPathSeparator();
        if (thissysdir.EndsWith(sep, &rest))
            thissysdir = rest;
        wxString cfgsysdir;
        wxConfigBase::Get()->Write(wxT("Config/SystemNxDir"), thissysdir);
        wxConfigBase::Get()->Flush();
       /* if (wxConfigBase::Get()->Read(wxT("Config/SystemNxDir"), &cfgsysdir)) {
            if (!thissysdir.IsSameAs(cfgsysdir)) {
                if (!m_bNoGui) {
                    wxString msg(wxString::Format(_("Your System NX directory setting (%s)\nappears to be incorrect.\nDo you want to change it to the correct default value\n(%s)?"), cfgsysdir.c_str(), thissysdir.c_str()));
                    int response = wxMessageBox(msg, _("Update System NX directory? - RX Client"), wxYES_NO|wxICON_QUESTION);
                    if (wxYES == response) {
                        wxConfigBase::Get()->Write(wxT("Config/SystemNxDir"), thissysdir);
                        wxConfigBase::Get()->Flush();
                    }
                }
            //}
        }*/
    //}
    wxConfigBase::Get()->Write(wxT("Config/CurrentVersion"), thisver);
    wxConfigBase::Get()->Flush();

    wxString tmp;
    if (!wxConfigBase::Get()->Read(wxT("Config/SystemNxDir"), &tmp)) {
        wxFileName fn(GetSelfPath());
        if (fn.GetDirs().Last().IsSameAs(wxT("bin")))
            fn.RemoveLastDir();
        fn.SetFullName(wxEmptyString);
        tmp = fn.GetFullPath();
        wxString rest;
        wxString sep = wxFileName::GetPathSeparator();
        if (tmp.EndsWith(sep, &rest))
            tmp = rest;
        wxConfigBase::Get()->Write(wxT("Config/SystemNxDir"), tmp);
        wxConfigBase::Get()->Flush();
    }
    m_cLocale.AddCatalogLookupPathPrefix(tmp + wxFileName::GetPathSeparator()
            + wxT("share") + wxFileName::GetPathSeparator() + wxT("locale"));
    m_cLocale.AddCatalogLookupPathPrefix(wxT("locale"));
    m_cLocale.Init();
    m_cLocale.AddCatalog(wxT("rxclient"));

   //sergeym
  //  if (!wxConfigBase::Get()->Read(wxT("Config/CupsPath"), &tmp)) {
#if defined(__LINUX__) || defined(__OPENBSD__) || defined(__WXMAC__)
        tmp = findExecutable(wxT("cupsd"));
        if (tmp.IsEmpty()) {
            const wxChar* candidates[] = {
                wxT("/sbin/cupsd"), wxT("/usr/sbin/cupsd"), wxT("/usr/local/sbin/cupsd"),
                NULL
            };
            int i;
            for (i = 0; candidates[i]; i++) {
                if (wxFileName::IsFileExecutable(candidates[i])) {
                    tmp = candidates[i];
                    break;
                }
            }
            if (tmp.IsEmpty()) {
                for (i = 0; candidates[i]; i++) {
                    if (wxFileName::FileExists(candidates[i])) {
                        tmp = candidates[i];
                        myLogTrace(MYTRACETAG, wxT("Found a CUPS daemon binary in %s, however it is not executable.\nIn order to use CUPS printing, you need to fix its permissions."), to_c_str(tmp));
                        break;
                    }
                }
                if (tmp.IsEmpty())
                    myLogTrace(MYTRACETAG, wxT("Could not find any CUPS daemon binary.\nIn order to use CUPS printing, you need to install cups."));
                tmp = wxEmptyString;
            }
        }
#endif
        wxConfigBase::Get()->Write(wxT("Config/CupsPath"), tmp);
        wxConfigBase::Get()->Flush();
  //  }


    if( ModuleManager::instance().exists("usbip") ) {
        if (!wxConfigBase::Get()->Read(wxT("Config/UsbipPort"), &tmp)) {
            wxConfigBase::Get()->Write(wxT("Config/UsbipPort"), 3240);
            wxConfigBase::Get()->Flush();
        }
    }

    wxConfigBase::Get()->Read(wxT("Config/SystemNxDir"), &tmp);
#ifdef __WXMSW__
    wxString ldpath;
    if (wxGetEnv(wxT("PATH"), &ldpath))
        ldpath += wxT(";");
    ldpath = tmp + wxT("\\bin");
    if (!wxSetEnv(wxT("PATH"), ldpath)) {
        wxLogSysError(wxT("Can not set PATH"));
        return false;
    }
#endif

#ifdef __UNIX__
# ifdef __WXMAC__
#  define LD_LIBRARY_PATH wxT("DYLD_LIBRARY_PATH")
# else
#  define LD_LIBRARY_PATH wxT("LD_LIBRARY_PATH")
# endif

# if defined(__x86_64) || defined(__IA64__)
    wxString archlib = wxT("lib64");
# else
    wxString archlib = wxT("lib");
# endif
    wxString ldpath;
    if (wxGetEnv(LD_LIBRARY_PATH, &ldpath))
        ldpath += wxT(":");
    ldpath += tmp + wxFileName::GetPathSeparator() + archlib;
    // see eterbug #11676
    ldpath += wxT(":");
    ldpath += tmp + wxFileName::GetPathSeparator() + archlib + wxFileName::GetPathSeparator() + wxT("rxclient");
# ifdef __WXMAC__
    if (wxFileName::DirExists(wxT("/Library/OpenSC/lib")))
        ldpath.Append(wxT(":/Library/OpenSC/lib"));
    if (wxFileName::DirExists(wxT("/usr/lib/samba")))
        ldpath.Append(wxT(":/usr/lib/samba"));
# else
    // If libjpeg-turbo is installed, prepend it's path in
    // order to speed-up image compression.
    // Don't do this for Mac OSX, as libjpeg-turbo breaks
    // Apple's ImageIO framework (which is used, when launching
    // an external browser).
    // On OSX, this path is added only before starting nxssh or nxproxy.
    wxFileName tjpeg;
    tjpeg.AssignDir(wxT("/usr"));
    tjpeg.AppendDir(archlib);
    tjpeg.AppendDir(wxT("libjpeg-turbo"));
    if (tjpeg.DirExists()) {
        ldpath = ldpath.Prepend(tjpeg.GetPath().Append(wxT(":")));
    } else {
        tjpeg.AssignDir(wxT("/opt/libjpeg-turbo"));
        tjpeg.AppendDir(archlib);
        if (tjpeg.DirExists()) {
            ldpath = ldpath.Prepend(tjpeg.GetPath().Append(wxT(":")));
        }
    }
# endif
    ::myLogDebug(wxT("%s='%s'"), LD_LIBRARY_PATH, to_c_str(ldpath));
    if (!wxSetEnv(LD_LIBRARY_PATH, ldpath)) {
        wxLogSysError(wxT("Cannot set LD_LIBRARY_PATH"));
        return false;
    }
#endif

    if (wxGetEnv(wxT("WXTRACE"), &tmp)) {
        CheckAllTrace(tmp);
        wxStringTokenizer t(tmp, wxT(",:"));
        while (t.HasMoreTokens()) {
            wxString tag = t.GetNextToken();
            if (allTraceTags.Index(tag) != wxNOT_FOUND) {
                ::myLogDebug(wxT("Trace for '%s' enabled"), to_c_str(tag));
                wxLog::AddTraceMask(tag);
            }
        }
    }

    checkNxSmartCardSupport();
    checkNxProxy();
    return true;
}

int opennxApp::FilterEvent(wxEvent& event)
{
    if (event.IsCommandEvent() && m_bRunproc) {
        wxCommandEvent *ce = (wxCommandEvent *)&event;
        if (ce->GetEventType() == wxEVT_GENERIC) {
            MyIPC::tSessionEvents e = wx_static_cast(MyIPC::tSessionEvents, ce->GetInt());
            wxString msg(ce->GetString());
            switch (e) {
                case MyIPC::ActionTerminated:
                    m_bRunproc = false;
                    return true;
                case MyIPC::ActionStderr:
                    if (msg.IsSameAs(wxT("no support for smartcards.")))
                        m_bNxSmartCardSupport = false;
                    return true;
                default:
                    break;
            }
            return false;
        }
    }
    return -1;
}

void opennxApp::checkNxSmartCardSupport()
{
    LibOpenSC l;
    if (!l.HasOpenSC()) {
        m_bNxSmartCardSupport = false;
        myLogTrace(MYTRACETAG, wxT("No OpenSC lib found, disabling SmartCard support"));
        wxConfigBase::Get()->Write(wxT("Config/NxSshSmartCardSupport"), m_bNxSmartCardSupport);
        wxConfigBase::Get()->Flush();
        return;
    }
    wxString sysdir;
    wxConfigBase::Get()->Read(wxT("Config/SystemNxDir"), &sysdir);
    wxFileName fn(sysdir, wxEmptyString);
    fn.AppendDir(wxT("bin"));

    auto m_pcsc = ModuleManager::instance().getModule("pcsc");
    if( !m_pcsc )
        return;

    fn.SetName( m_pcsc->getNxSshCmd(m_pSessionCfg, ModuleManager::instance().getDefaultNxSshCmd()) );

    if (!fn.FileExists())
        return;

    time_t last_mtime;
    long last_size;
    time_t mtime = fn.GetModificationTime().GetTicks();
    long size = fn.GetSize().ToULong();
    wxConfigBase::Get()->Read(wxT("Config/NxSshStamp"), &last_mtime, 0);
    wxConfigBase::Get()->Read(wxT("Config/NxSshSize"), &last_size, 0);
    wxConfigBase::Get()->Read(wxT("Config/NxSshSmartCardSupport"), &m_bNxSmartCardSupport, false);

    if ((mtime != last_mtime) || (size != last_size)) {
        wxConfigBase::Get()->Write(wxT("Config/NxSshStamp"), mtime);
        wxConfigBase::Get()->Write(wxT("Config/NxSshSize"), size);
        wxString nxsshcmd = fn.GetShortPath();
        nxsshcmd << wxT(" -I 0 -V");
        MyIPC testproc;
        if (testproc.GenericProcess(nxsshcmd, wxEmptyString, this)) {
            m_bNxSmartCardSupport = true;
            m_bRunproc = true;
            while (m_bRunproc) {
                wxLog::FlushActive();
                Yield(true);
                wxThread::Sleep(500);
            }
            wxConfigBase::Get()->Write(wxT("Config/NxSshSmartCardSupport"), m_bNxSmartCardSupport);
        }
        wxConfigBase::Get()->Flush();
    }
}

void opennxApp::checkNxProxy()
{
    wxString sysdir;
    wxConfigBase::Get()->Read(wxT("Config/SystemNxDir"), &sysdir);
    wxFileName fn(sysdir, wxEmptyString);
    fn.AppendDir(wxT("bin"));
#ifdef __WXMSW__
    fn.SetName(wxT("nxproxy.exe"));
#else
    fn.SetName(wxT("nxproxy"));
#endif
    m_bNxProxyAvailable = fn.IsFileExecutable();
}

void opennxApp::OnInitCmdLine(wxCmdLineParser& parser)
{
    // Init standard options (--help, --verbose);
    wxApp::OnInitCmdLine(parser);

    // tags will be appended to the last switch/option
    wxString tags;
    allTraceTags.Sort();
    size_t i;
    for (i = 0; i < allTraceTags.GetCount(); i++) {
        if (!tags.IsEmpty())
            tags += wxT(" ");
        tags += allTraceTags.Item(i);
    }
    tags.Prepend(_("\n\nSupported trace tags: "));

    parser.AddSwitch(wxEmptyString, wxT("autologin"),
            _("Automatically login to the specified session."));
    parser.AddSwitch(wxEmptyString, wxT("readonly"),
            _("Disable changing session by user."));
    parser.AddSwitch(wxEmptyString, wxT("autoresume"),
            _("Automatically resume/takeover a session with the same name."));
    parser.AddSwitch(wxEmptyString, wxT("killerrors"),
            _("Automatically destroy error dialogs at termination."));
    parser.AddSwitch(wxEmptyString, wxT("silent"),
            _("Run autologin in silent mode."));
    parser.AddSwitch(wxEmptyString, wxT("noprogressbar"),
            _("Turn progress bar during connection."));
    parser.AddSwitch(wxEmptyString, wxT("admin"),
            _("Start the session administration tool."));
    parser.AddOption(wxEmptyString, wxT("cacert"),
            _("Specify alternate CA cert for fetching configuration via https."));
    parser.AddOption(wxEmptyString, wxT("caption"),
            _("Specify window title for dialog mode."));
    parser.AddOption(wxEmptyString, wxT("style"),
            _("Specify dialog style for dialog mode."));
    parser.AddOption(wxEmptyString, wxT("dialog"),
            _("Run in dialog mode."));
    parser.AddOption(wxEmptyString, wxT("display"),
            _("Compatibility option for dialog mode (ignored)."));
    parser.AddOption(wxEmptyString, wxT("exportres"),
            _("Export builtin GUI resources to zip file and exit."));
    parser.AddSwitch(wxEmptyString, wxT("local"),
            _("Dialog mode proxy."));
    parser.AddOption(wxEmptyString, wxT("message"),
            _("Specify message for dialog mode."));
    parser.AddOption(wxEmptyString, wxT("parent"),
            _("Specify parent PID for dialog mode."), wxCMD_LINE_VAL_NUMBER);
    parser.AddOption(wxEmptyString, wxT("session"),
            _("Run a session importing configuration settings from FILENAME."));
    parser.AddOption(wxEmptyString, wxT("window"),
            _("Specify window-ID for dialog mode."), wxCMD_LINE_VAL_NUMBER);
    parser.AddOption(wxEmptyString, wxT("trace"),
            _("Specify wxWidgets trace mask."));
    parser.AddSwitch(wxEmptyString, wxT("check-modules"),
            _("Check which modules are connected."));

#ifdef __WXDEBUG__
    parser.AddSwitch(wxEmptyString, wxT("waittest"),
            _("Test CardWaiterDialog"));
#endif
    parser.AddSwitch(wxEmptyString, wxT("wizard"),
            _("Guide the user through the steps to configure a session.") + tags);
    // Workaround for commandline compatibility:
    // Despite of the doc (specifying space, colon and '='),
    // wxCmdLineParser insists on having a '=' as separator
    // between option and option-value. The original however
    // *requires* the separator to be a space instead.
#ifdef __WXMSW__
    wxRegEx re(wxT("^--((exportres)|(cacert)|(caption)|(style)|(dialog)|(display)|(message)|(parent)|(session)|(window)|(trace))$"));
#else
    // On Unix, --display is a toolkit option
    wxRegEx re(wxT("^--((exportres)|(cacert)|(caption)|(style)|(dialog)|(message)|(parent)|(session)|(window)|(trace))$"));
#endif

#if wxCHECK_VERSION(2,9,0)
    wxArrayString as;
    for(int i = 0; i < argc; i++)
    {
        wxString str(argv[i]);
        as.Add(str);
    }
#else
    wxArrayString as(argc, (const wxChar **)argv);
#endif

    for (i = 1; i < as.GetCount(); i++) {
        if (re.Matches(as[i])) {
            if ((i + 1) < as.GetCount()) {
                as[i].Append(wxT("=")).Append(as[i+1]);
                as.RemoveAt(i+1);
            }
        }
    }
    wxChar **xargv = new wxChar* [as.GetCount()];
    for (i = 0; i < as.GetCount(); i++)
        xargv[i] = wxStrdup(as[i].wc_str());
    parser.SetCmdLine(as.GetCount(), xargv);

}

static const wxChar *_dlgTypes[] = {
    wxT("yesno"), wxT("ok"), wxT("error"), wxT("panic"),
    wxT("quit"), wxT("pulldown"), wxT("yesnosuspend")
};

static wxArrayString aDlgTypes(sizeof(_dlgTypes)/sizeof(wxChar *), _dlgTypes);

static const wxChar *_dlgClasses[] = {
    wxT("info"), wxT("warning"), wxT("error")
};

static wxArrayString aDlgClasses(sizeof(_dlgClasses)/sizeof(wxChar *), _dlgClasses);

bool opennxApp::OnCmdLineParsed(wxCmdLineParser& parser)
{
    if (!wxApp::OnCmdLineParsed(parser))
        return false;
    wxString sDlgType;

    m_eMode = MODE_CLIENT;
    (void)parser.Found(wxT("cacert"), &m_sCaCert);
    if (parser.Found(wxT("dialog"), &sDlgType)) {
        wxString tmp;
        m_iDialogStyle = wxICON_WARNING;
        (void)parser.Found(wxT("caption"), &m_sDialogCaption);
        (void)parser.Found(wxT("message"), &m_sDialogMessage);
#ifdef __WXMAC__
        // Special message and caption tags, when called from watchreader
        if (m_sDialogCaption.IsSameAs(wxT("CARDREMOVED")))
            m_sDialogCaption = _("Smart card removed");
        if (m_sDialogMessage.IsSameAs(wxT("CARDREMOVED")))
            m_sDialogMessage = _("RX Client session has been suspended, because\nthe authenticating smart card has been removed.");
#endif
        (void)parser.Found(wxT("style"), &tmp);
        m_sDialogMessage.Replace(wxT("\\r\\n"), wxT("\n"));
        m_sDialogMessage.Replace(wxT("\\r"), wxT("\n"));
        m_sDialogMessage.Replace(wxT("\\n"), wxT("\n"));
        m_sDialogMessage.Replace(wxT("\\t"), wxT("\t"));
        if (!parser.Found(wxT("parent"), &m_nOtherPID))
            m_nOtherPID = (long)getppid();
        switch (aDlgClasses.Index(tmp)) {
            case 0:
                m_iDialogStyle = wxICON_INFORMATION;
                break;
            case 2:
                m_iDialogStyle = wxICON_ERROR;
                break;
            default:
                m_iDialogStyle = wxICON_WARNING;
                break;
        }
        switch (aDlgTypes.Index(sDlgType)) {
            case 0:
                // yesno
                m_iDialogStyle |= wxYES_NO;
                m_eMode = MODE_DIALOG_YESNO;
                return true;
            case 1:
                // ok
                m_iDialogStyle |= wxOK;
                m_eMode = MODE_DIALOG_OK;
                return true;
            case 2:
                // error
                m_iDialogStyle |= wxOK;
                m_eMode = MODE_DIALOG_ERROR;
                return true;
            case 3:
                // panic (Buttons: Terminate and Cancel, Terminate sends SIGTERM)
                m_eMode = MODE_DIALOG_PANIC;
                return true;
            case 4:
                // quit (Button: Quit, no signals)
                m_eMode = MODE_DIALOG_QUIT;
                return true;
#ifndef __WXMSW__
            case 5:
                // pulldown (a toolbar, docked to top-center of wID),
                // timing out after ~ 6sec. The toolbar has 3 buttons:
                // suspend, terminate and close.
                // suspend sends SIGHUP to real ppid, terminate sends SIGTERM to pPID
                // and close sends WM_CLOSE event to wID
                if (!parser.Found(wxT("window"), &m_nWindowID)) {
                    OnCmdLineError(parser);
                    return false;
                }
                m_eMode = MODE_FOREIGN_TOOLBAR;
                return true;
#else
                OnCmdLineError(parser);
                return false;
#endif
            case 6:
                // yesnosuspend (Buttons: Suspend, Terminate and Cancel,
                // Terminate sends SIGTERM to pPID, Suspend sends SIGHUP to real ppid)
                break;
            default:
                OnCmdLineError(parser);
                return false;
        }
        return false;
    }
    if (parser.Found(wxT("admin")))
        m_eMode = MODE_ADMIN;
    if (parser.Found(wxT("wizard")))
        m_eMode = MODE_WIZARD;
    if (parser.Found(wxT("exportres"), &m_sExportFile)) {
        m_eMode = MODE_EXPORTRES;
    }
    if (parser.Found(wxT("autologin")))
        m_bAutoLogin = true;
    if (parser.Found(wxT("readonly")))
        m_bConfigReadOnly = true;
    if (parser.Found(wxT("autoresume")))
        m_bAutoResume = true;
    if (parser.Found(wxT("killerrors")))
        m_bKillErrors = true;
    if (parser.Found(wxT("silent"))) {
        m_bKillErrors = true;
        m_bAutoLogin = true;
        m_bSilent = true;
    }
    if (parser.Found(wxT("noprogressbar")))
        m_bNoProgressBar = true;
    if (parser.Found(wxT("waittest")))
        m_bTestCardWaiter = true;
    (void)parser.Found(wxT("session"), &m_sSessionName);
    wxString traceTags;
    if (parser.Found(wxT("trace"), &traceTags)) {
        CheckAllTrace(traceTags);
        wxStringTokenizer t(traceTags, wxT(","));
        while (t.HasMoreTokens()) {
            wxString tag = t.GetNextToken();
            if (allTraceTags.Index(tag) == wxNOT_FOUND) {
                OnCmdLineError(parser);
                return false;
            }
            ::myLogDebug(wxT("Trace for '%s' enabled"), to_c_str(tag));
            wxLog::AddTraceMask(tag);
        }
    }

    if (parser.Found(wxT("check-modules"))) {
        std::cout << std::endl;
        std::cout << "Available modules:" << std::endl
                  << "==================" << std::endl;
        std::cout << ModuleManager::instance().modulesInfo() << std::endl;
        return false;
    }

    return true;
}

// 'Main program' equivalent: the program execution "starts" here
bool opennxApp::realInit()
{
    if (!preInit())
        return false;

    wxString tmp;
    wxConfigBase::Get()->Read(wxT("Config/SystemNxDir"), &tmp);

    // Win: Don't remap bitmaps to system colors
    wxSystemOptions::SetOption(wxT("msw.remap"), 0);
    // WinXP: Don't draw themed gradients on notebook pages
    wxSystemOptions::SetOption(wxT("msw.notebook.themed-background"), 0);

    // Call to base class needed for initializing command line processing
    if (!wxApp::OnInit())
        return false;
#ifdef __WXMAC__
    wxFileName::MacRegisterDefaultTypeAndCreator(wxT("nxs"), 'TEXT', 'OPNX');
#endif

    wxHelpProvider::Set(new wxSimpleHelpProvider);

    wxFileSystem::AddHandler(new wxZipFSHandler);
    wxFileSystem::AddHandler(new wxMemoryFSHandler);
    wxInitAllImageHandlers();
    wxBitmap::InitStandardHandlers();
    wxXmlResource::Get()->InitAllHandlers();
    wxXmlResource::Get()->AddHandler(new wxRichTextCtrlXmlHandler());

    // This enables socket-I/O from other threads.
    wxSocketBase::Initialize();

    bool resok = false;
    wxString optionalRsc = tmp + wxFileName::GetPathSeparator() + wxT("share")
        + wxFileName::GetPathSeparator() + wxT("opennx.rsc");
    if (wxFileName::FileExists(optionalRsc)) {
        wxFile rf(optionalRsc);
        if (rf.IsOpened()) {
            unsigned char *resptr = (unsigned char *)malloc(rf.Length());
            if (resptr) {
                if (rf.Read(resptr, rf.Length()) == rf.Length()) {
                    wxMemoryFSHandler::AddFileWithMimeType(wxT("memrsc"), resptr, rf.Length(), wxT("application/zip"));
                    {
                        // The following code eliminates a stupid error dialog which shows up
                        // if some .desktop entires (in KDE or GNOME applink dirs) are dangling symlinks.
                        wxLogNull lognull;
                        wxTheMimeTypesManager->GetFileTypeFromExtension(wxT("zip"));
                    }
                    resok = true;
                }
                free(resptr);
            }
        }
    }
    if (!resok) {
        {
            wxMemoryFSHandler::AddFileWithMimeType(wxT("memrsc"), tmpres_zip, tmpres_zip_len, wxT("application/zip"));
            {
                // The following code eliminates a stupid error dialog which shows up
                // if some .desktop entires (in KDE or GNOME applink dirs) are dangling symlinks.
                wxLogNull lognull;
                wxTheMimeTypesManager->GetFileTypeFromExtension(wxT("zip"));
            }
        }
        resok = true;
    }
    if (!resok) {
        wxLogFatalError(wxT("Could not load application resource."));
        return false;
    }

    m_sResourcePrefix = wxT("memory:memrsc#zip:");
    if (!wxXmlResource::Get()->Load(m_sResourcePrefix + wxT("res/opennx.xrc")))
        return false;

    switch (m_eMode) {
        case MODE_EXPORTRES:
            {
                if (!m_sExportFile.IsEmpty()) {
                    wxFileOutputStream os(m_sExportFile);
                    wxFileSystem fs;
                    wxFSFile *f = fs.OpenFile(wxT("memory:memrsc"));
                    if (NULL != f) {
                        wxInputStream *is = f->GetStream();
                        is->Read(os);
                    }
                }
                return false;
            }
        case MODE_CLIENT:
        case MODE_WIZARD:
        case MODE_MAC_WAITOPEN:
            break;
        case MODE_DIALOG_YESNO:
            {
                wxMessageDialog d(NULL, m_sDialogMessage, m_sDialogCaption, m_iDialogStyle);
                d.SetIcon(CreateIconFromFile(wxT("res/rxclient.png")));
                if (d.ShowModal() == wxYES)
                    wxKill(m_nOtherPID, wxSIGTERM);
                return false;
            }
            break;
        case MODE_DIALOG_OK:
            {
                wxMessageDialog d(NULL, m_sDialogMessage, m_sDialogCaption, m_iDialogStyle);
                d.SetIcon(CreateIconFromFile(wxT("res/rxclient.png")));
                d.ShowModal();
                return false;
            }
            break;
        case MODE_DIALOG_ERROR:
            {
                wxMessageDialog d(NULL, m_sDialogMessage, m_sDialogCaption, m_iDialogStyle);
                d.SetIcon(CreateIconFromFile(wxT("res/rxclient.png")));
                d.ShowModal();
                wxKill(m_nOtherPID, wxSIGTERM);
                return false;
            }
            break;
        case MODE_DIALOG_PANIC:
            {
                PanicDialog d;
                d.SetMessage(m_sDialogMessage);
                d.SetDialogClass(m_iDialogStyle);
                d.Create(NULL, wxID_ANY, m_sDialogCaption);
                if (d.ShowModal() == wxID_OK)
                    wxKill(m_nOtherPID, wxSIGTERM);
                return false;
            }
            break;
        case MODE_DIALOG_QUIT:
            {
                QuitDialog d;
                d.SetMessage(m_sDialogMessage);
                d.SetDialogClass(m_iDialogStyle);
                d.Create(NULL, wxID_ANY, m_sDialogCaption);
                d.ShowModal();
                return false;
            }
            break;
#ifndef __WXMSW__
        case MODE_FOREIGN_TOOLBAR:
            {
                ForeignFrame *ff = new ForeignFrame(NULL);
                // If we return true, the global config will
                // be deleted by the framework, so we set it to NULL here
                // to prevent double free.
                m_pCfg = NULL;
                ff->SetOtherPID(m_nOtherPID);
                ff->SetForeignWindowID(m_nWindowID);
                ff->Show();
                SetTopWindow(ff);
                return true;
            }
#endif
            break;
        case MODE_ADMIN:
            {
                SessionAdmin *sa = new SessionAdmin(NULL);
                // If we return true, the global config will
                // be deleted by the framework, so we set it to NULL here
                // to prevent double free.
                m_pCfg = NULL;
                sa->Show();
                SetTopWindow(sa);
                return true;
            }
            break;
    }

    if (m_bTestCardWaiter) {
        CardWaiterDialog cwd;
        cwd.WaitForCard(NULL);
        return false;
    }
#ifdef __WXMAC__
    // If we reach this point, we definitively are in dialog mode, so
    // on MacOSX we need set up a dummy menu
    // wxMenuBar *macmenubar = new wxMenuBar();
    // wxMenuBar::MacSetCommonMenuBar(macmenubar);
#endif
    if (!m_sSessionName.IsEmpty()) {
        wxFileName fn(m_sSessionName);
        if (fn.Normalize() && fn.FileExists())
            m_sSessionName = fn.GetFullPath();
    }
    if ((m_eMode == MODE_CLIENT) && m_sSessionName.IsEmpty()) {
#ifdef __WXMAC__
        // On OSX, opening of session files uses MacOpen().
        // Therefore, we have to introduce a special mode
        // where we wait up to 3 secs for an MacOpen event.
        m_eMode = MODE_MAC_WAITOPEN;
        time_t start = time(NULL) + 3;
        while (MODE_MAC_WAITOPEN == m_eMode) {
            wxYield();
            if (time(NULL) > start)
                break;
        }
        m_eMode = MODE_CLIENT;
#endif
        if (m_sSessionName.IsEmpty()) {
            // Second check is necessary, because above wait mode.
            if (!wxConfigBase::Get()->Read(wxT("Config/LastSession"), &m_sSessionName)) {
                // Only run the wizard if there a no session config files.
                wxString cfgdir;
                wxConfigBase::Get()->Read(wxT("Config/UserNxDir"), &cfgdir);
                cfgdir = cfgdir + wxFileName::GetPathSeparator() + wxT("config");
                wxArrayString a;
                wxDir::GetAllFiles(cfgdir, &a, wxT("*.nxs"), wxDIR_FILES);
                if (0 == a.GetCount())
                    m_eMode = MODE_WIZARD;
            }
        }
    } else {
        if (!m_sSessionName.IsEmpty()) {
            MyXmlConfig cfg(m_sSessionName);
            if (cfg.IsValid())
                m_sSessionName = cfg.sGetFileName();
        }
    }

    if (m_eMode == MODE_WIZARD) {
        MyWizard wz(NULL);
        if (!wz.Run())
            return false;
        m_sSessionName = wz.sGetConfigName();
    }

    LoginDialog d;
    d.SetLastSessionFilename(m_sSessionName);
    d.Create(NULL);
    m_pLoginDialog = &d;
    int result = d.ShowModal();
    m_pLoginDialog = NULL;
//    if (result == wxID_OK) {
//    save not only successfull login
        m_sSessionName = d.GetLastSessionFilename();
        if (!m_sSessionName.IsEmpty()) {
            wxConfigBase::Get()->Write(wxT("Config/LastSession"), m_sSessionName);
            wxConfigBase::Get()->Flush();
        }
//    }

    // success: wxApp::OnRun() will be called which will enter the main message
    // loop and the application will run. We returne FALSE here, so that the
    // application exits if the dialog is destroyed.
    return false;
}

// 'Main program' equivalent: the program execution "starts" here
bool opennxApp::OnInit()
{
    for (int i = 1; i < argc; ++i) {
        wxString tmp(argv[i]);
        if (tmp.StartsWith(wxT("--exportres"))) {
            m_bNoGui = true;
            break;
        }
        if (tmp.IsSameAs(wxT("--killerrors")) || tmp.IsSameAs(wxT("--silent"))) {
            m_bKillErrors = true;
            break;
        }
    }
    if (m_bKillErrors || m_bNoGui) {
        wxLog::SetActiveTarget(new wxLogStderr());
    }
    if (m_bKillErrors) {
        wxLog::SetLogLevel(0);
    }
    bool ret = realInit();

    if (m_bRequireWatchReader) {
        ::myLogTrace(MYTRACETAG, wxT("require Watchreader: m_iReader = %d, m_nNxSshPID = %ld"), m_iReader, m_nNxSshPID);
        if (-1 != m_iReader) {
            wxLogNull noerrors;
            wxString appDir;
            wxConfigBase::Get()->Read(wxT("Config/SystemNxDir"), &appDir);
            wxFileName fn(appDir, wxEmptyString);
            fn.AppendDir(wxT("bin"));
#ifdef __WXMSW__
            fn.SetName(wxT("watchreader.exe"));
#else
            fn.SetName(wxT("watchreader"));
#endif
            wxString watchcmd = fn.GetShortPath();
            watchcmd << wxT(" -r ") << m_iReader << wxT(" -p ") << m_nNxSshPID;
            ::myLogTrace(MYTRACETAG, wxT("executing %s"), to_c_str(watchcmd));
            wxExecute(watchcmd);
        }
    }

    while (wxGetApp().Pending())
        wxGetApp().Dispatch();
    if (!ret) {
        wxLogNull lognull;
        wxMemoryFSHandler::RemoveFile(wxT("memrsc"));
        exitApp("Login dialog closed, exiting");
    }
    return ret;
}

/*!
 * Cleanup for opennxApp
 */
int opennxApp::OnExit()
{
    delete wxHelpProvider::Set(NULL);
    {
        wxLogNull lognull;
        wxMemoryFSHandler::RemoveFile(wxT("memrsc"));
    }
    exitApp("Exiting opennxApp, manager is loading");

    return wxApp::OnExit();
}

void opennxApp::SetSessionCfg(MyXmlConfig &cfg)
{
    m_pSessionCfg = new MyXmlConfig();
    *m_pSessionCfg = cfg;
    m_pSessionCfg->sSetFileName(cfg.sGetFileName());
}

#ifdef __WXMAC__
/// Respond to Apple Event for opening a document
void opennxApp::MacOpenFile(const wxString& filename)
{
    ::myLogTrace(MYTRACETAG, wxT("MacOpen '%s'"), to_c_str(filename));
    m_sSessionName = filename;
    if (MODE_MAC_WAITOPEN == m_eMode) {
        ::myLogTrace(MYTRACETAG, wxT("MacOpen finishing wait"));
        m_eMode = MODE_CLIENT;
        return;
    }
    if (NULL != m_pLoginDialog) {
        ::myLogTrace(MYTRACETAG, wxT("MacOpen modifying selection in LoginDialog"));
        m_pLoginDialog->SelectSession(m_sSessionName);
    }
}
#endif

void opennxApp::exitApp(char * message)
{
    unsigned long procId = ::wxGetProcessId();
    if(procId)
    {
        fprintf(stderr, "%s\n", message);
        ::wxKill(procId);
    }
}
