/**********************************************************************

   Audacity - A Digital Audio Editor
   Copyright 1999-2018 Audacity Team
   License: wxWidgets

   James Crook

******************************************************************//**

\file GetInfoCommand.cpp
\brief Contains definitions for GetInfoCommand class.
This class now lists
- Commands
- Menus
- Tracks
- Clips
- Labels
- Boxes

*//*******************************************************************/

#include "../Audacity.h" // for USE_* macros
#include "GetInfoCommand.h"

#include "../Project.h"
#include "CommandManager.h"
#include "CommandTargets.h"
#include "../effects/EffectManager.h"
#include "../widgets/Overlay.h"
#include "../TrackPanel.h"
#include "../WaveClip.h"
#include "../ViewInfo.h"
#include "../WaveTrack.h"
#include "../LabelTrack.h"
#include "../Envelope.h"
#include "CommandContext.h"

#include "SelectCommand.h"
#include "../ShuttleGui.h"
#include "CommandContext.h"

#include "../prefs/PrefsDialog.h"
#include "../Shuttle.h"
#include "../PluginManager.h"
#include "../ShuttleGui.h"

#include <wx/frame.h>
#include <wx/menu.h>

enum {
   kCommands,
   //kCommandsPlus,
   kMenus,
   kPreferences,
   kTracks,
   kClips,
   kEnvelopes,
   kLabels,
   kBoxes,
   nTypes
};

static const EnumValueSymbol kTypes[nTypes] =
{
   { XO("Commands") },
   //{ wxT("CommandsPlus"), XO("Commands Plus") },
   { XO("Menus") },
   { XO("Preferences") },
   { XO("Tracks") },
   { XO("Clips") },
   { XO("Envelopes") },
   { XO("Labels") },
   { XO("Boxes") },
};

enum {
   kJson,
   kLisp,
   kBrief,
   nFormats
};

static const EnumValueSymbol kFormats[nFormats] =
{
   // These are acceptable dual purpose internal/visible names
   
   /* i18n-hint JavaScript Object Notation */
   { XO("JSON") },
   /* i18n-hint name of a computer programming language */
   { XO("LISP") },
   { XO("Brief") }
};



bool GetInfoCommand::DefineParams( ShuttleParams & S ){
   S.DefineEnum( mInfoType, wxT("Type"), 0, kTypes, nTypes );
   S.DefineEnum( mFormat, wxT("Format"), 0, kFormats, nFormats );
   return true;
}

void GetInfoCommand::PopulateOrExchange(ShuttleGui & S)
{
   S.AddSpace(0, 5);

   S.StartMultiColumn(2, wxALIGN_CENTER);
   {
      S.TieChoice( _("Type:"),
         mInfoType, LocalizedStrings( kTypes, nTypes ));
      S.TieChoice( _("Format:"),
         mFormat, LocalizedStrings( kFormats, nFormats ));
   }
   S.EndMultiColumn();
}

bool GetInfoCommand::Apply(const CommandContext &context)
{
   if( mFormat == kJson )
      return ApplyInner( context );

   if( mFormat == kLisp )
   {
      CommandContext LispyContext( 
         context.project,
         std::make_unique<LispifiedCommandOutputTargets>( *context.pOutput.get() )
         );
      return ApplyInner( LispyContext );
   }

   if( mFormat == kBrief )
   {
      CommandContext BriefContext( 
         context.project,
         std::make_unique<BriefCommandOutputTargets>( *context.pOutput.get() )
         );
      return ApplyInner( BriefContext );
   }

   return false;
}

bool GetInfoCommand::ApplyInner(const CommandContext &context)
{
   switch( mInfoType  ){
      case kCommands     : return SendCommands( context, 0 );
      //case kCommandsPlus : return SendCommands( context, 1 );
      case kMenus        : return SendMenus( context );
      case kPreferences  : return SendPreferences( context );
      case kTracks       : return SendTracks( context );
      case kClips        : return SendClips( context );
      case kEnvelopes    : return SendEnvelopes( context );
      case kLabels       : return SendLabels( context );
      case kBoxes        : return SendBoxes( context );
      default:
         context.Status( "Command options not recognised" );
   }
   return false;
}

bool GetInfoCommand::SendMenus(const CommandContext &context)
{
   wxMenuBar * pBar = GetProjectFrame( context.project ).GetMenuBar();
   if(!pBar ){
      wxLogDebug("No menus");
      return false;
   }

   size_t cnt = pBar->GetMenuCount();
   size_t i;
   wxString Label;
   context.StartArray();
   for(i=0;i<cnt;i++)
   {
      Label = pBar->GetMenuLabelText( i );
      context.StartStruct();
      context.AddItem( 0, "depth" );
      context.AddItem( 0, "flags" );
      context.AddItem( Label, "label" );
      context.AddItem( "", "accel" );
      context.EndStruct();
      ExploreMenu( context, pBar->GetMenu( i ), pBar->GetId(), 1 );
   }
   context.EndArray();
   return true;
}

namespace {

/**************************************************************************//**
\brief Shuttle that retrieves a JSON format definition of a command's parameters.
********************************************************************************/
class ShuttleGuiGetDefinition : public ShuttleGui, public CommandMessageTargetDecorator
{
public:
   ShuttleGuiGetDefinition(wxWindow * pParent,CommandMessageTarget & target );
   virtual ~ShuttleGuiGetDefinition();

   wxCheckBox * TieCheckBox(
      const wxString &Prompt,
      const wxString &SettingName,
      const bool bDefault) override;
   wxCheckBox * TieCheckBoxOnRight(
      const wxString &Prompt,
      const wxString &SettingName,
      const bool bDefault) override;
   wxChoice * TieChoice(
      const wxString &Prompt,
      const wxString &SettingName,
      const wxString &Default,
      const wxArrayStringEx &Choices,
      const wxArrayStringEx & InternalChoices ) override;

   // An assertion will be violated if this override is reached!
   wxChoice * TieChoice(
      const wxString &Prompt,
      const wxString &SettingName,
      const int Default,
      const wxArrayStringEx & Choices,
      const std::vector<int> & InternalChoices) override;

   wxChoice * TieNumberAsChoice(
      const wxString &Prompt,
      const wxString &SettingName,
      const int Default,
      const wxArrayStringEx & Choices,
      const std::vector<int> & InternalChoices) override;

   wxTextCtrl * TieTextBox(
      const wxString &Prompt,
      const wxString &SettingName,
      const wxString &Default,
      const int nChars) override;
   wxTextCtrl * TieNumericTextBox(
      const wxString & Prompt,
      const wxString & SettingName,
      const double & Default,
      const int nChars) override;
   wxSlider * TieSlider(
      const wxString & Prompt,
      const wxString & SettingName,
      const int iDefault,
      const int max,
      const int min = 0) override;
   wxSpinCtrl * TieSpinCtrl(
      const wxString &Prompt,
      const wxString &SettingName,
      const int Value,
      const int max,
      const int min) override;
};

ShuttleGuiGetDefinition::ShuttleGuiGetDefinition(
   wxWindow * pParent,CommandMessageTarget & target )
: ShuttleGui( pParent, eIsGettingMetadata ),
  CommandMessageTargetDecorator( target )
{

}
ShuttleGuiGetDefinition::~ShuttleGuiGetDefinition(void)
{
}

wxCheckBox * ShuttleGuiGetDefinition::TieCheckBox(
   const wxString &Prompt,
   const wxString &SettingName,
   const bool bDefault) 
{ 
   StartStruct();
   AddItem( SettingName, "id" );
   AddItem( Prompt, "prompt" );
   AddItem( "bool", "type" );
   AddBool( bDefault, "default"  );
   EndStruct();
   return ShuttleGui::TieCheckBox( Prompt, SettingName, bDefault );
}
wxCheckBox * ShuttleGuiGetDefinition::TieCheckBoxOnRight(
   const wxString &Prompt,
   const wxString &SettingName,
   const bool bDefault) 
{
   StartStruct();
   AddItem( SettingName, "id" );
   AddItem( Prompt, "prompt" );
   AddItem( "bool", "type" );
   AddBool( bDefault, "default"  );
   EndStruct();
   return ShuttleGui::TieCheckBoxOnRight( Prompt, SettingName, bDefault );
}
wxChoice * ShuttleGuiGetDefinition::TieChoice(
   const wxString &Prompt,
   const wxString &SettingName,
   const wxString &Default,
   const wxArrayStringEx &Choices,
   const wxArrayStringEx & InternalChoices )
{
   StartStruct();
   AddItem( SettingName, "id" );
   AddItem( Prompt, "prompt" );
   AddItem( "enum", "type" );
   AddItem( Default, "default"  );
   StartField( "enum" );
   StartArray();
   for( size_t i=0;i<Choices.size(); i++ )
      AddItem( InternalChoices[i] );
   EndArray();
   EndField();
   EndStruct();
   return ShuttleGui::TieChoice( Prompt, SettingName, Default, Choices, InternalChoices );
}
wxChoice * ShuttleGuiGetDefinition::TieChoice(
   const wxString &Prompt,
   const wxString &SettingName,
   const int Default,
   const wxArrayStringEx & Choices,
   const std::vector<int> & InternalChoices)
{
   // Should no longer come here!
   // Choice controls in Preferences that really are exhaustive choices among
   // non-numerical options must now encode the internal choices as strings,
   // not numbers.
   wxASSERT(false);

   // But if we do get here anyway, proceed sub-optimally as before.
   StartStruct();
   AddItem( SettingName, "id" );
   AddItem( Prompt, "prompt" );
   AddItem( "enum", "type" );
   AddItem( Default, "default"  );
   StartField( "enum" );
   StartArray();
   for( size_t i=0;i<Choices.size(); i++ )
      AddItem( Choices[i] );
   EndArray();
   EndField();
   EndStruct();
   return ShuttleGui::TieChoice( Prompt, SettingName, Default, Choices, InternalChoices );
}
wxChoice * ShuttleGuiGetDefinition::TieNumberAsChoice(
   const wxString &Prompt,
   const wxString &SettingName,
   const int Default,
   const wxArrayStringEx & Choices,
   const std::vector<int> & InternalChoices)
{
   // Come here for controls that present non-exhaustive choices among some
   //  numbers, with an associated control that allows arbitrary entry of an
   // "Other..."
   StartStruct();
   AddItem( SettingName, "id" );
   AddItem( Prompt, "prompt" );
   AddItem( "number", "type" ); // not "enum" !
   AddItem( Default, "default"  );
   EndStruct();
   return ShuttleGui::TieNumberAsChoice(
      Prompt, SettingName, Default, Choices, InternalChoices );
}
wxTextCtrl * ShuttleGuiGetDefinition::TieTextBox(
   const wxString &Prompt,
   const wxString &SettingName,
   const wxString &Default,
   const int nChars) 
{
   StartStruct();
   AddItem( SettingName, "id" );
   AddItem( Prompt, "prompt" );
   AddItem( "string", "type" );
   AddItem( Default, "default"  );
   EndStruct();
   return ShuttleGui::TieTextBox( Prompt, SettingName, Default, nChars );
}
wxTextCtrl * ShuttleGuiGetDefinition::TieNumericTextBox(
   const wxString & Prompt,
   const wxString & SettingName,
   const double & Default,
   const int nChars) 
{
   StartStruct();
   AddItem( SettingName, "id" );
   AddItem( Prompt, "prompt" );
   AddItem( "number", "type" );
   AddItem( Default, "default"  );
   EndStruct();
   return ShuttleGui::TieNumericTextBox( Prompt, SettingName, Default, nChars );
}
wxSlider * ShuttleGuiGetDefinition::TieSlider(
   const wxString & Prompt,
   const wxString & SettingName,
   const int iDefault,
   const int max,
   const int min) 
{
   StartStruct();
   AddItem( SettingName, "id" );
   AddItem( Prompt, "prompt" );
   AddItem( "number", "type" );
   AddItem( iDefault, "default"  );
   EndStruct();
   return ShuttleGui::TieSlider( Prompt, SettingName, iDefault, max, min );
}
wxSpinCtrl * ShuttleGuiGetDefinition::TieSpinCtrl(
   const wxString &Prompt,
   const wxString &SettingName,
   const int Value,
   const int max,
   const int min) 
{
   StartStruct();
   AddItem( SettingName, "id" );
   AddItem( Prompt, "prompt" );
   AddItem( "number", "type" );
   AddItem( Value, "default"  );
   EndStruct();
   return ShuttleGui::TieSpinCtrl( Prompt, SettingName, Value, max, min );
}

}

bool GetInfoCommand::SendPreferences(const CommandContext &context)
{
   context.StartArray();
   auto pWin = &GetProjectFrame( context.project );
   GlobalPrefsDialog dialog( pWin );
   // wxCommandEvent Evt;
   //dialog.Show();
   ShuttleGuiGetDefinition S(pWin, *((context.pOutput)->mStatusTarget) );
    dialog.ShuttleAll( S );
   context.EndArray();
   return true;
}

/**
 Send the list of commands.
 */
bool GetInfoCommand::SendCommands(const CommandContext &context, int flags )
{
   context.StartArray();
   PluginManager & pm = PluginManager::Get();
   EffectManager & em = EffectManager::Get();
   {
      const PluginDescriptor *plug = pm.GetFirstPlugin(PluginTypeEffect | PluginTypeAudacityCommand);
      while (plug)
      {
         auto command = em.GetCommandIdentifier(plug->GetID());
         if (!command.empty()){
            em.GetCommandDefinition( plug->GetID(), context, flags );
         }
         plug = pm.GetNextPlugin(PluginTypeEffect | PluginTypeAudacityCommand );
      }
   }
   context.EndArray();
   return true;
}

bool GetInfoCommand::SendBoxes(const CommandContext &context)
{
   //context.Status("Boxes");
   auto pWin = &GetProjectFrame( context.project );

   context.StartArray();
   wxRect R = pWin->GetScreenRect();

   //R.SetPosition( wxPoint(0,0) );
   
   //wxString Name = pWin->GetName();
   context.StartStruct();
   context.AddItem( 0, "depth" );
   context.AddItem( "Audacity Window", "name" ); 
   context.StartField( "box" );
   context.StartArray( );
   context.AddItem( R.GetLeft() );
   context.AddItem( R.GetTop() );
   context.AddItem( R.GetRight() );
   context.AddItem( R.GetBottom() );
   context.EndArray( );
   context.EndField();
   context.EndStruct( );

   ExploreAdornments( context, pWin->GetPosition()+wxSize( 6,-1), pWin, pWin->GetId(), 1 );
   ExploreWindows( context, pWin->GetPosition()+wxSize( 6,-1), pWin, pWin->GetId(), 1 );
   context.EndArray( );
   return true;
}

bool GetInfoCommand::SendTracks(const CommandContext & context)
{
   auto &tracks = TrackList::Get( context.project );
   context.StartArray();
   for (auto trk : tracks.Leaders())
   {
      auto &panel = TrackPanel::Get( context.project );
      Track * fTrack = panel.GetFocusedTrack();

      context.StartStruct();
      context.AddItem( trk->GetName(), "name" );
      context.AddBool( (trk == fTrack), "focused");
      context.AddBool( trk->GetSelected(), "selected" );
      //JKC: Possibly add later...
      //context.AddItem( trk->GetHeight(), "height" );
      trk->TypeSwitch( [&] (const WaveTrack* t ) {
         float vzmin, vzmax;
         t->GetDisplayBounds(&vzmin, &vzmax);
         context.AddItem( "wave", "kind" );
         context.AddItem( t->GetStartTime(), "start" );
         context.AddItem( t->GetEndTime(), "end" );
         context.AddItem( t->GetPan() , "pan");
         context.AddItem( t->GetGain() , "gain");
         context.AddItem( TrackList::Channels(t).size(), "channels");
         context.AddBool( t->GetSolo(), "solo" );
         context.AddBool( t->GetMute(), "mute");
         context.AddItem( vzmin, "VZoomMin");
         context.AddItem( vzmax, "VZoomMax");
      },
#if defined(USE_MIDI)
      [&](const NoteTrack *) {
         context.AddItem( "note", "kind" );
      },
#endif
      [&](const LabelTrack *) {
         context.AddItem( "label", "kind" );
      },
      [&](const TimeTrack *) {
         context.AddItem( "time", "kind" );
      }
      );
      context.EndStruct();
   }
   context.EndArray();
   return true;
}

bool GetInfoCommand::SendClips(const CommandContext &context)
{
   auto &tracks = TrackList::Get( context.project );
   int i=0;
   context.StartArray();
   for (auto waveTrack : tracks.Leaders<WaveTrack>()) {
      WaveClipPointers ptrs( waveTrack->SortedClipArray());
      for(WaveClip * pClip : ptrs ) {
         context.StartStruct();
         context.AddItem( (double)i, "track" );
         context.AddItem( pClip->GetStartTime(), "start" );
         context.AddItem( pClip->GetEndTime(), "end" );
         context.AddItem( pClip->GetColourIndex(), "color" );
         context.EndStruct();
      }
      i++;
   }
   context.EndArray();

   return true;
}

bool GetInfoCommand::SendEnvelopes(const CommandContext &context)
{
   auto &tracks = TrackList::Get( context.project );
   int i=0;
   int j=0;
   context.StartArray();
   for (auto waveTrack : tracks.Leaders<WaveTrack>()) {
      WaveClipPointers ptrs( waveTrack->SortedClipArray());
      for(WaveClip * pClip : ptrs ) {
         context.StartStruct();
         context.AddItem( (double)i, "track" );
         context.AddItem( (double)j, "clip" );
         context.AddItem( pClip->GetStartTime(), "start" );
         Envelope * pEnv = pClip->GetEnvelope();
         context.StartField( "points" );
         context.StartArray();
         double offset = pEnv->mOffset;
         for( size_t k=0;k<pEnv->mEnv.size(); k++)
         {
            context.StartStruct( );
            context.AddItem( pEnv->mEnv[k].GetT()+offset, "t" );
            context.AddItem( pEnv->mEnv[k].GetVal(), "y" );
            context.EndStruct();
         }
         context.EndArray();
         context.EndField();
         context.AddItem( pClip->GetEndTime(), "end" );
         context.EndStruct();
         j++;
      }
   }
   context.EndArray();

   return true;
}


bool GetInfoCommand::SendLabels(const CommandContext &context)
{
   auto &tracks = TrackList::Get( context.project );
   int i=0;
   context.StartArray();
   for (auto t : tracks.Leaders()) {
      t->TypeSwitch( [&](LabelTrack *labelTrack) {
#ifdef VERBOSE_LABELS_FORMATTING
         for (int nn = 0; nn< (int)labelTrack->mLabels.size(); nn++) {
            const auto &label = labelTrack->mLabels[nn];
            context.StartStruct();
            context.AddItem( (double)i, "track" );
            context.AddItem( label.getT0(), "start" );
            context.AddItem( label.getT1(), "end" );
            context.AddItem( label.title, "text" );
            context.EndStruct();
         }
#else
         context.StartArray();
         context.AddItem( (double)i ); // Track number.
         context.StartArray();
         for (int nn = 0; nn< (int)labelTrack->mLabels.size(); nn++) {
            const auto &label = labelTrack->mLabels[nn];
            context.StartArray();
            context.AddItem( label.getT0() ); // start
            context.AddItem( label.getT1() ); // end
            context.AddItem( label.title ); //text.
            context.EndArray();
         }
         context.EndArray();
         context.EndArray();
#endif
      } );
      // Per track numbering counts all tracks
      i++;
   }
   context.EndArray();



   return true;
}

/*******************************************************************
The various Explore functions are called from the Send functions,
and may be recursive.  'Send' is the top level.
*******************************************************************/

void GetInfoCommand::ExploreMenu( const CommandContext &context, wxMenu * pMenu, int Id, int depth ){
   static_cast<void>(Id);//compiler food.
   if( !pMenu )
      return;

   auto &commandManager = CommandManager::Get( context.project );

   wxMenuItemList list = pMenu->GetMenuItems();
   size_t lcnt = list.size();
   wxMenuItem * item;
   wxString Label;
   wxString Accel;
   CommandID Name;

   for (size_t lndx = 0; lndx < lcnt; lndx++) {
      item = list.Item(lndx)->GetData();
      Label = item->GetItemLabelText();
      Name = commandManager.GetNameFromNumericID( item->GetId() );
      Accel = item->GetItemLabel();
      if( Accel.Contains("\t") )
         Accel = Accel.AfterLast('\t');
      else
         Accel = "";
      if( item->IsSeparator() )
         Label = "----";
      int flags = 0;
      if (item->IsSubMenu())
         flags +=1;
      if (item->IsCheck() && item->IsChecked())
         flags +=2;

      context.StartStruct();
      context.AddItem( depth, "depth" );
      context.AddItem( flags, "flags" );
      context.AddItem( Label, "label" );
      context.AddItem( Accel, "accel" );
      if( !Name.empty() )
         // using GET to expose CommandID in results of GetInfoCommand...
         // PRL asks, is that all right?
         context.AddItem( Name.GET(), "id" );// It is called Scripting ID outside Audacity.
      context.EndStruct();

      if (item->IsSubMenu()) {
         pMenu = item->GetSubMenu();
         ExploreMenu( context, pMenu, item->GetId(), depth+1 );
      }
   }
}

void GetInfoCommand::ExploreAdornments( const CommandContext &context,
   wxPoint WXUNUSED(P), wxWindow * pWin, int WXUNUSED(Id), int depth )
{
   // Dang! wxMenuBar returns bogus screen rect.
   // We're going to have to fake it instead.
   //wxMenuBar * pBar = context.GetProject()->GetMenuBar();
   //wxRect R = pBar->GetScreenRect();
   //R.SetPosition( R.GetPosition() - P );

   wxRect R1 = pWin->GetScreenRect();
   wxSize s = pWin->GetWindowBorderSize();
   wxRect R( 2,32, R1.GetWidth() - s.GetWidth() * 2 -16, 22 );

   context.StartStruct();
   context.AddItem( depth, "depth" );
   context.AddItem( "MenuBar", "label" ); 
   context.StartField( "box" );
   context.StartArray();
   context.AddItem( R.GetLeft() );
   context.AddItem( R.GetTop() );
   context.AddItem( R.GetRight() );
   context.AddItem( R.GetBottom() );
   context.EndArray();
   context.EndField();
   context.EndStruct();
}

void GetInfoCommand::ExploreTrackPanel( const CommandContext &context,
   wxPoint P, wxWindow * pWin, int WXUNUSED(Id), int depth )
{
   AudacityProject * pProj = &context.project;
   auto &tp = TrackPanel::Get( *pProj );

   wxRect trackRect = pWin->GetRect();

   for ( auto t : TrackList::Get( *pProj ).Any() + IsVisibleTrack{ pProj } ) {
      trackRect.y = t->GetY() - tp.mViewInfo->vpos;
      trackRect.height = t->GetHeight();

#if 0
      // Work in progress on getting the TCP button positions and sizes.
      wxRect rect = trackRect;
      Track *l = t->GetLink();

      if (t->GetLinked()) {
         rect.height += l->GetHeight();
      }

      switch (t->GetKind()) {
         case Track::Wave:
         {
            break;
         }
#ifdef USE_MIDI
         case Track::Note:
         {
            break;
         }
#endif // USE_MIDI
         case Track::Label:
            break;
         case Track::Time:
            break;
      }
      {
         // Start with whole track rect
         wxRect R = trackRect;

         // Now exclude left, right, and top insets
         R.x += kLeftInset;
         R.y += kTopInset;
         R.width -= kLeftInset * 2;
         R.height -= kTopInset;

         int labelw = pTP->GetLabelWidth();
         int vrul = pTP->GetVRulerOffset();
         bool bIsWave = true;
         //mTrackInfo.DrawBackground(dc, R, t->GetSelected(), bIsWave, labelw, vrul);


         for (Overlay * pOverlay : pTP->mOverlays) {
            auto R2(pOverlay->GetRectangle(trackRect.GetSize()).first);
            context.Status( wxString::Format("  [ %2i, %3i, %3i, %3i, %3i, \"%s\" ],", 
               depth, R2.GetLeft(), R2.GetTop(), R2.GetRight(), R2.GetBottom(), "Overthing" )); 
         }
      }
#endif

      // The VRuler.
      {  
         wxRect R = trackRect;
         R.x += tp.GetVRulerOffset();
         R.y += kTopMargin;
         R.width = tp.GetVRulerWidth();
         R.height -= (kTopMargin + kBottomMargin);
         R.SetPosition( R.GetPosition() + P );

         context.StartStruct();
         context.AddItem( depth, "depth" );
         context.AddItem( "VRuler", "label" ); 
         context.StartField("box");
         context.StartArray();
         context.AddItem( R.GetLeft() );
         context.AddItem( R.GetTop() );
         context.AddItem( R.GetRight() );
         context.AddItem( R.GetBottom() );
         context.EndArray();
         context.EndField();
         context.EndStruct();
      }
   }
}


void GetInfoCommand::ExploreWindows( const CommandContext &context,
   wxPoint P, wxWindow * pWin, int Id, int depth )
{
   static_cast<void>(Id);//Compiler food.

   if( pWin->GetName() == "Track Panel" )
   {
      wxRect R = pWin->GetScreenRect();
      ExploreTrackPanel(  context, R.GetPosition()-P, pWin, Id, depth );
      return;
   }
   wxWindowList list = pWin->GetChildren();
   size_t lcnt = list.size();

   for (size_t lndx = 0; lndx < lcnt; lndx++) {
      wxWindow * item = list[lndx];
      if( !item->IsShown() )
         continue;
      wxRect R = item->GetScreenRect();
      R.SetPosition( R.GetPosition() - P );
      wxString Name = item->GetName();
      // Ignore staticLine and StaticBitmap.
      if( Name.StartsWith( "static" ) )
         continue;
      // Ignore anonymous panels.
      if( Name == "panel"  )
         continue;
      if( Name.empty() )
         Name = wxString("*") + item->GetToolTipText();

      context.StartStruct();
      context.AddItem( depth, "depth" );
      context.AddItem( Name, "label" );
      context.AddItem( item->GetId(), "id" );
      context.StartField( "box" );
      context.StartArray();
      context.AddItem( R.GetLeft() );
      context.AddItem( R.GetTop() );
      context.AddItem( R.GetRight() );
      context.AddItem( R.GetBottom() );
      context.EndArray();
      context.EndField();
      context.EndStruct();

      ExploreWindows( context, P, item, item->GetId(), depth+1 );
   }
}

