/**********************************************************************

  Audacity: A Digital Audio Editor

  LyricsWindow.cpp

  Vaughan Johnson
  Dominic Mazzoni

**********************************************************************/

#include "LyricsWindow.h"
#include "Lyrics.h"
#include "AudioIO.h"
#include "Prefs.h" // for RTL_WORKAROUND
#include "Project.h"
#include "ProjectAudioIO.h"
#include "TrackPanel.h" // for EVT_TRACK_PANEL_TIMER
#include "ViewInfo.h"

#include <wx/radiobut.h>
#include <wx/toolbar.h>
#include <wx/settings.h>

#ifdef __WXMSW__
   #include "../images/AudacityLogo.xpm"
#else
   #include "../images/AudacityLogo48x48.xpm"
#endif

#ifdef __WXMAC__
   #include <Carbon/Carbon.h>
#endif

enum {
   kID_RadioButton_BouncingBall = 10101,
   kID_RadioButton_Highlight,
};

BEGIN_EVENT_TABLE(LyricsWindow, wxFrame)
   EVT_CLOSE(LyricsWindow::OnCloseWindow)
   EVT_RADIOBUTTON(kID_RadioButton_BouncingBall, LyricsWindow::OnStyle_BouncingBall)
   EVT_RADIOBUTTON(kID_RadioButton_Highlight, LyricsWindow::OnStyle_Highlight)
END_EVENT_TABLE()

const wxSize gSize = wxSize(LYRICS_DEFAULT_WIDTH, LYRICS_DEFAULT_HEIGHT);

LyricsWindow::LyricsWindow(AudacityProject *parent)
   : wxFrame( &GetProjectFrame( *parent ), -1,
            wxString::Format(_("Audacity Karaoke%s"),
                              ((parent->GetProjectName().empty()) ?
                                 wxT("") :
                                 wxString::Format(
                                   wxT(" - %s"),
                                   parent->GetProjectName()))),
            wxPoint(100, 300), gSize,
            //v Bug in wxFRAME_FLOAT_ON_PARENT:
            // If both the project frame and LyricsWindow are minimized and you restore LyricsWindow,
            // you can't restore project frame until you close LyricsWindow, but then project frame and
            // LyricsWindow are restored but LyricsWindow is unresponsive because it thinks it's not shown.
            //    wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT)
            wxDEFAULT_FRAME_STYLE)
{
   //vvv Still necessary? It's commented out in ToolManager and Meter, so I did so here.
   //   #ifdef __WXMAC__
   //      // WXMAC doesn't support wxFRAME_FLOAT_ON_PARENT, so we do
   //      SetWindowClass((WindowRef) MacGetWindowRef(), kFloatingWindowClass);
   //   #endif
   mProject = parent;

   // loads either the XPM or the windows resource, depending on the platform
#if !defined(__WXMAC__) && !defined(__WXX11__)
   {
#ifdef __WXMSW__
      wxIcon ic{ wxICON(AudacityLogo) };
#else
      wxIcon ic{wxICON(AudacityLogo48x48)};
#endif
      SetIcon(ic);
   }
   #endif

   wxPoint panelPos(0, 0);
   wxSize panelSize = gSize;

   //vvv not yet working right in ported version, so choice is disabled.
   // It seems when you select highlight style, the TrackPanel timer stops working, but
   // going back to bouncing ball style starts it up again (!!!), per breakpoints in TrackPanel::OnTimer().
   //
   //wxToolBar* pToolBar = this->CreateToolBar();
   //const int kHorizMargin = 8;
   //wxASSERT(pToolBar); // To justify safenew
   //wxRadioButton* pRadioButton_BouncingBall =
   //   safenew wxRadioButton(pToolBar, kID_RadioButton_BouncingBall, _("Bouncing Ball"), wxPoint(kHorizMargin, 4),
   //       wxDefaultSize, wxRB_GROUP);
   //// Reposition to center vertically.
   //wxSize tbSize = pToolBar->GetSize();
   //wxSize btnSize = pRadioButton_BouncingBall->GetSize();
   //int top = (tbSize.GetHeight() - btnSize.GetHeight()) / 2;
   //pRadioButton_BouncingBall->Move(kHorizMargin, top);
   //pToolBar->AddControl(pRadioButton_BouncingBall);
   //
   //int left = kHorizMargin + btnSize.GetWidth() + kHorizMargin; //vvv Doesn't actually work. Probably need sizers.
   //wxRadioButton* pRadioButton_Highlight =
   //   safenew wxRadioButton(pToolBar, kID_RadioButton_Highlight, _("Highlight"), wxPoint(left, top));
   //pToolBar->AddControl(pRadioButton_Highlight);
   //
   //panelPos.x += tbSize.GetHeight();
   //panelSize.y -= tbSize.GetHeight();
   //
   //#if defined(__WXMAC__)
   //   wxColour face = wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE);
   //   pRadioButton_BouncingBall->SetBackgroundColour(face);
   //   pRadioButton_Highlight->SetBackgroundColour(face);
   //#endif
   //
   //pToolBar->Realize();

   mLyricsPanel = safenew LyricsPanel(this, -1, parent, panelPos, panelSize);
   RTL_WORKAROUND(mLyricsPanel);

   //vvv Highlight style is broken in ported version.
   //switch (mLyricsPanel->GetLyricsStyle())
   //{
   //   case LyricsPanel::kBouncingBallLyrics:
   //      pRadioButton_BouncingBall->SetValue(true); break;
   //   case LyricsPanel::kHighlightLyrics:
   //   default:
   //      pRadioButton_Highlight->SetValue(true); break;
   //}

   // Events from the project don't propagate directly to this other frame, so...
   mProject->Bind(EVT_TRACK_PANEL_TIMER,
      &LyricsWindow::OnTimer,
      this);
   Center();
}

void LyricsWindow::OnCloseWindow(wxCloseEvent & WXUNUSED(event))
{
  this->Hide();
}

void LyricsWindow::OnStyle_BouncingBall(wxCommandEvent & WXUNUSED(event))
{
   mLyricsPanel->SetLyricsStyle(LyricsPanel::kBouncingBallLyrics);
}

void LyricsWindow::OnStyle_Highlight(wxCommandEvent & WXUNUSED(event))
{
   mLyricsPanel->SetLyricsStyle(LyricsPanel::kHighlightLyrics);
}

void LyricsWindow::OnTimer(wxCommandEvent &event)
{
   if (ProjectAudioIO::Get( *mProject ).IsAudioActive())
   {
      GetLyricsPanel()->Update(gAudioIO->GetStreamTime());
   }
   else
   {
      // Reset lyrics display.
      const auto &selectedRegion = ViewInfo::Get( *mProject ).selectedRegion;
      GetLyricsPanel()->Update(selectedRegion.t0());
   }

   // Let other listeners get the notification
   event.Skip();
}
