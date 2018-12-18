#ifndef _MYMESSAGEDIALOG_H_
#define _MYMESSAGEDIALOG_H_

#include "wx/dialog.h"

// special message dialog located under the parent window
class MyMessageDialog: public wxDialog
{
    DECLARE_DYNAMIC_CLASS( MyMessageDialog )
    DECLARE_EVENT_TABLE()

public:

        MyMessageDialog();

    MyMessageDialog (wxWindow * parent, const wxString & text,
                  const wxString & title,
                  long style = wxDEFAULT_DIALOG_STYLE,
                  const wxPoint & pos = wxDefaultPosition,
                  const wxSize & size = wxDefaultSize );

    ~MyMessageDialog();

protected:

//    void Init(){}
    void Create(wxWindow * parent, const wxString & text,
                 const wxString & title,
                 long style = wxDEFAULT_DIALOG_STYLE);

};

#endif
