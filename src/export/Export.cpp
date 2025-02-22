/**********************************************************************

  Audacity: A Digital Audio Editor

  Export.cpp

  Dominic Mazzoni

*******************************************************************//**

\class Export
\brief Main class to control the export function.

*//****************************************************************//**

\class ExportType
\brief Container for information about supported export types.

*//****************************************************************//**

\class ExportMixerDialog
\brief Dialog for advanced mixing.

*//****************************************************************//**

\class ExportMixerPanel
\brief Panel that displays mixing for advanced mixing option.

*//********************************************************************/

#include "../Audacity.h" // for USE_* macros
#include "Export.h"

#include <wx/dcclient.h>
#include <wx/file.h>
#include <wx/filectrl.h>
#include <wx/filename.h>
#include <wx/progdlg.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/timer.h>
#include <wx/dcmemory.h>
#include <wx/window.h>

#include "ExportPCM.h"
#include "ExportMP3.h"
#include "ExportOGG.h"
#include "ExportFLAC.h"
#include "ExportCL.h"
#include "ExportMP2.h"
#include "ExportFFmpeg.h"

#include "sndfile.h"

#include "FileDialog.h"

#include "../DirManager.h"
#include "../FileFormats.h"
#include "../Menus.h"
#include "../Mix.h"
#include "../Prefs.h"
#include "../Project.h"
#include "../ProjectSettings.h"
#include "../ProjectWindow.h"
#include "../ShuttleGui.h"
#include "../WaveTrack.h"
#include "../widgets/AudacityMessageBox.h"
#include "../widgets/Warning.h"
#include "../widgets/HelpSystem.h"
#include "../AColor.h"
#include "../Dependencies.h"
#include "../FileNames.h"
#include "../widgets/HelpSystem.h"
#include "../widgets/ProgressDialog.h"

//----------------------------------------------------------------------------
// ExportPlugin
//----------------------------------------------------------------------------

ExportPlugin::ExportPlugin()
{
}

ExportPlugin::~ExportPlugin()
{
}

bool ExportPlugin::CheckFileName(wxFileName & WXUNUSED(filename), int WXUNUSED(format))
{
  return true;
}

/** \brief Add a NEW entry to the list of formats this plug-in can export
 *
 * To configure the format use SetFormat, SetCanMetaData etc with the index of
 * the format.
 * @return The number of formats currently set up. This is one more than the
 * index of the newly added format.
 */
int ExportPlugin::AddFormat()
{
   FormatInfo nf;
   mFormatInfos.push_back(nf);
   return mFormatInfos.size();
}

int ExportPlugin::GetFormatCount()
{
   return mFormatInfos.size();
}

/**
 * @param index The plugin to set the format for (range 0 to one less than the
 * count of formats)
 */
void ExportPlugin::SetFormat(const wxString & format, int index)
{
   mFormatInfos[index].mFormat = format;
}

void ExportPlugin::SetDescription(const wxString & description, int index)
{
   mFormatInfos[index].mDescription = description;
}

void ExportPlugin::AddExtension(const wxString &extension,int index)
{
   mFormatInfos[index].mExtensions.push_back(extension);
}

void ExportPlugin::SetExtensions(FileExtensions extensions, int index)
{
   mFormatInfos[index].mExtensions = std::move(extensions);
}

void ExportPlugin::SetMask(const wxString & mask, int index)
{
   mFormatInfos[index].mMask = mask;
}

void ExportPlugin::SetMaxChannels(unsigned maxchannels, unsigned index)
{
   mFormatInfos[index].mMaxChannels = maxchannels;
}

void ExportPlugin::SetCanMetaData(bool canmetadata, int index)
{
   mFormatInfos[index].mCanMetaData = canmetadata;
}

wxString ExportPlugin::GetFormat(int index)
{
   return mFormatInfos[index].mFormat;
}

wxString ExportPlugin::GetDescription(int index)
{
   return mFormatInfos[index].mDescription;
}

FileExtension ExportPlugin::GetExtension(int index)
{
   return mFormatInfos[index].mExtensions[0];
}

FileExtensions ExportPlugin::GetExtensions(int index)
{
   return mFormatInfos[index].mExtensions;
}

wxString ExportPlugin::GetMask(int index)
{
   if (!mFormatInfos[index].mMask.empty()) {
      return mFormatInfos[index].mMask;
   }

   wxString mask = GetDescription(index) + wxT("|");

   // Build the mask
   // const auto &ext = GetExtension(index);
   const auto &exts = GetExtensions(index);
   for (size_t i = 0; i < exts.size(); i++) {
      mask += wxT("*.") + exts[i] + wxT(";");
   }

   return mask;
}

unsigned ExportPlugin::GetMaxChannels(int index)
{
   return mFormatInfos[index].mMaxChannels;
}

bool ExportPlugin::GetCanMetaData(int index)
{
   return mFormatInfos[index].mCanMetaData;
}

bool ExportPlugin::IsExtension(const wxString & ext, int index)
{
   bool isext = false;
   for (int i = index; i < GetFormatCount(); i = GetFormatCount())
   {
      const auto &defext = GetExtension(i);
      const auto &defexts = GetExtensions(i);
      int indofext = defexts.Index(ext, false);
      if (defext.empty() || (indofext != wxNOT_FOUND))
         isext = true;
   }
   return isext;
}

bool ExportPlugin::DisplayOptions(wxWindow * WXUNUSED(parent), int WXUNUSED(format))
{
   return false;
}

wxWindow *ExportPlugin::OptionsCreate(wxWindow *parent, int WXUNUSED(format))
{
   wxASSERT(parent); // To justify safenew
   wxPanel *p = safenew wxPanelWrapper(parent, wxID_ANY);
   ShuttleGui S(p, eIsCreatingFromPrefs);

   S.StartHorizontalLay(wxCENTER);
   {
      S.StartHorizontalLay(wxCENTER, 0);
      {
         S.Prop(1).AddTitle(_("No format specific options"));
      }
      S.EndHorizontalLay();
   }
   S.EndHorizontalLay();

   return p;
}

//Create a mixer by computing the time warp factor
std::unique_ptr<Mixer> ExportPlugin::CreateMixer(const WaveTrackConstArray &inputTracks,
         const TimeTrack *timeTrack,
         double startTime, double stopTime,
         unsigned numOutChannels, size_t outBufferSize, bool outInterleaved,
         double outRate, sampleFormat outFormat,
         bool highQuality, MixerSpec *mixerSpec)
{
   // MB: the stop time should not be warped, this was a bug.
   return std::make_unique<Mixer>(inputTracks,
                  // Throw, to stop exporting, if read fails:
                  true,
                  Mixer::WarpOptions(timeTrack),
                  startTime, stopTime,
                  numOutChannels, outBufferSize, outInterleaved,
                  outRate, outFormat,
                  highQuality, mixerSpec);
}

void ExportPlugin::InitProgress(std::unique_ptr<ProgressDialog> &pDialog,
   const wxString &title, const wxString &message)
{
   if (!pDialog)
      pDialog = std::make_unique<ProgressDialog>( title, message );
   else {
      pDialog->SetTitle( title );
      pDialog->SetMessage( message );
      pDialog->Reinit();
   }
}

//----------------------------------------------------------------------------
// Export
//----------------------------------------------------------------------------


wxDEFINE_EVENT(AUDACITY_FILE_SUFFIX_EVENT, wxCommandEvent);

BEGIN_EVENT_TABLE(Exporter, wxEvtHandler)
   EVT_FILECTRL_FILTERCHANGED(wxID_ANY, Exporter::OnFilterChanged)
   EVT_BUTTON(wxID_HELP, Exporter::OnHelp)
   EVT_COMMAND( wxID_ANY, AUDACITY_FILE_SUFFIX_EVENT, Exporter::OnExtensionChanged)
END_EVENT_TABLE()

Exporter::Exporter()
{
   mMixerSpec = NULL;
   mBook = NULL;

   SetFileDialogTitle( _("Export Audio") );

   RegisterPlugin(New_ExportPCM());
   RegisterPlugin(New_ExportMP3());

#ifdef USE_LIBVORBIS
   RegisterPlugin(New_ExportOGG());
#endif

#ifdef USE_LIBFLAC
   RegisterPlugin(New_ExportFLAC());
#endif

#if USE_LIBTWOLAME
   RegisterPlugin(New_ExportMP2());
#endif

   // Command line export not available on Windows and Mac platforms
   RegisterPlugin(New_ExportCL());

#if defined(USE_FFMPEG)
   RegisterPlugin(New_ExportFFmpeg());
#endif
}

Exporter::~Exporter()
{
}

// Beginnings of a fix for bug 1355.
// 'Other Uncompressed Files' Header option updates do not update
// the extension shown in the file dialog.
// Unfortunately, although we get the new extension here, we
// can't do anything with it as the FileDialog does not provide
// methods for setting its standard controls.
// We would need OS specific code that 'knows' about the system 
// dialogs.
void Exporter::OnExtensionChanged(wxCommandEvent &evt) {
   wxString ext = evt.GetString();
   ext = ext.BeforeFirst(' ').Lower();
   wxLogDebug("Extension changed to '.%s'", ext);
//   wxString Name = mDialog->GetFilename();
//   Name = Name.BeforeLast('.')+ext;
//   mDialog->SetFilename(Name);
}

void Exporter::OnHelp(wxCommandEvent& WXUNUSED(evt))
{
   wxWindow * pWin = FindProjectFrame( GetActiveProject() );
   HelpSystem::ShowHelp(pWin, wxT("File_Export_Dialog"), true);
}

void Exporter::SetFileDialogTitle( const wxString & DialogTitle )
{
   // The default title is "Export File"
   mFileDialogTitle = DialogTitle;
}

int Exporter::FindFormatIndex(int exportindex)
{
   int c = 0;
   for (const auto &pPlugin : mPlugins)
   {
      for (int j = 0; j < pPlugin->GetFormatCount(); j++)
      {
         if (exportindex == c) return j;
         c++;
      }
   }
   return 0;
}

void Exporter::RegisterPlugin(std::unique_ptr<ExportPlugin> &&ExportPlugin)
{
   mPlugins.push_back(std::move(ExportPlugin));
}

const ExportPluginArray &Exporter::GetPlugins()
{
   return mPlugins;
}

bool Exporter::Process(AudacityProject *project, bool selectedOnly, double t0, double t1)
{
   // Save parms
   mProject = project;
   mSelectedOnly = selectedOnly;
   mT0 = t0;
   mT1 = t1;

   // Gather track information
   if (!ExamineTracks()) {
      return false;
   }

   // Ask user for file name
   if (!GetFilename()) {
      return false;
   }

   // Check for down mixing
   if (!CheckMix()) {
      return false;
   }

   // Let user edit MetaData
   if (mPlugins[mFormat]->GetCanMetaData(mSubFormat)) {
      if (!(EditActions::DoEditMetadata( *project,
         _("Edit Metadata Tags"), _("Exported Tags"),
         ProjectSettings::Get( *mProject ).GetShowId3Dialog()))) {
         return false;
      }
   }

   // Ensure filename doesn't interfere with project files.
   if (!CheckFilename()) {
      return false;
   }

   // Export the tracks
   bool success = ExportTracks();

   // Get rid of mixerspec
   mMixerSpec.reset();

   return success;
}

bool Exporter::Process(AudacityProject *project, unsigned numChannels,
                       const wxChar *type, const wxString & filename,
                       bool selectedOnly, double t0, double t1)
{
   // Save parms
   mProject = project;
   mChannels = numChannels;
   mFilename = filename;
   mSelectedOnly = selectedOnly;
   mT0 = t0;
   mT1 = t1;
   mActualName = mFilename;

   int i = -1;
   for (const auto &pPlugin : mPlugins) {
      ++i;
      for (int j = 0; j < pPlugin->GetFormatCount(); j++)
      {
         if (pPlugin->GetFormat(j).IsSameAs(type, false))
         {
            mFormat = i;
            mSubFormat = j;
            return CheckFilename() && ExportTracks();
         }
      }
   }

   return false;
}

bool Exporter::ExamineTracks()
{
   // Init
   mNumSelected = 0;
   mNumLeft = 0;
   mNumRight = 0;
   mNumMono = 0;

   // First analyze the selected audio, perform sanity checks, and provide
   // information as appropriate.

   // Tally how many are right, left, mono, and make sure at
   // least one track is selected (if selectedOnly==true)

   double earliestBegin = mT1;
   double latestEnd = mT0;

   auto &tracks = TrackList::Get( *mProject );

   for (auto tr :
         tracks.Any< const WaveTrack >()
            + ( mSelectedOnly ? &Track::IsSelected : &Track::Any )
            - &WaveTrack::GetMute
   ) {
      mNumSelected++;

      if (tr->GetChannel() == Track::LeftChannel) {
         mNumLeft++;
      }
      else if (tr->GetChannel() == Track::RightChannel) {
         mNumRight++;
      }
      else if (tr->GetChannel() == Track::MonoChannel) {
         // It's a mono channel, but it may be panned
         float pan = tr->GetPan();

         if (pan == -1.0)
            mNumLeft++;
         else if (pan == 1.0)
            mNumRight++;
         else if (pan == 0)
            mNumMono++;
         else {
            // Panned partially off-center. Mix as stereo.
            mNumLeft++;
            mNumRight++;
         }
      }

      if (tr->GetOffset() < earliestBegin) {
         earliestBegin = tr->GetOffset();
      }

      if (tr->GetEndTime() > latestEnd) {
         latestEnd = tr->GetEndTime();
      }
   }

   if (mNumSelected == 0) {
      wxString message;
      if(mSelectedOnly)
         message = _("All selected audio is muted.");
      else
         message = _("All audio is muted.");
      AudacityMessageBox(message,
                    _("Unable to export"),
                    wxOK | wxICON_INFORMATION);
      return false;
   }

   if (mT0 < earliestBegin)
      mT0 = earliestBegin;

   if (mT1 > latestEnd)
      mT1 = latestEnd;

   return true;
}

bool Exporter::GetFilename()
{
   mFormat = -1;

   wxString maskString;
   wxString defaultFormat = mFormatName;
   if( defaultFormat.empty() )
      defaultFormat = gPrefs->Read(wxT("/Export/Format"),
                                         wxT("WAV"));

   mFilterIndex = 0;

   {
      int i = -1;
      for (const auto &pPlugin : mPlugins) {
         ++i;
         for (int j = 0; j < pPlugin->GetFormatCount(); j++)
         {
            maskString += pPlugin->GetMask(j) + wxT("|");
            if (mPlugins[i]->GetFormat(j) == defaultFormat) {
               mFormat = i;
               mSubFormat = j;
            }
            if (mFormat == -1) mFilterIndex++;
         }
      }
   }
   if (mFormat == -1)
   {
      mFormat = 0;
      mFilterIndex = 0;
      mSubFormat = 0;
   }
   maskString.RemoveLast();
   wxString defext = mPlugins[mFormat]->GetExtension(mSubFormat).Lower();

//Bug 1304: Set a default path if none was given.  For Export.
   mFilename = FileNames::DefaultToDocumentsFolder(wxT("/Export/Path"));
   mFilename.SetName(mProject->GetProjectName());
   if (mFilename.GetName().empty())
      mFilename.SetName(_("untitled"));
   while (true) {
      // Must reset each iteration
      mBook = NULL;

      {
         auto useFileName = mFilename;
         if (!useFileName.HasExt())
            useFileName.SetExt(defext);
         FileDialogWrapper fd( ProjectWindow::Find( mProject ),
                       mFileDialogTitle,
                       mFilename.GetPath(),
                       useFileName.GetFullName(),
                       maskString,
                       wxFD_SAVE | wxRESIZE_BORDER);
         mDialog = &fd;
         mDialog->PushEventHandler(this);

         fd.SetUserPaneCreator(CreateUserPaneCallback, (wxUIntPtr) this);
         fd.SetFilterIndex(mFilterIndex);

         int result = fd.ShowModal();

         mDialog->PopEventHandler();

         if (result == wxID_CANCEL) {
            return false;
         }

         mFilename = fd.GetPath();
         if (mFilename == wxT("")) {
            return false;
         }

         mFormat = fd.GetFilterIndex();
         mFilterIndex = fd.GetFilterIndex();
      }

      {
         int c = 0;
         int i = -1;
         for (const auto &pPlugin : mPlugins)
         {
            ++i;
            for (int j = 0; j < pPlugin->GetFormatCount(); j++)
            {
               if (mFilterIndex == c)
               {
                  mFormat = i;
                  mSubFormat = j;
               }
               c++;
            }
         }
      }

      auto ext = mFilename.GetExt();
      defext = mPlugins[mFormat]->GetExtension(mSubFormat).Lower();

      //
      // Check the extension - add the default if it's not there,
      // and warn user if it's abnormal.
      //
      if (ext.empty()) {
         //
         // Make sure the user doesn't accidentally save the file
         // as an extension with no name, like just plain ".wav".
         //
         if (mFilename.GetName().Left(1) == wxT(".")) {
            wxString prompt = wxString::Format(
               _("Are you sure you want to export the file as \"%s\"?\n"),
                  mFilename.GetFullName() );

            int action = AudacityMessageBox(prompt,
                                      _("Warning"),
                                      wxYES_NO | wxICON_EXCLAMATION);
            if (action != wxYES) {
               continue;
            }
         }

         mFilename.SetExt(defext);
      }
      else if (!mPlugins[mFormat]->CheckFileName(mFilename, mSubFormat))
      {
         continue;
      }
      else if (!ext.empty() && !mPlugins[mFormat]->IsExtension(ext,mSubFormat) && ext.CmpNoCase(defext)) {
         wxString prompt;
         prompt.Printf(_("You are about to export a %s file with the name \"%s\".\n\nNormally these files end in \".%s\", and some programs will not open files with nonstandard extensions.\n\nAre you sure you want to export the file under this name?"),
                       mPlugins[mFormat]->GetFormat(mSubFormat),
                       mFilename.GetFullName(),
                       defext);

         int action = AudacityMessageBox(prompt,
                                   _("Warning"),
                                   wxYES_NO | wxICON_EXCLAMATION);
         if (action != wxYES) {
            continue;
         }
      }

      if (mFilename.GetFullPath().length() >= 256) {
         AudacityMessageBox(_("Sorry, pathnames longer than 256 characters not supported."));
         continue;
      }

      // Check to see if we are writing to a path that a missing aliased file existed at.
      // This causes problems for the exporter, so we don't allow it.
      // Overwritting non-missing aliased files is okay.
      // Also, this can only happen for uncompressed audio.
      bool overwritingMissingAliasFiles;
      overwritingMissingAliasFiles = false;
      for (auto pProject : AllProjects{}) {
         AliasedFileArray aliasedFiles;
         FindDependencies(pProject.get(), aliasedFiles);
         for (const auto &aliasedFile : aliasedFiles) {
            if (mFilename.GetFullPath() == aliasedFile.mFileName.GetFullPath() &&
                !mFilename.FileExists()) {
               // Warn and return to the dialog
               AudacityMessageBox(_("You are attempting to overwrite an aliased file that is missing.\n\
               The file cannot be written because the path is needed to restore the original audio to the project.\n\
               Choose Help > Diagnostics > Check Dependencies to view the locations of all missing files.\n\
               If you still wish to export, please choose a different filename or folder."));
               overwritingMissingAliasFiles = true;
            }
         }
      }
      if (overwritingMissingAliasFiles)
         continue;

      if (mFilename.FileExists()) {
         wxString prompt;

         prompt.Printf(_("A file named \"%s\" already exists. Replace?"),
                       mFilename.GetFullPath());

         int action = AudacityMessageBox(prompt,
                                   _("Warning"),
                                   wxYES_NO | wxICON_EXCLAMATION);
         if (action != wxYES) {
            continue;
         }
      }

      break;
   }

   return true;
}

//
// For safety, if the file already exists it stores the filename
// the user wants in actualName, and returns a temporary file name.
// The calling function should rename the file when it's successfully
// exported.
//
bool Exporter::CheckFilename()
{
   //
   // Ensure that exporting a file by this name doesn't overwrite
   // one of the existing files in the project.  (If it would
   // overwrite an existing file, DirManager tries to rename the
   // existing file.)
   //

   if (!DirManager::Get( *mProject ).EnsureSafeFilename(mFilename))
      return false;

   if( mFormatName.empty() )
      gPrefs->Write(wxT("/Export/Format"), mPlugins[mFormat]->GetFormat(mSubFormat));
   gPrefs->Write(wxT("/Export/Path"), mFilename.GetPath());
   gPrefs->Flush();

   //
   // To be even safer, return a temporary file name based
   // on this one...
   //

   mActualName = mFilename;

   int suffix = 0;
   while (mFilename.FileExists()) {
      mFilename.SetName(mActualName.GetName() +
                        wxString::Format(wxT("%d"), suffix));
      suffix++;
   }

   return true;
}

void Exporter::DisplayOptions(int index)
{
   int c = 0;
   int mf = -1, msf = -1;
   int i = -1;
   for (const auto &pPlugin : mPlugins)
   {
      ++i;
      for (int j = 0; j < pPlugin->GetFormatCount(); j++)
      {
         if (index == c)
         {
            mf = i;
            msf = j;
         }
         c++;
      }
   }
   // This shouldn't happen...
   if (index >= c) {
      return;
   }

#if defined(__WXMSW__)
   mPlugins[mf]->DisplayOptions(mProject, msf);
#else
   mPlugins[mf]->DisplayOptions(mDialog, msf);
#endif
}

bool Exporter::CheckMix()
{
   // Clean up ... should never happen
   mMixerSpec.reset();

   // Detemine if exported file will be stereo or mono or multichannel,
   // and if mixing will occur.

   int downMix = gPrefs->Read(wxT("/FileFormats/ExportDownMix"), true);
   int exportedChannels = mPlugins[mFormat]->SetNumExportChannels();

   if (downMix) {
      if (mNumRight > 0 || mNumLeft > 0) {
         mChannels = 2;
      }
      else {
         mChannels = 1;
      }
      mChannels = std::min(mChannels,
                           mPlugins[mFormat]->GetMaxChannels(mSubFormat));

      auto numLeft =  mNumLeft + mNumMono;
      auto numRight = mNumRight + mNumMono;

      if (numLeft > 1 || numRight > 1 || mNumLeft + mNumRight + mNumMono > mChannels) {
         wxString exportFormat = mPlugins[mFormat]->GetFormat(mSubFormat);
         if (exportFormat != wxT("CL") && exportFormat != wxT("FFMPEG") && exportedChannels == -1)
            exportedChannels = mChannels;

         auto pWindow = ProjectWindow::Find( mProject );
         if (exportedChannels == 1) {
            if (ShowWarningDialog(pWindow,
                                  wxT("MixMono"),
                                  _("Your tracks will be mixed down and exported as one mono file."),
                                  true) == wxID_CANCEL)
               return false;
         }
         else if (exportedChannels == 2) {
            if (ShowWarningDialog(pWindow,
                                  wxT("MixStereo"),
                                  _("Your tracks will be mixed down and exported as one stereo file."),
                                  true) == wxID_CANCEL)
               return false;
         }
         else {
            if (ShowWarningDialog(pWindow,
                                  wxT("MixUnknownChannels"),
                                  _("Your tracks will be mixed down to one exported file according to the encoder settings."),
                                  true) == wxID_CANCEL)
               return false;
         }
      }
   }
   else
   {
      if (exportedChannels < 0)
         exportedChannels = mPlugins[mFormat]->GetMaxChannels(mSubFormat);

      ExportMixerDialog md(&TrackList::Get( *mProject ),
                           mSelectedOnly,
                           exportedChannels,
                           NULL,
                           1,
                           _("Advanced Mixing Options"));

      if (md.ShowModal() != wxID_OK) {
         return false;
      }

      mMixerSpec = std::make_unique<MixerSpec>(*(md.GetMixerSpec()));
      mChannels = mMixerSpec->GetNumChannels();
   }

   return true;
}

bool Exporter::ExportTracks()
{
   // Keep original in case of failure
   if (mActualName != mFilename) {
      ::wxRenameFile(mActualName.GetFullPath(), mFilename.GetFullPath());
   }

   bool success = false;

   auto cleanup = finally( [&] {
      if (mActualName != mFilename) {
         // Remove backup
         if ( success )
            ::wxRemoveFile(mFilename.GetFullPath());
         else {
            // Restore original, if needed
            ::wxRemoveFile(mActualName.GetFullPath());
            ::wxRenameFile(mFilename.GetFullPath(), mActualName.GetFullPath());
         }
      }
      else {
         if ( ! success )
            // Remove any new, and only partially written, file.
            ::wxRemoveFile(mFilename.GetFullPath());
      }
   } );

   std::unique_ptr<ProgressDialog> pDialog;
   auto result = mPlugins[mFormat]->Export(mProject,
                                       pDialog,
                                       mChannels,
                                       mActualName.GetFullPath(),
                                       mSelectedOnly,
                                       mT0,
                                       mT1,
                                       mMixerSpec.get(),
                                       NULL,
                                       mSubFormat);

   success =
      result == ProgressResult::Success || result == ProgressResult::Stopped;

   return success;
}

void Exporter::CreateUserPaneCallback(wxWindow *parent, wxUIntPtr userdata)
{
   Exporter *self = (Exporter *) userdata;
   if (self)
   {
      self->CreateUserPane(parent);
   }
}

void Exporter::CreateUserPane(wxWindow *parent)
{
   ShuttleGui S(parent, eIsCreating);

   S.StartVerticalLay();
   {
      S.StartHorizontalLay(wxEXPAND);
      {
         S.StartStatic(_("Format Options"), 1);
         {
            mBook = safenew wxSimplebook(S.GetParent());
            S.AddWindow(mBook, wxEXPAND);

            for (const auto &pPlugin : mPlugins)
            {
               for (int j = 0; j < pPlugin->GetFormatCount(); j++)
               {
                  mBook->AddPage(pPlugin->OptionsCreate(mBook, j), wxEmptyString);
               }
            }
         }
         S.EndStatic();
      }
      S.EndHorizontalLay();
   }
   S.StartHorizontalLay(wxALIGN_RIGHT, 0);
   {
      S.AddStandardButtons(eHelpButton);
   }
   S.EndHorizontalLay();
   S.EndVerticalLay();

   return;
}

void Exporter::OnFilterChanged(wxFileCtrlEvent & evt)
{
   int index = evt.GetFilterIndex();

   // On GTK, this event can fire before the userpane is created
   if (mBook == NULL || index < 0 || index >= (int) mBook->GetPageCount())
   {
      return;
   }

   mBook->ChangeSelection(index);
}

bool Exporter::ProcessFromTimerRecording(AudacityProject *project,
                                         bool selectedOnly,
                                         double t0,
                                         double t1,
                                         wxFileName fnFile,
                                         int iFormat,
                                         int iSubFormat,
                                         int iFilterIndex)
{
   // Save parms
   mProject = project;
   mSelectedOnly = selectedOnly;
   mT0 = t0;
   mT1 = t1;

   // Auto Export Parameters
   mFilename = fnFile;
   mFormat = iFormat;
   mSubFormat = iSubFormat;
   mFilterIndex = iFilterIndex;

   // Gather track information
   if (!ExamineTracks()) {
      return false;
   }

   // Check for down mixing
   if (!CheckMix()) {
      return false;
   }

   // Ensure filename doesn't interfere with project files.
   if (!CheckFilename()) {
      return false;
   }

   // Export the tracks
   bool success = ExportTracks();

   // Get rid of mixerspec
   mMixerSpec.reset();

   return success;
}

int Exporter::GetAutoExportFormat() {
   return mFormat;
}

int Exporter::GetAutoExportSubFormat() {
   return mSubFormat;
}

int Exporter::GetAutoExportFilterIndex() {
   return mFormat;
}

wxFileName Exporter::GetAutoExportFileName() {
   return mFilename;
}

bool Exporter::SetAutoExportOptions(AudacityProject *project) {
   mFormat = -1;
   mProject = project;

   if( GetFilename()==false )
        return false;

   // Let user edit MetaData
   if (mPlugins[mFormat]->GetCanMetaData(mSubFormat)) {
      if (!(EditActions::DoEditMetadata( *project,
         _("Edit Metadata Tags"),
         _("Exported Tags"),
         ProjectSettings::Get(*mProject).GetShowId3Dialog()))) {
         return false;
      }
   }

   return true;
}

//----------------------------------------------------------------------------
// ExportMixerPanel
//----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(ExportMixerPanel, wxPanelWrapper)
    EVT_PAINT(ExportMixerPanel::OnPaint)
    EVT_MOUSE_EVENTS(ExportMixerPanel::OnMouseEvent)
END_EVENT_TABLE()

ExportMixerPanel::ExportMixerPanel( wxWindow *parent, wxWindowID id,
      MixerSpec *mixerSpec,
      wxArrayString trackNames,
      const wxPoint& pos, const wxSize& size):
   wxPanelWrapper(parent, id, pos, size)
   , mMixerSpec{mixerSpec}
   , mChannelRects{ mMixerSpec->GetMaxNumChannels() }
   , mTrackRects{ mMixerSpec->GetNumTracks() }
{
   mBitmap = NULL;
   mWidth = 0;
   mHeight = 0;
   mSelectedTrack = mSelectedChannel = -1;

   mTrackNames = trackNames;
}

ExportMixerPanel::~ExportMixerPanel()
{
}

//set the font on memDC such that text can fit in specified width and height
void ExportMixerPanel::SetFont(wxMemoryDC &memDC, const wxString &text, int width,
      int height )
{
   int l = 0, u = 13, m, w, h;
   wxFont font = memDC.GetFont();
   while( l < u - 1 )
   {
      m = ( l + u ) / 2;
      font.SetPointSize( m );
      memDC.SetFont( font );
      memDC.GetTextExtent( text, &w, &h );

      if( w < width && h < height )
         l = m;
      else
         u = m;
   }
   font.SetPointSize( l );
   memDC.SetFont( font );
}

void ExportMixerPanel::OnPaint(wxPaintEvent & WXUNUSED(event))
{
   wxPaintDC dc( this );

   int width, height;
   GetSize( &width, &height );

   if( !mBitmap || mWidth != width || mHeight != height )
   {
      mWidth = width;
      mHeight = height;
      mBitmap = std::make_unique<wxBitmap>( mWidth, mHeight,24 );
   }

   wxColour bkgnd = GetBackgroundColour();
   wxBrush bkgndBrush( bkgnd, wxBRUSHSTYLE_SOLID );

   wxMemoryDC memDC;
   memDC.SelectObject( *mBitmap );

   //draw background
   wxRect bkgndRect;
   bkgndRect.x = 0;
   bkgndRect.y = 0;
   bkgndRect.width = mWidth;
   bkgndRect.height = mHeight;

   memDC.SetBrush( *wxWHITE_BRUSH );
   memDC.SetPen( *wxBLACK_PEN );
   memDC.DrawRectangle( bkgndRect );

   //box dimensions
   mBoxWidth = mWidth / 6;

   mTrackHeight = ( mHeight * 3 ) / ( mMixerSpec->GetNumTracks() * 4 );
   if( mTrackHeight > 30 )
      mTrackHeight = 30;

   mChannelHeight = ( mHeight * 3 ) / ( mMixerSpec->GetNumChannels() * 4 );
   if( mChannelHeight > 30 )
      mChannelHeight = 30;

   static double PI = 2 * acos( 0.0 );
   double angle = atan( ( 3.0 * mHeight ) / mWidth );
   double radius = mHeight / ( 2.0 * sin( PI - 2.0 * angle ) );
   double totAngle = ( asin( mHeight / ( 2.0 * radius ) ) * 2.0 );

   //draw tracks
   memDC.SetBrush( AColor::envelopeBrush );
   angle = totAngle / ( mMixerSpec->GetNumTracks() + 1 );

   int max = 0, w, h;
   for( unsigned int i = 1; i < mMixerSpec->GetNumTracks(); i++ )
      if( mTrackNames[ i ].length() > mTrackNames[ max ].length() )
         max = i;

   SetFont( memDC, mTrackNames[ max ], mBoxWidth, mTrackHeight );

   for( unsigned int i = 0; i < mMixerSpec->GetNumTracks(); i++ )
   {
      mTrackRects[ i ].x = (int)( mBoxWidth * 2 + radius - radius *
         cos( totAngle / 2.0 - angle * ( i + 1 ) ) - mBoxWidth + 0.5 );
      mTrackRects[ i ].y = (int)( mHeight * 0.5 - radius *
            sin( totAngle * 0.5 - angle * ( i + 1.0 ) ) -
            0.5 * mTrackHeight + 0.5 );

      mTrackRects[ i ].width = mBoxWidth;
      mTrackRects[ i ].height = mTrackHeight;

      memDC.SetPen( mSelectedTrack == (int)i ? *wxRED_PEN : *wxBLACK_PEN );
      memDC.DrawRectangle( mTrackRects[ i ] );

      memDC.GetTextExtent( mTrackNames[ i ], &w, &h );
      memDC.DrawText( mTrackNames[ i ],
            mTrackRects[ i ].x + ( mBoxWidth - w ) / 2,
            mTrackRects[ i ].y + ( mTrackHeight - h ) / 2 );
   }

   //draw channels
   memDC.SetBrush( AColor::playRegionBrush[ 0 ] );
   angle = ( asin( mHeight / ( 2.0 * radius ) ) * 2.0 ) /
      ( mMixerSpec->GetNumChannels() + 1 );

   SetFont( memDC, wxT( "Channel: XX" ), mBoxWidth, mChannelHeight );
   memDC.GetTextExtent( wxT( "Channel: XX" ), &w, &h );

   for( unsigned int i = 0; i < mMixerSpec->GetNumChannels(); i++ )
   {
      mChannelRects[ i ].x = (int)( mBoxWidth * 4 - radius  + radius *
         cos( totAngle * 0.5 - angle * ( i + 1 ) ) + 0.5 );
      mChannelRects[ i ].y = (int)( mHeight * 0.5 - radius *
            sin( totAngle * 0.5 - angle * ( i + 1 ) ) -
            0.5 * mChannelHeight + 0.5 );

      mChannelRects[ i ].width = mBoxWidth;
      mChannelRects[ i ].height = mChannelHeight;

      memDC.SetPen( mSelectedChannel == (int)i ? *wxRED_PEN : *wxBLACK_PEN );
      memDC.DrawRectangle( mChannelRects[ i ] );

      memDC.DrawText( wxString::Format( _( "Channel: %2d" ), i + 1 ),
            mChannelRects[ i ].x + ( mBoxWidth - w ) / 2,
            mChannelRects[ i ].y + ( mChannelHeight - h ) / 2 );
   }

   //draw links
   memDC.SetPen( wxPen( *wxBLACK, mHeight / 200 ) );
   for( unsigned int i = 0; i < mMixerSpec->GetNumTracks(); i++ )
      for( unsigned int j = 0; j < mMixerSpec->GetNumChannels(); j++ )
         if( mMixerSpec->mMap[ i ][ j ] )
            AColor::Line(memDC, mTrackRects[ i ].x + mBoxWidth,
                  mTrackRects[ i ].y + mTrackHeight / 2, mChannelRects[ j ].x,
                  mChannelRects[ j ].y + mChannelHeight / 2 );

   dc.Blit( 0, 0, mWidth, mHeight, &memDC, 0, 0, wxCOPY, FALSE );
}

double ExportMixerPanel::Distance( wxPoint &a, wxPoint &b )
{
   return sqrt( pow( a.x - b.x, 2.0 ) + pow( a.y - b.y, 2.0 ) );
}

//checks if p is on the line connecting la, lb with tolerence
bool ExportMixerPanel::IsOnLine( wxPoint p, wxPoint la, wxPoint lb )
{
   return Distance( p, la ) + Distance( p, lb ) - Distance( la, lb ) < 0.1;
}

void ExportMixerPanel::OnMouseEvent(wxMouseEvent & event)
{
   if( event.ButtonDown() )
   {
      bool reset = true;
      //check tracks
      for( unsigned int i = 0; i < mMixerSpec->GetNumTracks(); i++ )
         if( mTrackRects[ i ].Contains( event.m_x, event.m_y ) )
         {
            reset = false;
            if( mSelectedTrack == (int)i )
               mSelectedTrack = -1;
            else
            {
               mSelectedTrack = i;
               if( mSelectedChannel != -1 )
                  mMixerSpec->mMap[ mSelectedTrack ][ mSelectedChannel ] =
                     !mMixerSpec->mMap[ mSelectedTrack ][ mSelectedChannel ];
            }
            goto found;
         }

      //check channels
      for( unsigned int i = 0; i < mMixerSpec->GetNumChannels(); i++ )
         if( mChannelRects[ i ].Contains( event.m_x, event.m_y ) )
         {
            reset = false;
            if( mSelectedChannel == (int)i )
               mSelectedChannel = -1;
            else
            {
               mSelectedChannel = i;
               if( mSelectedTrack != -1 )
                  mMixerSpec->mMap[ mSelectedTrack ][ mSelectedChannel ] =
                     !mMixerSpec->mMap[ mSelectedTrack ][ mSelectedChannel ];
            }
            goto found;
         }

      //check links
      for( unsigned int i = 0; i < mMixerSpec->GetNumTracks(); i++ )
         for( unsigned int j = 0; j < mMixerSpec->GetNumChannels(); j++ )
            if( mMixerSpec->mMap[ i ][ j ]  && IsOnLine( wxPoint( event.m_x,
                        event.m_y ), wxPoint( mTrackRects[ i ].x + mBoxWidth,
                           mTrackRects[ i ].y + mTrackHeight / 2 ),
                     wxPoint( mChannelRects[ j ].x, mChannelRects[ j ].y +
                     mChannelHeight / 2 ) ) )
               mMixerSpec->mMap[ i ][ j ] = false;

found:
      if( reset )
         mSelectedTrack = mSelectedChannel = -1;
      Refresh( false );
   }
}

//----------------------------------------------------------------------------
// ExportMixerDialog
//----------------------------------------------------------------------------

enum
{
   ID_MIXERPANEL = 10001,
   ID_SLIDER_CHANNEL
};

BEGIN_EVENT_TABLE( ExportMixerDialog, wxDialogWrapper )
   EVT_BUTTON( wxID_OK, ExportMixerDialog::OnOk )
   EVT_BUTTON( wxID_CANCEL, ExportMixerDialog::OnCancel )
   EVT_BUTTON( wxID_HELP, ExportMixerDialog::OnMixerPanelHelp )
   EVT_SIZE( ExportMixerDialog::OnSize )
   EVT_SLIDER( ID_SLIDER_CHANNEL, ExportMixerDialog::OnSlider )
END_EVENT_TABLE()

ExportMixerDialog::ExportMixerDialog( const TrackList *tracks, bool selectedOnly,
      unsigned maxNumChannels, wxWindow *parent, wxWindowID id, const wxString &title,
      const wxPoint &position, const wxSize& size, long style ) :
   wxDialogWrapper( parent, id, title, position, size, style | wxRESIZE_BORDER )
{
   SetName(GetTitle());

   unsigned numTracks = 0;

   for (auto t :
         tracks->Any< const WaveTrack >()
            + ( selectedOnly ? &Track::IsSelected : &Track::Any )
            - &WaveTrack::GetMute
   ) {
      numTracks++;
      const wxString sTrackName = (t->GetName()).Left(20);
      if( t->GetChannel() == Track::LeftChannel )
      /* i18n-hint: track name and L abbreviating Left channel */
         mTrackNames.push_back( wxString::Format( _( "%s - L" ), sTrackName ) );
      else if( t->GetChannel() == Track::RightChannel )
      /* i18n-hint: track name and R abbreviating Right channel */
         mTrackNames.push_back( wxString::Format( _( "%s - R" ), sTrackName ) );
      else
         mTrackNames.push_back(sTrackName);
   }

   // JKC: This is an attempt to fix a 'watching brief' issue, where the slider is
   // sometimes not slidable.  My suspicion is that a mixer may incorrectly
   // state the number of channels - so we assume there are always at least two.
   // The downside is that if someone is exporting to a mono device, the dialog
   // will allow them to output to two channels. Hmm.  We may need to revisit this.

   if (maxNumChannels < 2 )
      // STF (April 2016): AMR (narrowband) and MP3 may export 1 channel.
      // maxNumChannels = 2;
      maxNumChannels = 1;
   if (maxNumChannels > 32)
      maxNumChannels = 32;

   mMixerSpec = std::make_unique<MixerSpec>(numTracks, maxNumChannels);
   
   wxBoxSizer *vertSizer;
   {
      auto uVertSizer = std::make_unique<wxBoxSizer>(wxVERTICAL);
      vertSizer = uVertSizer.get();

      wxWindow *mixerPanel = safenew ExportMixerPanel(this, ID_MIXERPANEL,
         mMixerSpec.get(), mTrackNames,
         wxDefaultPosition, wxSize(400, -1));
      mixerPanel->SetName(_("Mixer Panel"));
      vertSizer->Add(mixerPanel, 1, wxEXPAND | wxALL, 5);

      {
         auto horSizer = std::make_unique<wxBoxSizer>(wxHORIZONTAL);

         wxString label;
         label.Printf(_("Output Channels: %2d"), mMixerSpec->GetNumChannels());
         mChannelsText = safenew wxStaticText(this, -1, label);
         horSizer->Add(mChannelsText, 0, wxALIGN_LEFT | wxALL, 5);

         wxSlider *channels = safenew wxSliderWrapper(this, ID_SLIDER_CHANNEL,
            mMixerSpec->GetNumChannels(), 1, mMixerSpec->GetMaxNumChannels(),
            wxDefaultPosition, wxSize(300, -1));
         channels->SetName(label);
         horSizer->Add(channels, 0, wxEXPAND | wxALL, 5);

         vertSizer->Add(horSizer.release(), 0, wxALIGN_CENTRE | wxALL, 5);
      }

      vertSizer->Add(CreateStdButtonSizer(this, eCancelButton | eOkButton | eHelpButton).release(), 0, wxEXPAND);

      SetAutoLayout(true);
      SetSizer(uVertSizer.release());
   }

   vertSizer->Fit( this );
   vertSizer->SetSizeHints( this );

   SetSizeHints( 640, 480, 20000, 20000 );

   SetSize( 640, 480 );
   Center();
}

ExportMixerDialog::~ExportMixerDialog()
{
}

void ExportMixerDialog::OnSize(wxSizeEvent &event)
{
   ExportMixerPanel *pnl = ( ( ExportMixerPanel* ) FindWindow( ID_MIXERPANEL ) );
   pnl->Refresh( false );
   event.Skip();
}

void ExportMixerDialog::OnSlider( wxCommandEvent & WXUNUSED(event))
{
   wxSlider *channels = ( wxSlider* )FindWindow( ID_SLIDER_CHANNEL );
   ExportMixerPanel *pnl = ( ( ExportMixerPanel* ) FindWindow( ID_MIXERPANEL ) );
   mMixerSpec->SetNumChannels( channels->GetValue() );
   pnl->Refresh( false );
   wxString label;
   label.Printf( _( "Output Channels: %2d" ), mMixerSpec->GetNumChannels() );
   mChannelsText->SetLabel( label );
   channels->SetName( label );
}

void ExportMixerDialog::OnOk(wxCommandEvent & WXUNUSED(event))
{
   EndModal( wxID_OK );
}

void ExportMixerDialog::OnCancel(wxCommandEvent & WXUNUSED(event))
{
   EndModal( wxID_CANCEL );
}

void ExportMixerDialog::OnMixerPanelHelp(wxCommandEvent & WXUNUSED(event))
{
   HelpSystem::ShowHelp(this, wxT("Advanced_Mixing_Options"), true);
}

