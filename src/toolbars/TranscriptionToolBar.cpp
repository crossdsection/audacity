/**********************************************************************

  Audacity: A Digital Audio Editor

  TranscriptionToolBar.cpp

  Shane T. Mueller
  Leland Lucius

*******************************************************************//**

\class TranscriptionToolBar
\brief A kind of ToolBar used to help with analysing voice recordings.

*//*******************************************************************/

#include "../Audacity.h"
#include "TranscriptionToolBar.h"
#include "ToolManager.h"

#include "../Experimental.h"

// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/choice.h>
#include <wx/defs.h>
#include <wx/brush.h>
#include <wx/image.h>
#include <wx/intl.h>
#endif // WX_PRECOMP

#include "../Envelope.h"

#include "ControlToolBar.h"
#include "../AllThemeResources.h"
#include "../AudioIO.h"
#include "../ImageManipulation.h"
#include "../KeyboardCapture.h"
#include "../Project.h"
#include "../ProjectManager.h"
#include "../TimeTrack.h"
#include "../ViewInfo.h"
#include "../WaveTrack.h"
#include "../widgets/AButton.h"
#include "../widgets/ASlider.h"
#include "../tracks/ui/Scrubbing.h"
#include "../Prefs.h"

#ifdef EXPERIMENTAL_VOICE_DETECTION
#include "../VoiceKey.h"
#endif

IMPLEMENT_CLASS(TranscriptionToolBar, ToolBar);

///////////////////////////////////////////
///  Methods for TranscriptionToolBar
///////////////////////////////////////////


BEGIN_EVENT_TABLE(TranscriptionToolBar, ToolBar)
   EVT_CHAR(TranscriptionToolBar::OnKeyEvent)
   EVT_COMMAND(wxID_ANY, EVT_CAPTURE_KEY, TranscriptionToolBar::OnCaptureKey)

   EVT_COMMAND_RANGE(TTB_PlaySpeed, TTB_PlaySpeed,
                     wxEVT_COMMAND_BUTTON_CLICKED, TranscriptionToolBar::OnPlaySpeed)
   EVT_SLIDER(TTB_PlaySpeedSlider, TranscriptionToolBar::OnSpeedSlider)

#ifdef EXPERIMENTAL_VOICE_DETECTION
   EVT_COMMAND_RANGE(TTB_StartOn, TTB_StartOn,
                     wxEVT_COMMAND_BUTTON_CLICKED, TranscriptionToolBar::OnStartOn)
   EVT_COMMAND_RANGE(TTB_StartOff, TTB_StartOff,
                     wxEVT_COMMAND_BUTTON_CLICKED, TranscriptionToolBar::OnStartOff)
   EVT_COMMAND_RANGE(TTB_EndOn, TTB_EndOn,
                     wxEVT_COMMAND_BUTTON_CLICKED, TranscriptionToolBar::OnEndOn)
   EVT_COMMAND_RANGE(TTB_EndOff, TTB_EndOff,
                     wxEVT_COMMAND_BUTTON_CLICKED, TranscriptionToolBar::OnEndOff)
   EVT_COMMAND_RANGE(TTB_SelectSound, TTB_SelectSound,
                     wxEVT_COMMAND_BUTTON_CLICKED, TranscriptionToolBar::OnSelectSound)
   EVT_COMMAND_RANGE(TTB_SelectSilence, TTB_SelectSilence,
                     wxEVT_COMMAND_BUTTON_CLICKED, TranscriptionToolBar::OnSelectSilence)
   EVT_COMMAND_RANGE(TTB_AutomateSelection, TTB_AutomateSelection,
                     wxEVT_COMMAND_BUTTON_CLICKED, TranscriptionToolBar::OnAutomateSelection)
   EVT_COMMAND_RANGE(TTB_MakeLabel, TTB_MakeLabel,
                     wxEVT_COMMAND_BUTTON_CLICKED, TranscriptionToolBar::OnMakeLabel)
   EVT_COMMAND_RANGE(TTB_Calibrate, TTB_Calibrate,
                     wxEVT_COMMAND_BUTTON_CLICKED, TranscriptionToolBar::OnCalibrate)
   EVT_SLIDER(TTB_SensitivitySlider, TranscriptionToolBar::OnSensitivitySlider)

   EVT_CHOICE(TTB_KeyType, TranscriptionToolBar::SetKeyType)
#endif
END_EVENT_TABLE()
   ;   //semicolon enforces  proper automatic indenting in emacs.

////Standard Constructor
TranscriptionToolBar::TranscriptionToolBar()
: ToolBar(TranscriptionBarID, _("Play-at-Speed"), wxT("Transcription"),true)
{
   mPlaySpeed = 1.0 * 100.0;
#ifdef EXPERIMENTAL_VOICE_DETECTION
   mVk = std::make_unique<VoiceKey>();
#endif
}

TranscriptionToolBar::~TranscriptionToolBar()
{
}

TranscriptionToolBar &TranscriptionToolBar::Get( AudacityProject &project )
{
   auto &toolManager = ToolManager::Get( project );
   return *static_cast<TranscriptionToolBar*>( toolManager.GetToolBar(TranscriptionBarID) );
}

const TranscriptionToolBar &TranscriptionToolBar::Get( const AudacityProject &project )
{
   return Get( const_cast<AudacityProject&>( project )) ;
}

void TranscriptionToolBar::Create(wxWindow * parent)
{
   ToolBar::Create(parent);

   mBackgroundHeight = 0;
   mBackgroundWidth = 0;

#ifdef EXPERIMENTAL_VOICE_DETECTION
   mButtons[TTB_StartOn]->Disable();
   mButtons[TTB_StartOff]->Disable();
   mButtons[TTB_EndOn]->Disable();
   mButtons[TTB_EndOff]->Disable();
   mButtons[TTB_SelectSound]->Disable();
   mButtons[TTB_SelectSilence]->Disable();
   mButtons[TTB_Calibrate]->Enable();
   mButtons[TTB_AutomateSelection]->Disable();
   mButtons[TTB_MakeLabel]->Enable();
#endif

   //Old code...
   //Process a dummy event to set up mPlaySpeed
   //wxCommandEvent dummy;
   //OnSpeedSlider(dummy);

   //JKC: Set speed this way is better, as we don't
   //then stop Audio if it is playing, so we can be playing
   //audio and open a second project.
   mPlaySpeed = (mPlaySpeedSlider->Get()) * 100;

   // Simulate a size event to set initial placement/size
   wxSizeEvent event(GetSize(), GetId());
   event.SetEventObject(this);
   GetEventHandler()->ProcessEvent(event);
}

/// This is a convenience function that allows for button creation in
/// MakeButtons() with fewer arguments
/// Very similar to code in ControlToolBar...
AButton *TranscriptionToolBar::AddButton(
   TranscriptionToolBar *pBar,
   teBmps eFore, teBmps eDisabled,
   int id,
   const wxChar *label)
{
   AButton *&r = pBar->mButtons[id];

   r = ToolBar::MakeButton(pBar,
      bmpRecoloredUpSmall, bmpRecoloredDownSmall, bmpRecoloredUpHiliteSmall,bmpRecoloredHiliteSmall,
      eFore, eFore, eDisabled,
      wxWindowID(id),
      wxDefaultPosition,
      false,
      theTheme.ImageSize( bmpRecoloredUpSmall ));

   r->SetLabel(label);
// JKC: Unlike ControlToolBar, does not have a focus rect.  Shouldn't it?
// r->SetFocusRect( r->GetRect().Deflate( 4, 4 ) );

   pBar->Add( r, 0, wxALIGN_CENTER );

   return r;
}

void TranscriptionToolBar::MakeAlternateImages(
   teBmps eFore, teBmps eDisabled,
   int id, unsigned altIdx)
{
   ToolBar::MakeAlternateImages(*mButtons[id], altIdx,
      bmpRecoloredUpSmall, bmpRecoloredDownSmall, bmpRecoloredUpHiliteSmall,bmpRecoloredHiliteSmall,
      eFore, eFore, eDisabled,
      theTheme.ImageSize( bmpRecoloredUpSmall ));
}

void TranscriptionToolBar::Populate()
{
   SetBackgroundColour( theTheme.Colour( clrMedium  ) );
// Very similar to code in ControlToolBar...
// Very similar to code in EditToolBar
   MakeButtonBackgroundsSmall();

   AddButton(this, bmpPlay,     bmpPlayDisabled,   TTB_PlaySpeed,
      _("Play at selected speed"));
   MakeAlternateImages(bmpLoop, bmpLoopDisabled, TTB_PlaySpeed, 1);
   MakeAlternateImages(bmpCutPreview, bmpCutPreviewDisabled, TTB_PlaySpeed, 2);
   mButtons[TTB_PlaySpeed]->FollowModifierKeys();

   //Add a slider that controls the speed of playback.
   const int SliderWidth=100;
   mPlaySpeedSlider = safenew ASlider(this,
      TTB_PlaySpeedSlider,
      _("Playback Speed"),
      wxDefaultPosition,
      wxSize(SliderWidth,25),
      ASlider::Options{}
         .Style( SPEED_SLIDER )
         //  6 steps using page up/down, and 60 using arrow keys
         .Line( 0.16667f )
         .Page( 1.6667f )
   );
   mPlaySpeedSlider->SetSizeHints(wxSize(100, 25), wxSize(2000, 25));
   mPlaySpeedSlider->Set(mPlaySpeed / 100.0);
   mPlaySpeedSlider->SetLabel(_("Playback Speed"));
   Add( mPlaySpeedSlider, 1, wxALIGN_CENTER );
   mPlaySpeedSlider->Bind(wxEVT_SET_FOCUS,
                 &TranscriptionToolBar::OnFocus,
                 this);
   mPlaySpeedSlider->Bind(wxEVT_KILL_FOCUS,
                 &TranscriptionToolBar::OnFocus,
                 this);

#ifdef EXPERIMENTAL_VOICE_DETECTION
// If we need these strings translated, then search and replace
// TRANSLATBLE by _ and remove this #define.
#define TRANSLATABLE( x ) wxT( x )
   AddButton(this, bmpTnStartOn,     bmpTnStartOnDisabled,  TTB_StartOn,
      TRANSLATABLE("Adjust left selection to next onset"));
   AddButton(this, bmpTnEndOn,       bmpTnEndOnDisabled,   TTB_EndOn,
      TRANSLATABLE("Adjust right selection to previous offset"));
   AddButton(this, bmpTnStartOff,    bmpTnStartOffDisabled,  TTB_StartOff,
      TRANSLATABLE("Adjust left selection to next offset"));
   AddButton(this, bmpTnEndOff,      bmpTnEndOffDisabled,    TTB_EndOff,
      TRANSLATABLE("Adjust right selection to previous onset"));
   AddButton(this, bmpTnSelectSound, bmpTnSelectSoundDisabled, TTB_SelectSound,
      TRANSLATABLE("Select region of sound around cursor"));
   AddButton(this, bmpTnSelectSilence, bmpTnSelectSilenceDisabled, TTB_SelectSilence,
      TRANSLATABLE("Select region of silence around cursor"));
   AddButton(this, bmpTnAutomateSelection,   bmpTnAutomateSelectionDisabled,  TTB_AutomateSelection,
      TRANSLATABLE("Automatically make labels from words"));
   AddButton(this, bmpTnMakeTag, bmpTnMakeTagDisabled,  TTB_MakeLabel,
      TRANSLATABLE("Add label at selection"));
   AddButton(this, bmpTnCalibrate, bmpTnCalibrateDisabled, TTB_Calibrate,
      TRANSLATABLE("Calibrate voicekey"));

   mSensitivitySlider = safenew ASlider(this,
                                    TTB_SensitivitySlider,
                                    TRANSLATABLE("Adjust Sensitivity"),
                                    wxDefaultPosition,
                                    wxSize(SliderWidth,25),
                                    SPEED_SLIDER);
   mSensitivitySlider->Set(.5);
   mSensitivitySlider->SetLabel(TRANSLATABLE("Sensitivity"));
   Add( mSensitivitySlider, 0, wxALIGN_CENTER );

   wxString choices[] =
   {
      TRANSLATABLE("Energy"),
      TRANSLATABLE("Sign Changes (Low Threshold)"),
      TRANSLATABLE("Sign Changes (High Threshold)"),
      TRANSLATABLE("Direction Changes (Low Threshold)"),
      TRANSLATABLE("Direction Changes (High Threshold)")
   };

   mKeyTypeChoice = safenew wxChoice(this, TTB_KeyType,
                                 wxDefaultPosition,
                                 wxDefaultSize,
                                 5,
                                 choices );
   mKeyTypeChoice->SetName(TRANSLATABLE("Key type"));
   mKeyTypeChoice->SetSelection(0);
   Add( mKeyTypeChoice, 0, wxALIGN_CENTER );
#endif

   // Add a little space
   Add(2, -1);

   UpdatePrefs();
}

void TranscriptionToolBar::EnableDisableButtons()
{
#ifdef EXPERIMENTAL_VOICE_DETECTION
   AudacityProject *p = GetActiveProject();
   if (!p) return;
   // Is anything selected?
   auto selection = p->GetSel0() < p->GetSel1() && p->GetTracks()->Selected();

   mButtons[TTB_Calibrate]->SetEnabled(selection);
#endif
}

void TranscriptionToolBar::UpdatePrefs()
{
   RegenerateTooltips();

   // Set label to pull in language change
   SetLabel(_("Play-at-Speed"));

   // Give base class a chance
   ToolBar::UpdatePrefs();
}

void TranscriptionToolBar::RegenerateTooltips()
{
   // We could also mention the shift- and ctrl-modified versions in the
   // tool tip... but it would get long

   static const struct Entry {
      int tool;
      CommandID commandName;
      wxString untranslatedLabel;
      CommandID commandName2;
      wxString untranslatedLabel2;
   } table[] = {
      { TTB_PlaySpeed,   wxT("PlayAtSpeed"),    XO("Play-at-Speed"),
      wxT("PlayAtSpeedLooped"),    XO("Looped-Play-at-Speed")
      },
   };

   for (const auto &entry : table) {
      TranslatedInternalString commands[] = {
         { entry.commandName,  wxGetTranslation(entry.untranslatedLabel)  },
         { entry.commandName2, wxGetTranslation(entry.untranslatedLabel2) },
      };
      ToolBar::SetButtonToolTip( *mButtons[entry.tool], commands, 2u );
   }

#ifdef EXPERIMENTAL_VOICE_DETECTION
   mButtons[TTB_StartOn]->SetToolTip(TRANSLATABLE("Left-to-On"));
   mButtons[TTB_EndOn]->SetToolTip(   TRANSLATABLE("Right-to-Off"));
   mButtons[TTB_StartOff]->SetToolTip(   TRANSLATABLE("Left-to-Off"));
   mButtons[TTB_EndOff]->SetToolTip(   TRANSLATABLE("Right-to-On"));
   mButtons[TTB_SelectSound]->SetToolTip(   TRANSLATABLE("Select-Sound"));
   mButtons[TTB_SelectSilence]->SetToolTip(   TRANSLATABLE("Select-Silence"));
   mButtons[TTB_AutomateSelection]->SetToolTip(   TRANSLATABLE("Make Labels"));
   mButtons[TTB_MakeLabel]->SetToolTip(   TRANSLATABLE("Add Label"));
   mButtons[TTB_Calibrate]->SetToolTip(   TRANSLATABLE("Calibrate"));

   mSensitivitySlider->SetToolTip(TRANSLATABLE("Sensitivity"));
   mKeyTypeChoice->SetToolTip(TRANSLATABLE("Key type"));
#endif
}

void TranscriptionToolBar::OnFocus(wxFocusEvent &event)
{
   KeyboardCapture::OnFocus( *this, event );
}

void TranscriptionToolBar::OnCaptureKey(wxCommandEvent &event)
{
   wxKeyEvent *kevent = (wxKeyEvent *)event.GetEventObject();
   int keyCode = kevent->GetKeyCode();

   // Pass LEFT/RIGHT/UP/DOWN/PAGEUP/PAGEDOWN through for input/output sliders
   if (FindFocus() == mPlaySpeedSlider && (keyCode == WXK_LEFT || keyCode == WXK_RIGHT
                                    || keyCode == WXK_UP || keyCode == WXK_DOWN
                                    || keyCode == WXK_PAGEUP || keyCode == WXK_PAGEDOWN)) {
      return;
   }

   event.Skip();

   return;
}

//This handles key-stroke events????
void TranscriptionToolBar::OnKeyEvent(wxKeyEvent & event)
{
   if (event.ControlDown()) {
      event.Skip();
      return;
   }

   if (event.GetKeyCode() == WXK_SPACE) {
      if (gAudioIO->IsBusy()) {
         /*Do Stuff Here*/
      }
      else {
         /*Do other stuff Here*/
      }
   }
}



//This changes the state of the various buttons
void TranscriptionToolBar::SetButton(bool down, AButton* button)
{
   if (down) {
      button->PushDown();
   }
   else {
      button->PopUp();
   }
}

void TranscriptionToolBar::GetSamples(
   const WaveTrack *t, sampleCount *s0, sampleCount *slen)
{
   // GetSamples attempts to translate the start and end selection markers into sample indices
   // These selection numbers are doubles.

   AudacityProject *p = GetActiveProject();
   if (!p) {
      return;
   }

   //First, get the current selection. It is part of the mViewInfo, which is
   //part of the project

   const auto &selectedRegion = ViewInfo::Get( *p ).selectedRegion;
   double start = selectedRegion.t0();
   double end = selectedRegion.t1();

   auto ss0 = sampleCount( (start - t->GetOffset()) * t->GetRate() );
   auto ss1 = sampleCount( (end - t->GetOffset()) * t->GetRate() );

   if (start < t->GetOffset()) {
      ss0 = 0;
   }

#if 0
   //This adjusts the right samplecount to the maximum sample.
   if (ss1 >= t->GetNumSamples()) {
      ss1 = t->GetNumSamples();
   }
#endif

   if (ss1 < ss0) {
      ss1 = ss0;
   }

   *s0 = ss0;
   *slen = ss1 - ss0;
}

// Come here from button clicks, or commands
void TranscriptionToolBar::PlayAtSpeed(bool looped, bool cutPreview)
{
   // Can't do anything without an active project
   AudacityProject * p = GetActiveProject();
   if (!p) {
      return;
   }

   // Fixed speed play is the old method, that uses a time track.
   // VariSpeed play reuses Scrubbing.
   bool bFixedSpeedPlay = !gPrefs->ReadBool(wxT("/AudioIO/VariSpeedPlay"), true);
   // Scrubbing doesn't support note tracks, but the fixed-speed method using time tracks does.
   if ( TrackList::Get( *p ).Any< NoteTrack >() )
      bFixedSpeedPlay = true;

   // Scrubbing only supports straight through play.
   // So if looped or cutPreview, we have to fall back to fixed speed.
   bFixedSpeedPlay = bFixedSpeedPlay || looped || cutPreview;
   if (bFixedSpeedPlay)
   {
      // Create a TimeTrack if we haven't done so already
      if (!mTimeTrack) {
         mTimeTrack = TrackFactory::Get( *p ).NewTimeTrack();
         if (!mTimeTrack) {
            return;
         }
      }
      // Set the speed range
      //mTimeTrack->SetRangeUpper((double)mPlaySpeed / 100.0);
      //mTimeTrack->SetRangeLower((double)mPlaySpeed / 100.0);
      mTimeTrack->GetEnvelope()->Flatten((double)mPlaySpeed / 100.0);
   }

   // Pop up the button
   SetButton(false, mButtons[TTB_PlaySpeed]);

   // If IO is busy, abort immediately
   if (gAudioIO->IsBusy()) {
      auto &bar = ControlToolBar::Get( *p );
      bar.StopPlaying();
   }

   // Get the current play region
   const auto &viewInfo = ViewInfo::Get( *p );
   const auto &playRegion = viewInfo.playRegion;

   // Start playing
   if (playRegion.GetStart() < 0)
      return;
   if (bFixedSpeedPlay)
   {
      auto options = DefaultPlayOptions( *p );
      options.playLooped = looped;
      // No need to set cutPreview options.
      options.timeTrack = mTimeTrack.get();
      auto mode =
         cutPreview ? PlayMode::cutPreviewPlay
         : options.playLooped ? PlayMode::loopedPlay
         : PlayMode::normalPlay;
      auto &bar = ControlToolBar::Get( *p );
      bar.PlayPlayRegion(
         SelectedRegion(playRegion.GetStart(), playRegion.GetEnd()),
            options,
            mode);
   }
   else
   {
      auto &scrubber = Scrubber::Get( *p );
      scrubber.StartSpeedPlay(GetPlaySpeed(),
         playRegion.GetStart(), playRegion.GetEnd());
   }
}

// Come here from button clicks only
void TranscriptionToolBar::OnPlaySpeed(wxCommandEvent & WXUNUSED(event))
{
   auto button = mButtons[TTB_PlaySpeed];

   // Let control have precedence over shift
   const bool cutPreview = mButtons[TTB_PlaySpeed]->WasControlDown();
   const bool looped = !cutPreview &&
      button->WasShiftDown();
   PlayAtSpeed(looped, cutPreview);
}

void TranscriptionToolBar::OnSpeedSlider(wxCommandEvent& WXUNUSED(event))
{
   mPlaySpeed = (mPlaySpeedSlider->Get()) * 100;
   RegenerateTooltips();

   // If IO is busy, abort immediately
   // AWD: This is disabled to work around a hang on Linux when PulseAudio is
   // used.  If we figure that one out we can re-enable this code.
   //if (gAudioIO->IsBusy()) {
   //   OnPlaySpeed(event);
   //}
}

#ifdef EXPERIMENTAL_VOICE_DETECTION
void TranscriptionToolBar::OnStartOn(wxCommandEvent & WXUNUSED(event))
{
   //If IO is busy, abort immediately
   if (gAudioIO->IsBusy()){
      SetButton(false,mButtons[TTB_StartOn]);
      return;
   }

   mVk->AdjustThreshold(GetSensitivity());
   AudacityProject *p = GetActiveProject();

   auto t = *p->GetTracks()->Any< const WaveTrack >().begin();
   if(t ) {
      auto wt = static_cast<const WaveTrack*>(t);
      sampleCount start, len;
      GetSamples(wt, &start, &len);

      //Adjust length to end if selection is null
      //if(len == 0)
      //len = wt->GetSequence()->GetNumSamples()-start;

      auto newstart = mVk->OnForward(*wt, start, len);
      double newpos = newstart.as_double() / wt->GetRate();

      auto &selectedRegion = p->GetViewInfo().selectedRegion;
      selectedRegion.setT0( newpos );
      p->RedrawProject();

      SetButton(false, mButtons[TTB_StartOn]);
   }
}

void TranscriptionToolBar::OnStartOff(wxCommandEvent & WXUNUSED(event))
{
   //If IO is busy, abort immediately
   if (gAudioIO->IsBusy()){
      SetButton(false,mButtons[TTB_StartOff]);
      return;
   }
   mVk->AdjustThreshold(GetSensitivity());
   AudacityProject *p = GetActiveProject();

   SetButton(false, mButtons[TTB_StartOff]);
   auto t = *p->GetTracks()->Any< const WaveTrack >().begin();
   if(t) {
      auto wt = static_cast<const WaveTrack*>(t);
      sampleCount start, len;
      GetSamples(wt, &start, &len);

      //Adjust length to end if selection is null
      //if(len == 0)
      //len = wt->GetSequence()->GetNumSamples()-start;

      auto newstart = mVk->OffForward(*wt, start, len);
      double newpos = newstart.as_double() / wt->GetRate();

      auto &selectedRegion = p->GetViewInfo().selectedRegion;
      selectedRegion.setT0( newpos );
      p->RedrawProject();

      SetButton(false, mButtons[TTB_StartOn]);
   }
}

void TranscriptionToolBar::OnEndOn(wxCommandEvent & WXUNUSED(event))
{

   //If IO is busy, abort immediately
   if (gAudioIO->IsBusy()){
      SetButton(false,mButtons[TTB_EndOn]);
      return;
   }

   mVk->AdjustThreshold(GetSensitivity());
   AudacityProject *p = GetActiveProject();
   auto t = *p->GetTracks()->Any< const WaveTrack >().begin();
   if(t) {
      auto wt = static_cast<const WaveTrack*>(t);
      sampleCount start, len;
      GetSamples(wt, &start, &len);

      //Adjust length to end if selection is null
      if(len == 0)
         {
            len = start;
            start = 0;
         }
      auto newEnd = mVk->OnBackward(*wt, start + len, len);
      double newpos = newEnd.as_double() / wt->GetRate();

      p->SetSel1(newpos);
      p->RedrawProject();

      SetButton(false, mButtons[TTB_EndOn]);
   }
}



void TranscriptionToolBar::OnEndOff(wxCommandEvent & WXUNUSED(event))
{

   //If IO is busy, abort immediately
   if (gAudioIO->IsBusy()){
      SetButton(false,mButtons[TTB_EndOff]);
      return;
   }
   mVk->AdjustThreshold(GetSensitivity());
   AudacityProject *p = GetActiveProject();

   auto t = *p->GetTracks()->Any< const WaveTrack >().begin();
   if(t) {
      auto wt = static_cast<const WaveTrack*>(t);
      sampleCount start, len;
      GetSamples(wt, &start, &len);

      //Adjust length to end if selection is null
      if(len == 0) {
         len = start;
         start = 0;
      }
      auto newEnd = mVk->OffBackward(*wt, start + len, len);
      double newpos = newEnd.as_double() / wt->GetRate();

      p->SetSel1(newpos);
      p->RedrawProject();

      SetButton(false, mButtons[TTB_EndOff]);
   }
}



void TranscriptionToolBar::OnSelectSound(wxCommandEvent & WXUNUSED(event))
{

   //If IO is busy, abort immediately
   if (gAudioIO->IsBusy()){
      SetButton(false,mButtons[TTB_SelectSound]);
      return;
   }


   mVk->AdjustThreshold(GetSensitivity());
   AudacityProject *p = GetActiveProject();


   TrackList *tl = p->GetTracks();
   if(auto wt = *tl->Any<const WaveTrack>().begin()) {
      sampleCount start, len;
      GetSamples(wt, &start, &len);

      //Adjust length to end if selection is null
      //if(len == 0)
      //len = wt->GetSequence()->GetNumSamples()-start;

      double rate =  wt->GetRate();
      auto newstart = mVk->OffBackward(*wt, start, start);
      auto newend   =
      mVk->OffForward(*wt, start + len, (int)(tl->GetEndTime() * rate));

      //reset the selection bounds.
      auto &selectedRegion = p->GetViewInfo().selectedRegion;
      selectedRegion.setTimes(
         newstart.as_double() / rate, newend.as_double() /  rate );
      p->RedrawProject();

   }

   SetButton(false,mButtons[TTB_SelectSound]);
}

void TranscriptionToolBar::OnSelectSilence(wxCommandEvent & WXUNUSED(event))
{

   //If IO is busy, abort immediately
   if (gAudioIO->IsBusy()){
      SetButton(false,mButtons[TTB_SelectSilence]);
      return;
   }

   mVk->AdjustThreshold(GetSensitivity());
   AudacityProject *p = GetActiveProject();


   TrackList *tl = p->GetTracks();
   if(auto wt = *tl->Any<const WaveTrack>().begin()) {
      sampleCount start, len;
      GetSamples(wt, &start, &len);

      //Adjust length to end if selection is null
      //if(len == 0)
      //len = wt->GetSequence()->GetNumSamples()-start;
      double rate =  wt->GetRate();
      auto newstart = mVk->OnBackward(*wt, start, start);
      auto newend   =
      mVk->OnForward(*wt, start + len, (int)(tl->GetEndTime() * rate));

      //reset the selection bounds.
      p->SetSel0(newstart.as_double() /  rate);
      p->SetSel1(newend.as_double() / rate);
      p->RedrawProject();

   }

   SetButton(false,mButtons[TTB_SelectSilence]);

}



void TranscriptionToolBar::OnCalibrate(wxCommandEvent & WXUNUSED(event))
{
   //If IO is busy, abort immediately
   if (gAudioIO->IsBusy()){
      SetButton(false,mButtons[TTB_Calibrate]);
      return;
   }


   AudacityProject *p = GetActiveProject();

   TrackList *tl = p->GetTracks();
   if(auto wt = *tl->Any<const WaveTrack>().begin()) {
      sampleCount start, len;
      GetSamples(wt, &start, &len);

      mVk->CalibrateNoise(*wt, start, len);
      mVk->AdjustThreshold(3);

      mButtons[TTB_StartOn]->Enable();
      mButtons[TTB_StartOff]->Enable();
      mButtons[TTB_EndOn]->Enable();
      mButtons[TTB_EndOff]->Enable();
      //mThresholdSensitivity->Set(3);

      SetButton(false,mButtons[TTB_Calibrate]);
   }

   mButtons[TTB_StartOn]->Enable();
   mButtons[TTB_StartOff]->Enable();
   mButtons[TTB_EndOn]->Enable();
   mButtons[TTB_EndOff]->Enable();
   mButtons[TTB_SelectSound]->Enable();
   mButtons[TTB_SelectSilence]->Enable();
   mButtons[TTB_AutomateSelection]->Enable();

   //Make the sensititivy slider set the sensitivity by processing an event.
   wxCommandEvent dummy;
   OnSensitivitySlider(dummy);

}

//This automates selection through a selected region,
//selecting its best guess for words and creating labels at those points.

void TranscriptionToolBar::OnAutomateSelection(wxCommandEvent & WXUNUSED(event))
{


   //If IO is busy, abort immediately
   if (gAudioIO->IsBusy())
   {
      SetButton(false,mButtons[TTB_EndOff]);
      return;
   }

   wxBusyCursor busy;

   mVk->AdjustThreshold(GetSensitivity());
   AudacityProject *p = GetActiveProject();
   TrackList *tl = p->GetTracks();
   if(auto wt = *tl->Any<const WaveTrack>().begin()) {
      sampleCount start, len;
      GetSamples(wt, &start, &len);

      //Adjust length to end if selection is null
      if(len == 0)
      {
         len = start;
         start = 0;
      }
      sampleCount lastlen = 0;
      double newStartPos, newEndPos;

      //This is the minumum word size in samples (.05 is 50 ms)
      int minWordSize = (int)(wt->GetRate() * .05);

      //Continue until we have processed the entire
      //region, or we are making no progress.
      while(len > 0 && lastlen != len)
      {

         lastlen = len;

         auto newStart = mVk->OnForward(*wt, start, len);

         //JKC: If no start found then don't add any labels.
         if( newStart==start)
            break;

         //Adjust len by the NEW start position
         len -= (newStart - start);

         //Adjust len by the minimum word size
         len -= minWordSize;



         //OK, now we have found a NEW starting point.  A 'word' should be at least
         //50 ms long, so jump ahead minWordSize

         auto newEnd   =
         mVk->OffForward(*wt, newStart + minWordSize, len);

         //If newEnd didn't move, we should give up, because
         // there isn't another end before the end of the selection.
         if(newEnd == (newStart + minWordSize))
            break;


         //Adjust len by the NEW word end
         len -= (newEnd - newStart);

         //Calculate the start and end of the words, in seconds
         newStartPos = newStart.as_double() / wt->GetRate();
         newEndPos = newEnd.as_double() / wt->GetRate();


         //Increment
         start = newEnd;

         p->DoAddLabel(SelectedRegion(newStartPos, newEndPos));
         p->RedrawProject();
      }
      SetButton(false, mButtons[TTB_AutomateSelection]);
   }
}

void TranscriptionToolBar::OnMakeLabel(wxCommandEvent & WXUNUSED(event))
{
   AudacityProject *p = GetActiveProject();
   SetButton(false, mButtons[TTB_MakeLabel]);
   p->DoAddLabel(SelectedRegion(p->GetSel0(),  p->GetSel1()));
}

//This returns a double z-score between 0 and 10.
double TranscriptionToolBar::GetSensitivity()
{
   return (double)mSensitivity;
}

void TranscriptionToolBar::OnSensitivitySlider(wxCommandEvent & WXUNUSED(event))
{
   mSensitivity = (mSensitivitySlider->Get());
}

void TranscriptionToolBar::SetKeyType(wxCommandEvent & WXUNUSED(event))
{
   int value = mKeyTypeChoice->GetSelection();

   //Only use one key type at a time.
   switch(value)
      {
      case 0:
         mVk->SetKeyType(true,0,0,0,0);
         break;
      case 1:
         mVk->SetKeyType(0,true,0,0,0);
         break;
      case 2:
         mVk->SetKeyType(0,0,true,0,0);
         break;
      case 3:
         mVk->SetKeyType(0,0,0,true,0);
         break;
      case 4:
         mVk->SetKeyType(0,0,0,0,true);
         break;
      }

}
#endif

void TranscriptionToolBar::ShowPlaySpeedDialog()
{
   mPlaySpeedSlider->ShowDialog();
   mPlaySpeedSlider->Refresh();
   wxCommandEvent e;
   OnSpeedSlider(e);
}

void TranscriptionToolBar::SetEnabled(bool enabled)
{
   mButtons[TTB_PlaySpeed]->SetEnabled(enabled);
}

void TranscriptionToolBar::SetPlaying(bool down, bool looped, bool cutPreview)
{
   AButton *const button = mButtons[TTB_PlaySpeed];
   if (down) {
      button->SetAlternateIdx(cutPreview ? 2 : looped ? 1 : 0);
      button->PushDown();
   }
   else {
      button->SetAlternateIdx(0);
      button->PopUp();
   }
}

void TranscriptionToolBar::AdjustPlaySpeed(float adj)
{
   if (adj < 0) {
      mPlaySpeedSlider->Decrease(-adj);
   }
   else {
      mPlaySpeedSlider->Increase(adj);
   }
   wxCommandEvent e;
   OnSpeedSlider(e);
}

