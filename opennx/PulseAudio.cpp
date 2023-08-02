// $Id: PulseAudio.cpp 688 2012-02-18 02:36:07Z felfert $
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma implementation "PulseAudio.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "PulseAudio.h"
#include "MyDynlib.h"

#include <wx/log.h>
#include <wx/utils.h>
#include <wx/regex.h>
#include <wx/config.h>
#include <wx/wfstream.h>
#include <wx/txtstrm.h>
#include <wx/process.h>
#include <wx/tokenzr.h>

#ifdef APP_OPENNX
# include "opennxApp.h"
#endif
#ifdef APP_PULSETEST
# include "pulseTest.h"
#endif
#include "osdep.h"

#include "trace.h"
ENABLE_TRACE;

#if defined(HAVE_PULSE_PULSEAUDIO_H) || defined(__WXMSW__) || defined(__WXMAC__)
# define WITH_PULSEAUDIO
#endif

#ifdef WITH_PULSEAUDIO

#ifdef __WXMSW__
#include "packaging/win32/pulse/include/pulseaudio.h"
#else
#include <pulse/pulseaudio.h>
#endif

typedef pa_threaded_mainloop* (*Tpa_threaded_mainloop_new)(void);
typedef pa_mainloop_api* (*Tpa_threaded_mainloop_get_api)(pa_threaded_mainloop*);
typedef int (*Tpa_threaded_mainloop_start)(pa_threaded_mainloop *);
typedef void (*Tpa_threaded_mainloop_stop)(pa_threaded_mainloop *);
typedef void (*Tpa_threaded_mainloop_free)(pa_threaded_mainloop *);
typedef pa_context* (*Tpa_context_new)(pa_mainloop_api *, const char *);
typedef void (*Tpa_context_set_state_callback)(pa_context *, pa_context_notify_cb_t, void *);
typedef int (*Tpa_context_connect)(pa_context *, const char *, pa_context_flags_t, const pa_spawn_api *);
typedef void (*Tpa_context_disconnect)(pa_context *);
typedef pa_operation* (*Tpa_context_drain)(pa_context *, pa_context_notify_cb_t, void *);
typedef pa_context_state_t (*Tpa_context_get_state)(pa_context *);
typedef pa_operation* (*Tpa_context_get_server_info)(pa_context *, pa_server_info_cb_t, void *);
typedef pa_operation* (*Tpa_context_get_module_info_list)(pa_context *, pa_module_info_cb_t, void *);
typedef pa_operation* (*Tpa_context_load_module)(pa_context *, const char*, const char *, pa_context_index_cb_t, void *);
typedef pa_operation* (*Tpa_context_unload_module)(pa_context *, uint32_t, pa_context_success_cb_t, void *);
typedef int (*Tpa_context_errno)(pa_context *);
typedef void (*Tpa_context_unref)(pa_context *);
typedef void (*Tpa_operation_unref)(pa_operation *);
typedef const char* (*Tpa_strerror)(int);
typedef void (*Tpa_xfree)(void *p);


#define LOADPTR(name) p = dll->GetSymbol(wxT(#name)); if (NULL == p) return 0; P##name = (T##name)p
#define FPTR(name) static T##name P##name = NULL

FPTR(pa_threaded_mainloop_new);
FPTR(pa_threaded_mainloop_get_api);
FPTR(pa_threaded_mainloop_start);
FPTR(pa_threaded_mainloop_stop);
FPTR(pa_threaded_mainloop_free);
FPTR(pa_context_new);
FPTR(pa_context_set_state_callback);
FPTR(pa_context_connect);
FPTR(pa_context_disconnect);
FPTR(pa_context_drain);
FPTR(pa_context_get_state);
FPTR(pa_context_unref);
FPTR(pa_operation_unref);
FPTR(pa_context_get_server_info);
FPTR(pa_context_get_module_info_list);
FPTR(pa_context_load_module);
FPTR(pa_context_unload_module);
FPTR(pa_context_errno);
FPTR(pa_strerror);
FPTR(pa_xfree);

static int _set_pasyms(MyDynamicLibrary *dll) {
    void *p;
    LOADPTR(pa_threaded_mainloop_new);
    LOADPTR(pa_threaded_mainloop_get_api);
    LOADPTR(pa_threaded_mainloop_start);
    LOADPTR(pa_threaded_mainloop_stop);
    LOADPTR(pa_threaded_mainloop_free);
    LOADPTR(pa_context_new);
    LOADPTR(pa_context_set_state_callback);
    LOADPTR(pa_context_connect);
    LOADPTR(pa_context_disconnect);
    LOADPTR(pa_context_drain);
    LOADPTR(pa_context_get_state);
    LOADPTR(pa_context_unref);
    LOADPTR(pa_operation_unref);
    LOADPTR(pa_context_get_server_info);
    LOADPTR(pa_context_get_module_info_list);
    LOADPTR(pa_context_load_module);
    LOADPTR(pa_context_unload_module);
    LOADPTR(pa_context_errno);
    LOADPTR(pa_strerror);
    LOADPTR(pa_xfree);
    return 1;
}

// On windows, calling ::myLogTrace() from within libpulse's
// connection loop apparently crashes libpulse for some
// unknown reason. Therefore, we use native OutputDebugStringA
// in this case.
#ifdef __WXMSW__
# define STATE_TRACE(msg) OutputDebugStringA(msg)
#else
# define STATE_TRACE(msg) ::myLogTrace(MYTRACETAG, wxT(msg))
#endif

class pawrapper {
    private:
        typedef enum {
            NONE,
            LIST,
            LOAD_MODULE,
            UNLOAD_MODULE,
            SET_SINK_VOLUME,
            SET_SOURCE_VOLUME,
            SET_SINK_INPUT_VOLUME,
        } eAction;

    public:
        pawrapper()
            : m_bConnected(false), m_pLoop(NULL), m_pApi(NULL), m_pContext(NULL)
        {
            m_bError = false;
            m_bConnected = false;
#ifdef __WXMSW__
            const char * server = "127.0.0.1";
#else
            const char * server = NULL;
#endif
            m_pLoop = Ppa_threaded_mainloop_new();
            m_pApi = Ppa_threaded_mainloop_get_api(m_pLoop);
            m_pContext = Ppa_context_new(m_pApi, "RX Client");
            Ppa_context_set_state_callback(m_pContext, context_state_callback_if, this);
            int retry = 3;
            do {
                m_bError = false;
                ::myLogTrace(MYTRACETAG, wxT("pa_context_connect try %d"), 4 - retry);
                if (0 <= Ppa_context_connect(m_pContext, NULL /* server */, PA_CONTEXT_NOAUTOSPAWN, NULL)) {
                    Ppa_threaded_mainloop_start(m_pLoop);
                    while (!(m_bConnected || m_bError))
                        wxGetApp().Yield(true);
                }
                if (m_bConnected)
                    break;
            } while (retry-- > 0);
        }

        ~pawrapper()
        {
#ifndef __WXMSW__
            if (m_bConnected)
                Ppa_context_disconnect(m_pContext);
#endif
            if (m_pLoop) {
                Ppa_threaded_mainloop_stop(m_pLoop);
                Ppa_threaded_mainloop_free(m_pLoop);
            }
            if (NULL != m_pContext)
                Ppa_context_unref(m_pContext);
        }

        bool getdefaults(wxString &Sink, wxString &Source)
        {
            m_bComplete = false;
            Ppa_operation_unref(Ppa_context_get_server_info(m_pContext, get_server_info_callback_if, this));
            bool ret = waitcmd();
            if (ret && m_bComplete) {
                Sink = m_DefSink;
                Source = m_DefSource;
            }
            return ret && m_bComplete;
        }

        bool findmodules(const wxChar *name, wxArrayString &indexes, wxArrayString &args)
//        bool findmodule(const wxChar *name, wxString &args, unsigned int &index)
        {
            m_sStr = name;
            m_bSearch = true;
            m_bFound = false;
            m_bComplete = false;
            m_bError = false;
            m_asIndexes.Empty(); m_asArgs.Empty();
            Ppa_operation_unref(Ppa_context_get_module_info_list(m_pContext, get_module_info_callback_if, this));
            bool ret = waitcmd();
            if (ret && m_bFound) {
                args = m_asArgs;
                indexes = m_asIndexes;
            }
            return ret && m_bFound;
        }

        bool loadmodule(const wxString name, const wxString args)
        {
            m_bComplete = false;
            m_bError = false;
            Ppa_operation_unref(Ppa_context_load_module(m_pContext, name.mb_str(), args.mb_str(), index_callback_if, this));
            return waitcmd();
        }

        bool unloadmodule(int index)
        {
            m_bComplete = false;
            m_bError = false;
            Ppa_operation_unref(Ppa_context_unload_module(m_pContext, index, simple_callback_if, this));
            return waitcmd();
        }

        bool isConnected()
        {
            return m_bConnected;
        }

    private:

        bool waitcmd() {
            while (!(m_bError || m_bComplete)) {
                wxGetApp().Yield(true);
            }
            return !m_bError;
        }

#if 0
        void runcmd(eAction a)
        {
            m_bComplete = false;
            m_bError = false;
            switch (a) {
                case SET_SINK_VOLUME:
                    {
                        pa_cvolume v;

                        pa_cvolume_set(&v, 1, volume);
                        Ppa_operation_unref(pa_context_set_sink_volume_by_name(m_pContext, sink_name, &v, simple_callback_if, NULL));
                        break;
                    }
                case SET_SOURCE_VOLUME:
                    {
                        pa_cvolume v;

                        pa_cvolume_set(&v, 1, volume);
                        Ppa_operation_unref(pa_context_set_source_volume_by_name(m_pContext, source_name, &v, simple_callback_if, NULL));
                        break;
                    }
                case SET_SINK_INPUT_VOLUME:
                    {
                        pa_cvolume v;

                        pa_cvolume_set(&v, 1, volume);
                        Ppa_operation_unref(pa_context_set_sink_input_volume(m_pContext, sink_input_idx, &v, simple_callback_if, NULL));
                        break;
                    }
            }
        }
#endif

        void drain(void) {
            pa_operation *o = Ppa_context_drain(m_pContext, context_drain_complete_if, this);
            if (NULL == o) {
                ::myLogTrace(MYTRACETAG, wxT("drain_complete"));
                m_bComplete = true;
            } else
                Ppa_operation_unref(o);
        }

        void context_state_callback(pa_context *c)
        {
            if (NULL == c)
                return;
            switch (Ppa_context_get_state(c)) {
                case PA_CONTEXT_UNCONNECTED:
                    STATE_TRACE("PA_CONTEXT_UNCONNECTED");
                    break;
                case PA_CONTEXT_CONNECTING:
                    STATE_TRACE("PA_CONTEXT_CONNECTING");
                    break;
                case PA_CONTEXT_AUTHORIZING:
                    STATE_TRACE("PA_CONTEXT_AUTHORIZING");
                    break;
                case PA_CONTEXT_SETTING_NAME:
                    STATE_TRACE("PA_CONTEXT_SETTING_NAME");
                    break;
                case PA_CONTEXT_READY:
                    STATE_TRACE("PA_CONTEXT_READY");
                    m_bConnected = true;
                    break;
                case PA_CONTEXT_TERMINATED:
                    STATE_TRACE("PA_CONTEXT_TERMINATED");
                    break;
                case PA_CONTEXT_FAILED:
                    STATE_TRACE("PA_CONTEXT_FAILED");
                    m_bError = true;
                    break;
                default:
                    STATE_TRACE("PA_CONTEXT_default");
                    m_bError = true;
                    break;
            }
        }

        void get_server_info_callback(pa_context *c, const pa_server_info *i)
        {
            if (!i) {
                myLogTrace(MYTRACETAG, wxT("Failed to get server information: %s"), Ppa_strerror(Ppa_context_errno(c)));
                m_bError = true;
                m_pApi->quit(m_pApi, 0);
                return;
            }

            wxString dsink(i->default_sink_name ? i->default_sink_name : "", wxConvUTF8);
            wxString dsrc(i->default_source_name ? i->default_source_name : "", wxConvUTF8);
            m_DefSink = dsink; m_DefSource = dsrc; m_bComplete = true;
        }

        void get_module_info_callback(pa_context *c, const pa_module_info *i, int is_last)
        {
            if (is_last < 0) {
                ::myLogTrace(MYTRACETAG, wxT("Failed to get module information: %s"), Ppa_strerror(Ppa_context_errno(c)));
                m_bError = true;
                m_pApi->quit(m_pApi, 0);
                return;
            }
            if (is_last) {
                drain();
                return;
            }
            if (NULL != i) {
                wxString name(i->name, wxConvUTF8);
                wxString args(i->argument ? i->argument : "", wxConvUTF8);
                ::myLogTrace(MYTRACETAG, wxT("module[%u] %s %s"),
                        i->index, to_c_str(name), to_c_str(args));
                if (m_bSearch) {
                    if (name.IsSameAs(m_sStr)) {
                        m_bSearch = false;
                        m_bFound = true;
                        m_iIndex = i->index;
                        m_sStr = args;
                    }
                }
            }
        }

        void index_callback(pa_context *c, uint32_t idx)
        {
            if (idx == PA_INVALID_INDEX) {
                ::myLogTrace(MYTRACETAG, wxT("Index failure: %s"), Ppa_strerror(Ppa_context_errno(c)));
                m_bError = true;
                m_pApi->quit(m_pApi, 0);
                return;
            }
            m_iIndex = idx;
            drain();
        }

        void simple_callback(pa_context *c, int success)
        {
            if (!success) {
                m_bError = true;
                ::myLogTrace(MYTRACETAG, wxT("Simple failure: %s"), Ppa_strerror(Ppa_context_errno(c)));
                m_pApi->quit(m_pApi, 0);
                return;
            }
            drain();
        }

        void context_drain_complete(pa_context *)
        {
            ::myLogTrace(MYTRACETAG, wxT("context_drain_complete"));
            m_bComplete = true;
        }

        static void simple_callback_if(pa_context *c, int success, void *udata) {
            if (NULL == udata)
                return;
            static_cast<pawrapper *>(udata)->simple_callback(c, success);
        }

        static void index_callback_if(pa_context *c, uint32_t idx, void *udata) {
            if (NULL == udata)
                return;
            static_cast<pawrapper *>(udata)->index_callback(c, idx);
        }

        static void context_drain_complete_if(pa_context *c, void *udata) {
            if (NULL == udata)
                return;
            static_cast<pawrapper *>(udata)->context_drain_complete(c);
        }

        static void get_module_info_callback_if(pa_context *c, const pa_module_info *i, int last, void *udata) {
            if (NULL == udata)
                return;
            static_cast<pawrapper *>(udata)->get_module_info_callback(c, i, last);
        }

        static void get_server_info_callback_if(pa_context *c, const pa_server_info *i, void *udata) {
            if (NULL == udata)
                return;
            static_cast<pawrapper *>(udata)->get_server_info_callback(c, i);
        }

        static void context_state_callback_if(pa_context *c, void *udata) {
            if (NULL == udata)
                return;
            static_cast<pawrapper *>(udata)->context_state_callback(c);
        }

        volatile bool m_bConnected;
        volatile bool m_bComplete;
        volatile bool m_bError;
        volatile bool m_bSearch;
        volatile bool m_bFound;
        unsigned int m_iIndex;
        pa_threaded_mainloop *m_pLoop;
        pa_mainloop_api *m_pApi;
        pa_context *m_pContext;
        wxString m_sStr;

        wxString m_DefSink;
        wxString m_DefSource;
        wxArrayString m_asIndexes;
        wxArrayString m_asArgs;
};

# if defined(__WXMSW__) || defined(__WXMAC__)
#  ifdef __WXMAC__
extern "C" {
    extern const char *getMacMachineID();
};
#  endif

static wxString MachineID() {
#  ifdef __WXMSW__
    return wxGetHostName().Lower();
#  else
    return wxString(getMacMachineID(), wxConvUTF8);
#  endif
}
# endif // defined(__WXMSW__) || defined(__WXMAC__)
#endif // WITH_PULSEAUDIO

bool PulseAudio::AutoSpawn()
{
#ifdef WITH_PULSEAUDIO
# if defined(__WXMSW__) || defined(__WXMAC__)
    int papid;
    int retry = 3;
    // On windows and mac, we do our own autospawn
    wxString piddir = ::wxGetHomeDir() + wxFileName::GetPathSeparator()
        + wxT(".config") + wxFileName::GetPathSeparator()
        + wxT("pulse") + wxFileName::GetPathSeparator()
        + MachineID() + wxT("-runtime");
    wxString pidfile = piddir + wxFileName::GetPathSeparator() + wxT("pid");
        wxString pacmd;
        wxConfigBase::Get()->Read(wxT("Config/SystemNxDir"), &pacmd);
        pacmd << wxFileName::GetPathSeparator() << wxT("bin")
            << wxFileName::GetPathSeparator() << wxT("pulseaudio");
#  ifdef __WXMSW__
        pacmd << wxT(".exe --exit-idle-time=-1 --log-target=file:\"")
            << ::wxGetHomeDir() << wxFileName::GetPathSeparator()
            << wxT(".config") << wxFileName::GetPathSeparator()
            << wxT("pulse") <<  wxFileName::GetPathSeparator()
            << wxT("pa.log\"");
#  endif
    do {
#  ifdef __WXMSW__
        ::myLogTrace(MYTRACETAG, wxT("PulseAudio::AutoSpawn: checking pulseaudio process"));
        papid = getpidof("pulseaudio.exe");
        if (papid != 0) {
                myLogTrace(MYTRACETAG, wxT("PulseAudio::AutoSpawn: process %d is running"), papid);
                return true;
        }
#  else
        ::myLogTrace(MYTRACETAG, wxT("PulseAudio::AutoSpawn: checking '%s'"), to_c_str(pidfile));
        wxFileInputStream sPid(pidfile);
        if (sPid.IsOk()) {
            ::myLogTrace(MYTRACETAG, wxT("PulseAudio::AutoSpawn: PID file exists"));
            wxTextInputStream tis(sPid);
            tis >> papid;
            ::myLogTrace(MYTRACETAG, wxT("PulseAudio::AutoSpawn: PID=%d"), papid);
            if ((papid != 0) && wxProcess::Exists(papid)) {
                ::myLogTrace(MYTRACETAG, wxT("PulseAudio::AutoSpawn: process %d is running"), papid);
                return true;
            }
        }
#  endif
        ::myLogTrace(MYTRACETAG, wxT("PulseAudio::AutoSpawn: trying to start '%s'"), to_c_str(pacmd));
#  ifdef __WXMSW__
        wxProcess *nxpa = wxProcess::Open(pacmd,
                                        wxEXEC_ASYNC|wxEXEC_MAKE_GROUP_LEADER);
        wxThread::Sleep(100);
        if (nxpa) {
            nxpa->CloseOutput();
            nxpa->Detach();
        }
#  else
        ::wxExecute(pacmd, wxEXEC_ASYNC|wxEXEC_MAKE_GROUP_LEADER);
#  endif
        wxThread::Sleep(1000);
    } while (retry-- > 0);
    ::myLogTrace(MYTRACETAG, wxT("PulseAudio::AutoSpawn: spawn failed"));
    return false;
# else
    myLogTrace(MYTRACETAG, wxT("Not spawning pulseaudio on this platform"));
    return true;
# endif // defined(__WXMSW__) || defined(__WXMAC__)
#else
    return true;
#endif // WITH_PULSEAUDIO
}

    PulseAudio::PulseAudio()
: pa(NULL), dll(NULL), m_bPulseAvailable(false)
{
    m_iPortEsound = 0; m_iPortNative = 0;
#ifdef WITH_PULSEAUDIO
    wxLogNull ignoreErrors;
    if (AutoSpawn()) {
        dll = new MyDynamicLibrary();
# ifdef __WXMSW__
        wxString pdll = wxT("libpulse-0");
# else
        wxString pdll = wxT("libpulse");
# endif
        if (dll->Load(pdll)) {
            ::myLogTrace(MYTRACETAG, wxT("libpulse loaded"));
            if (0 != _set_pasyms(dll)) {
                ::myLogTrace(MYTRACETAG, wxT("libpulse functions loaded"));
                pa = new pawrapper();
                if (pa->isConnected()) {
                    m_bPulseAvailable = true;
                    ::myLogTrace(MYTRACETAG, wxT("connected to pulseaudio daemon"));
                }
            }
        }
    }
#else
    ::myLogTrace(MYTRACETAG, wxT("No pulseaudio support"));
#endif
}

PulseAudio::~PulseAudio()
{
#ifdef WITH_PULSEAUDIO
    delete pa;
    delete dll;
#endif
}

bool PulseAudio::IsAvailable()
{
    ::myLogTrace(MYTRACETAG, wxT("IsAvailable:%s"), m_bPulseAvailable ? wxT("true") : wxT("false"));
    return m_bPulseAvailable;
}

int PulseAudio::FoundModuleIDs(wxString modname, wxString s_argstpl,
                                wxArrayString &a_indexes, wxArrayString &a_args,
                                bool HardAccordance = false)
{
#ifdef WITH_PULSEAUDIO
    wxRegEx re;
    wxArrayString indexes, args_args; indexes.Empty(); args_args.Empty();
    if (!pa->findmodules(modname.wc_str(), indexes, args_args))
        return 0;
    wxArrayString argstpl = ::wxStringTokenize(s_argstpl);
    myLogTrace(MYTRACETAG, wxT("Found modules named %s (argstpl=%d): idx/args_args count  = %d/%d"),
                to_c_str(modname),(int)argstpl.GetCount(),(int)indexes.GetCount(),
                (int)args_args.GetCount());
    //wxLogInfo(wxT("modename %s, s_argstpl %s, a_indexes %s, a_args %s"),
    //            to_c_str(modname),(int)argstpl.GetCount(),(int)indexes.GetCount(),
    //            (int)args_args.GetCount());
    a_indexes.Empty(); a_args.Empty(); int c = 0;
    for (int i = 0; i < indexes.GetCount(); i++)
    {
        if (argstpl.IsEmpty() && !HardAccordance) {
            a_indexes.Add(indexes[i]); a_args.Add(args_args[i]); c++;
            continue;
        }
        wxArrayString margs = ::wxStringTokenize(args_args[i]);
#ifdef PA_ADEBUG
        myLogTrace(MYTRACETAG, wxT("Checking mod[%s], in args = '%s';  count = %d "),
                    to_c_str(indexes[i]), to_c_str(args_args[i]),(int)margs.GetCount());
    //    wxLogInfo(MYTRACETAG, wxT("Checking mod[%s], in args = '%s';  count = %d "),
    //                to_c_str(indexes[i]), to_c_str(args_args[i]),(int)margs.GetCount());
#endif
        if (HardAccordance && (argstpl.GetCount() != margs.GetCount()))
            continue;
        bool Accordance = true;
        for (int j = 0; j < argstpl.GetCount(); j++) {
#ifdef PA_ADEBUG
            myLogTrace(MYTRACETAG, wxT("Checking template '%s'"), to_c_str(argstpl[j]));
    //        wxLogInfo(MYTRACETAG, wxT("Checking template '%s'"), to_c_str(argstpl[j]));
#endif
            re.Compile(argstpl[j], wxRE_ADVANCED);
            bool rfound = false;
            for (int k = 0; k < margs.GetCount(); k++) {
                rfound = re.Matches(margs[k]);
#ifdef PA_ADEBUG
                myLogTrace(MYTRACETAG, wxT("Checking arg '%s' -> res = %d"),
                                                        to_c_str(margs[k]), rfound);
        //        wxLogInfo(MYTRACETAG, wxT("Checking arg '%s' -> res = %d"),
        //                                                to_c_str(margs[k]), rfound);
#endif
                if (rfound)
                    break;
            }
            if (!rfound) {
                Accordance = false; break;
            }
        }
        if (Accordance) {
            a_indexes.Add(indexes[i]); a_args.Add(args_args[i]); c++;
        }
    }
    myLogTrace(MYTRACETAG, wxT("Matches %d modules named %s: idxs/args = %d/%d"),
                c, to_c_str(modname), (int)a_indexes.GetCount(),(int)a_args.GetCount());
    //wxLogInfo(MYTRACETAG, wxT("Matches %d modules named %s: idxs/args = %d/%d"),
    //            c, to_c_str(modname), (int)a_indexes.GetCount(),(int)a_args.GetCount());
    return c;
#else
    wxUnusedVar(modname);
    wxUnusedVar(s_argstpl);
    wxUnusedVar(HardAccordance);
    return -1;
#endif
}

bool PulseAudio::UnloadExistingModules(wxString modname, wxString s_argstpl,
                                        bool HardAccordance = false)
{
    if (!m_bPulseAvailable)
        return false;
#ifdef WITH_PULSEAUDIO
    wxArrayString mis,mas; mis.Empty(); mas.Empty();
    bool res = true; bool res0;
    if (FoundModuleIDs(modname,s_argstpl,mis,mas,HardAccordance) > 0) {
        for (int i = 0; i < mis.GetCount(); i++) {
            long  mid; mis[i].ToLong(&mid);
            res0 = pa->unloadmodule(mid);
            myLogTrace(MYTRACETAG, wxT("unloading %s module[%d] -> res = %d"),
                        to_c_str(modname), (int)mid, (int)res);
            res = res ? res0 : res;
        }
    }
    return res;
#else
    wxUnusedVar(modname);
    wxUnusedVar(s_argstpl);
    wxUnusedVar(HardAccordance);
    return false;
#endif
}

bool PulseAudio::ActivateEsound(int port)
{
    if (!m_bPulseAvailable)
        return false;
#ifdef WITH_PULSEAUDIO
    wxString mname = wxT("module-esound-protocol-tcp");
    UnloadExistingModules(mname,wxT(""));
    wxString ma = wxString::Format(wxT("port=%d"), port);
    UnloadExistingModules(wxT("module-native-protocol-tcp"),ma);
    ma.Append(wxString::Format(wxT(" listen=127.0.0.1 auth-anonymous=1"), port));
    bool res = pa->loadmodule(mname,ma);
    myLogTrace(MYTRACETAG, wxT("loading %s module -> res = %d"),
                                        to_c_str(mname), res);
    return res;
#else
    wxUnusedVar(port);
    return false;
#endif
}
bool PulseAudio::ActivateNative(int port, int rrate, bool mono)
{
    if (!m_bPulseAvailable)
        return false;
#ifdef WITH_PULSEAUDIO
    UnloadExistingModules(wxT("module-esound-protocol-tcp"),wxT(""));
    wxString mname = wxT("module-native-protocol-tcp");
    wxString ma = wxString::Format(wxT("port=%d"), port);
    wxArrayString mis,mas; bool res;
    bool bNativeModulePort = (FoundModuleIDs(mname,ma,mis,mas) > 0);
    wxString mname_fake = wxT("module-null-sink");
    wxString mname_conn = wxT("module-loopback");
    if (!bNativeModulePort) {
        ma.Append(wxString::Format(wxT(" listen=127.0.0.1 auth-anonymous=1")));
        res = pa->loadmodule(mname,ma);
        myLogTrace(MYTRACETAG, wxT("loading %s module -> res = %d (args = '%s')"),
                    to_c_str(mname), (int)res, to_c_str(ma));
    }
    //Strange indented if-statements but it works. Why?
    if (!res) {
        if (PulsePortChecker::isPortBusy(port))
            return true;
        return res;
    }
    wxString ma_fake = wxString::Format(wxT(" rate=%d channels=%s"),rrate,
                                        mono ?wxT("1"):wxT("2"));
    if ((rrate == 0) || (FoundModuleIDs(mname_fake,ma_fake,mis,mas) == 0)) {
        wxString deltpl = wxT("(sink|sink_name|source)=(ts_sender|ts_receiver)");
        UnloadExistingModules(mname_conn,deltpl);
        UnloadExistingModules(mname_fake,deltpl);
        if (rrate == 0)
            return res;
    }
#ifndef __WXMSW__
    wxString dsi,dso; dsi.Empty(); dso.Empty();
    pa->getdefaults(dsi,dso);
    myLogTrace(MYTRACETAG, wxT("Get defaults: Sink ='%s'; Source ='%s'"),
                    to_c_str(dsi), to_c_str(dso));
    if ((dsi.IsEmpty()) || (dso.IsEmpty()))
        return true; // modules for resample are optional now
#else
    // on win we check availability of sink and source only
    // because old resample scheme not work
    // on systems >=w7 is necessary try to load sink and source
    // separately since audio devices are present according to
    // speakers/headset/microphone connect status
    // Also these modules always must be reloaded in case
    // recent jacks reconnection
    bool IsSink, IsSource;
    mname = wxT("module-waveout");
    ma = wxT("sink_name=output");
    UnloadExistingModules(mname,ma);
    ma.Append(wxT(" record=0"));
    IsSink = pa->loadmodule(mname,ma);
    myLogTrace(MYTRACETAG, wxT("loading %s module -> res = %d (args = '%s')"),
                to_c_str(mname), (int)IsSink, to_c_str(ma));
    ma = wxT("source_name=input");
    UnloadExistingModules(mname,ma);
    ma.Append(wxT(" playback=0"));
    IsSource = pa->loadmodule(mname,ma);
    myLogTrace(MYTRACETAG, wxT("loading %s module -> res = %d (args = '%s')"),
                to_c_str(mname), (int)IsSource, to_c_str(ma));
    res = (IsSink || IsSource);
    return res;
#endif

#ifndef __WXMSW__
    ma = wxT("sink_name=ts_sender") + ma_fake;
    if (!FoundModuleIDs(mname_fake,wxT("sink_name=ts_sender"),mis,mas)) {
        res = pa->loadmodule(mname_fake,ma);
        myLogTrace(MYTRACETAG, wxT("loading %s module -> res = %d (args = '%s')"),
                    to_c_str(mname_fake), (int)res, to_c_str(ma));
    }

    ma = wxT("source=") + dso + wxT(" sink=ts_sender");
    if (!FoundModuleIDs(mname_conn,wxT("sink=ts_sender"),mis,mas)) {
        res = pa->loadmodule(mname_conn,ma);
        myLogTrace(MYTRACETAG, wxT("loading %s module -> res = %d (args = '%s')"),
                    to_c_str(mname_conn), (int)res, to_c_str(ma));
    }

    ma = wxT("sink_name=ts_receiver") + ma_fake;
    if (!FoundModuleIDs(mname_fake,wxT("sink_name=ts_receiver"),mis,mas)) {
        res = pa->loadmodule(mname_fake,ma);
        myLogTrace(MYTRACETAG, wxT("loading %s module -> res = %d (args = '%s')"),
                    to_c_str(mname_fake), (int)res, to_c_str(ma));
    }

    ma = wxT("source=ts_receiver.monitor sink=") + dsi;
    if (!FoundModuleIDs(mname_conn,wxT("source=ts_receiver.monitor"),mis,mas)) {
        res = pa->loadmodule(mname_conn,ma);
        myLogTrace(MYTRACETAG, wxT("loading %s module -> res = %d (args = '%s')"),
                    to_c_str(mname_conn), (int)res, to_c_str(ma));
    }
    return true;
#endif
#else
    wxUnusedVar(port);
    wxUnusedVar(rrate);
    wxUnusedVar(mono);
    return false;
#endif
}

