#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "wx/artprov.h"

#include "MyMessageDialog.h"
IMPLEMENT_DYNAMIC_CLASS( MyMessageDialog, wxDialog )

BEGIN_EVENT_TABLE( MyMessageDialog, wxDialog )

END_EVENT_TABLE()


MyMessageDialog::MyMessageDialog()
{

}

MyMessageDialog::MyMessageDialog (wxWindow * parent, const wxString & text,
                                  const wxString & title,
                                  long style,
                                  const wxPoint & pos,
                                  const wxSize & size )
: wxDialog(parent, wxID_ANY, title, pos, wxDefaultSize, style)
{
    Create(parent, text, title, style);
}

MyMessageDialog::~MyMessageDialog()
{

}

void MyMessageDialog::Create( wxWindow *parent,
                             const wxString &text, const wxString &title, long style)
{
    int border = 10;

    wxBoxSizer *vbox = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *hbox1 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer *hbox2 = new wxBoxSizer(wxHORIZONTAL);

    wxArtID aID = wxART_ERROR;
    switch ( style & wxICON_MASK )
    {
    case wxICON_WARNING:
        aID = wxART_WARNING;
        break;

    case wxICON_ERROR:
        aID = wxART_ERROR;
        break;

    case wxICON_INFORMATION:
        aID = wxART_INFORMATION;
        break;

    default:
        break;
    }

   wxBitmap bmp = wxArtProvider::GetBitmap(aID, wxART_MESSAGE_BOX);

    hbox1->Add(
          new wxStaticBitmap(this, wxID_ANY, bmp, wxDefaultPosition, wxDefaultSize, 0),
          0, wxLEFT | wxTOP, 0
    );

    hbox1->Add(
          new wxStaticText( this, -1, text, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE ),
          1, wxLEFT, border
    );

    vbox->Add(hbox1, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, border);
    vbox->Add(-1, border);

    wxButton *btn1 = new wxButton( this, wxID_OK, _("OK"));
    hbox2->Add(btn1, 0);
    vbox->Add(hbox2, 0, wxALIGN_RIGHT | wxRIGHT, border);
    vbox->Add(-1, border);

    SetSizerAndFit(vbox);

    wxPoint p_pos = parent->GetScreenPosition();
    wxSize p_sz = parent->GetSize();

    wxPoint dlgpos = p_pos;
    dlgpos.y = dlgpos.y + p_sz.GetHeight();

    wxSize m_sz = GetSize();
    if( m_sz.GetWidth() > p_sz.GetWidth() )
        dlgpos.x = dlgpos.x - (m_sz.GetWidth() - p_sz.GetWidth() ) / 2;

    Move(dlgpos.x, dlgpos.y);
}
