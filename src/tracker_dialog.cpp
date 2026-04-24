#include "tracker_dialog.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <wx/version.h>
#if wxCHECK_VERSION(3,1,6)
#include <wx/bmpbndl.h>
#endif
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/dialog.h>
#include <wx/filename.h>
#include <wx/grid.h>
#include <wx/hyperlink.h>
#include <wx/intl.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statbmp.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/translation.h>
#include <wx/window.h>

#include "1tracker_pi/endpoint_error_summary.h"
#include "1tracker_pi/endpoint_policy.h"
#include "1tracker_pi/endpoint_type_behavior.h"
#include "1tracker_pi/nfl_settings.h"
#include "endpoint_editor_page.h"
#include "endpoint_type_picker.h"
#include "http_json_endpoint_page.h"
#include "nfl_endpoint_page.h"
#include "ocpn_plugin.h"
#include "plugin_ui_utils.h"

namespace {

constexpr int kPreferencesDialogWidth = 576;
constexpr int kPreferencesDialogHeight = 416;
constexpr int kDetailFieldWidth = 260;
constexpr int kDetailLabelWidth = 110;
constexpr int kHeaderHeroWidth = (kPreferencesDialogWidth * 3) / 4;
// Per-type hero dimensions. NFL gets a fixed-size bitmap; the generic
// JSON tracker reuses the plugin's main hero rendered at the same width
// as the info/overview screen.
constexpr int kNflHeroWidth = 281;
constexpr int kNflHeroHeight = 58;
constexpr int kGenericHeroWidth = kHeaderHeroWidth;  // 432

using tracker_pi::summarizeEndpointError;

tracker_pi::EndpointConfig makeDefaultEndpoint(std::size_t index,
                                               const std::string& type) {
  tracker_pi::EndpointConfig endpoint;
  endpoint.id = tracker_pi::makeEndpointId();
  endpoint.name = "endpoint-" + std::to_string(index + 1);
  endpoint.type = type;
  endpoint.enabled = true;
  endpoint.includeAwaAws = true;
  endpoint.timeoutSeconds = 10;
  endpoint.headerName.clear();
  // Apply the chosen type's domain defaults (URL, timeout, min-distance,
  // interval, etc.) before normalising so downstream code sees a complete
  // endpoint regardless of the picked type.
  tracker_pi::getEndpointTypeBehavior(endpoint).applyDefaults(endpoint);
  tracker_pi::normalizeEndpointConfig(endpoint);
  return endpoint;
}

void setFixedLabelWidth(wxStaticText* label) {
  if (label != nullptr) {
    label->SetMinSize(wxSize(kDetailLabelWidth, -1));
  }
}

class ActionIcons {
public:
  explicit ActionIcons(wxWindow* parent)
      : enabledOn(load(parent, "check_mark.svg")),
        statusDisabled(load(parent, "circle-on.svg")),
        statusNoStats(load(parent, "circle-off.svg")),
        statusFailure(load(parent, "X_mult.svg")),
        statusSuccess(load(parent, "check_mark.svg")),
        settings(load(parent, "setting_gear.svg")),
        trash(load(parent, "trash_bin.svg")) {}

  const wxBitmap enabledOn;
  const wxBitmap statusDisabled;
  const wxBitmap statusNoStats;
  const wxBitmap statusFailure;
  const wxBitmap statusSuccess;
  const wxBitmap settings;
  const wxBitmap trash;

private:
  static wxBitmap load(wxWindow* parent, const char* filename) {
    const wxString* sharedData = GetpSharedDataLocation();
    if (sharedData == nullptr || sharedData->empty()) {
      return wxNullBitmap;
    }

    const wxSize size(parent->GetCharHeight(), parent->GetCharHeight());
    wxFileName path(*sharedData, "");
    path.AppendDir("uidata");
    path.AppendDir("MUI_flat");
    path.SetFullName(wxString::FromUTF8(filename));
#if wxCHECK_VERSION(3,1,6)
    return wxBitmapBundle::FromSVGFile(path.GetFullPath(), size).GetBitmap(size);
#else
    return wxNullBitmap;
#endif
  }
};

class BitmapCellRenderer final : public wxGridCellRenderer {
public:
  explicit BitmapCellRenderer(const wxBitmap& bitmap) : bitmap_(bitmap) {}

  void Draw(wxGrid& grid, wxGridCellAttr& attr, wxDC& dc, const wxRect& rect,
            int row, int col, bool isSelected) override {
    wxUnusedVar(grid);
    wxUnusedVar(attr);
    wxUnusedVar(row);
    wxUnusedVar(col);
    wxUnusedVar(isSelected);

    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(*wxWHITE));
    dc.DrawRectangle(rect);

    if (!bitmap_.IsOk()) {
      return;
    }

    dc.DrawBitmap(bitmap_, rect.x + (rect.width - bitmap_.GetWidth()) / 2,
                  rect.y + (rect.height - bitmap_.GetHeight()) / 2, true);
  }

  wxSize GetBestSize(wxGrid&, wxGridCellAttr&, wxDC&, int, int) override {
    return bitmap_.IsOk() ? bitmap_.GetSize() : wxSize(16, 16);
  }

  BitmapCellRenderer* Clone() const override {
    return new BitmapCellRenderer(bitmap_);
  }

private:
  wxBitmap bitmap_;
};

class BitmapTextCellRenderer final : public wxGridCellRenderer {
public:
  BitmapTextCellRenderer(const wxBitmap& bitmap, const wxString& text,
                         const wxColour& textColour)
      : bitmap_(bitmap), text_(text), textColour_(textColour) {}

  void Draw(wxGrid& grid, wxGridCellAttr& attr, wxDC& dc, const wxRect& rect,
            int row, int col, bool isSelected) override {
    wxUnusedVar(grid);
    wxUnusedVar(row);
    wxUnusedVar(col);
    wxUnusedVar(isSelected);

    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(*wxWHITE));
    dc.DrawRectangle(rect);

    dc.SetFont(attr.GetFont());
    dc.SetTextForeground(textColour_);

    const wxSize iconSize = bitmap_.IsOk() ? bitmap_.GetSize() : wxSize(0, 0);
    const wxSize textSize = dc.GetTextExtent(text_);
    const int spacing = bitmap_.IsOk() && !text_.empty() ? 6 : 0;
    const int contentWidth = iconSize.GetWidth() + spacing + textSize.GetWidth();
    const int startX = rect.x + (rect.width - contentWidth) / 2;

    if (bitmap_.IsOk()) {
      dc.DrawBitmap(bitmap_, startX,
                    rect.y + (rect.height - iconSize.GetHeight()) / 2, true);
    }

    dc.DrawText(text_, startX + iconSize.GetWidth() + spacing,
                rect.y + (rect.height - textSize.GetHeight()) / 2);
  }

  wxSize GetBestSize(wxGrid&, wxGridCellAttr& attr, wxDC& dc, int, int) override {
    dc.SetFont(attr.GetFont());
    const wxSize textSize = dc.GetTextExtent(text_);
    const wxSize iconSize = bitmap_.IsOk() ? bitmap_.GetSize() : wxSize(0, 0);
    const int spacing = bitmap_.IsOk() && !text_.empty() ? 6 : 0;
    return wxSize(iconSize.GetWidth() + spacing + textSize.GetWidth(),
                  std::max(iconSize.GetHeight(), textSize.GetHeight()));
  }

  BitmapTextCellRenderer* Clone() const override {
    return new BitmapTextCellRenderer(bitmap_, text_, textColour_);
  }

private:
  wxBitmap bitmap_;
  wxString text_;
  wxColour textColour_;
};

class BitmapEnableCellRenderer final : public wxGridCellBoolRenderer {
public:
  explicit BitmapEnableCellRenderer(int size) : size_(size) {}

  BitmapEnableCellRenderer* Clone() const override {
    return new BitmapEnableCellRenderer(size_);
  }

  wxSize GetBestSize(wxGrid&, wxGridCellAttr&, wxDC&, int, int) override {
    return {size_, size_};
  }

private:
  int size_;
};

}  // namespace

class TrackerDialog final : public wxDialog {
public:
  TrackerDialog(
      wxWindow* parent, const std::filesystem::path& configPath,
      const tracker_pi::RuntimeConfig& initialConfig,
      const std::map<std::string, tracker_pi::EndpointUiState>& endpointStatuses,
      std::function<bool(const tracker_pi::RuntimeConfig&, std::string*)> applyConfigFn,
      TrackerDialogEntryMode entryMode, std::function<void()> onClose)
      : wxDialog(parent, wxID_ANY,
                 entryMode == TrackerDialogEntryMode::Preferences
                     ? _("1tracker Preferences")
                     : _("Tracking"),
                 wxDefaultPosition,
                 wxSize(kPreferencesDialogWidth, kPreferencesDialogHeight),
                 dialogStyleFor(entryMode)),
        configPath_(configPath),
        config_(initialConfig),
        endpointStatuses_(endpointStatuses),
        entryMode_(entryMode),
        applyConfigFn_(std::move(applyConfigFn)),
        onClose_(std::move(onClose)) {
    buildUi();
    refreshEndpointList();
    if (!config_.endpoints.empty()) {
      selectEndpointRow(0);
      loadEndpointControls(0);
    } else {
      enableEndpointEditor(false);
    }

    if (entryMode_ == TrackerDialogEntryMode::Preferences) {
      showInfoMode();
    }

    Bind(wxEVT_CLOSE_WINDOW, &TrackerDialog::onWindowClose, this);
  }

  tracker_pi::RuntimeConfig GetConfig() {
    saveCurrentEndpoint();
    return config_;
  }

private:
  static long dialogStyleFor(TrackerDialogEntryMode entryMode) {
    wxUnusedVar(entryMode);
    return wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxSTAY_ON_TOP;
  }

  bool applyChanges(bool closeOnSuccess) {
    if (!applyConfigFn_) {
      if (closeOnSuccess) {
        Close();
      }
      return true;
    }

    const auto updatedConfig = GetConfig();
    std::string errorMessage;
    if (!applyConfigFn_(updatedConfig, &errorMessage)) {
      wxMessageBox(wxString::FromUTF8(errorMessage.c_str()), "1tracker",
                   wxOK | wxICON_ERROR, this);
      return false;
    }

    refreshEndpointList();
    if (currentEndpointIndex_ != wxNOT_FOUND &&
        static_cast<std::size_t>(currentEndpointIndex_) < config_.endpoints.size()) {
      refreshEndpointRow(currentEndpointIndex_);
      selectEndpointRow(currentEndpointIndex_);
    }
    showListMode();

    if (closeOnSuccess) {
      Close();
    }

    return true;
  }

  bool persistCurrentConfig() {
    if (!applyConfigFn_) {
      return true;
    }

    std::string errorMessage;
    if (!applyConfigFn_(config_, &errorMessage)) {
      wxMessageBox(wxString::FromUTF8(errorMessage.c_str()), "1tracker",
                   wxOK | wxICON_ERROR, this);
      return false;
    }

    return true;
  }

  void selectEndpointRow(int index) {
    if (index == wxNOT_FOUND) {
      return;
    }

    suppressSelectionEvents_ = true;
    endpointGrid_->SetGridCursor(index, 1);
    endpointGrid_->MakeCellVisible(index, 1);
    endpointGrid_->ClearSelection();
    suppressSelectionEvents_ = false;
  }

  void configureEndpointGrid() {
    actionIcons_ = std::make_unique<ActionIcons>(this);
    endpointGrid_->ShowScrollbars(wxSHOW_SB_NEVER, wxSHOW_SB_NEVER);
    endpointGrid_->CreateGrid(0, 7);
    endpointGrid_->HideRowLabels();
    endpointGrid_->SetColLabelValue(0, "");
    endpointGrid_->SetColLabelValue(1, _("Name"));
    endpointGrid_->SetColLabelValue(2, _("Send interval"));
    endpointGrid_->SetColLabelValue(3, _("Last successful send"));
    endpointGrid_->SetColLabelValue(4, _("Status"));
    endpointGrid_->SetColLabelValue(5, "");
    endpointGrid_->SetColLabelValue(6, "");
    applyGridColumnSizing();
    applyGridAppearance();
    configureGridColumns();
  }

  void buildListPanel(wxBoxSizer* contentSizer) {
    listPanel_ = new wxPanel(this, wxID_ANY);
    auto* listSizer = new wxBoxSizer(wxVERTICAL);
    auto* headerRow = new wxBoxSizer(wxHORIZONTAL);
    auto* headerHero = new wxStaticBitmap(
        listPanel_, wxID_ANY,
        tracker_plugin_ui::LoadBitmapFromPluginAssetWidth("1tracker_hero.png",
                                                          kHeaderHeroWidth));
    headerRow->Add(headerHero, 1, wxALIGN_CENTER_VERTICAL);
    listSizer->Add(headerRow, 0, wxEXPAND | wxBOTTOM, 10);

    endpointGrid_ = new wxGrid(listPanel_, wxID_ANY);
    configureEndpointGrid();
    listSizer->Add(endpointGrid_, 0, wxEXPAND | wxBOTTOM, 8);

    listButtonRow_ = new wxBoxSizer(wxHORIZONTAL);
    addButton_ = new wxButton(listPanel_, wxID_ADD, _("Add tracker"));
    closeButton_ = new wxButton(listPanel_, wxID_CANCEL, _("Close"));
    listButtonRow_->Add(addButton_, 0);
    listButtonRow_->AddStretchSpacer(1);
    listButtonRow_->Add(closeButton_, 0);
    listSizer->Add(listButtonRow_, 0, wxEXPAND);

    listPanel_->SetSizer(listSizer);
    contentSizer->Add(listPanel_, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 12);
  }

  wxFlexGridSizer* createDetailForm() {
    auto* form = new wxFlexGridSizer(0, 2, 8, 12);
    form->AddGrowableCol(1, 1);
    return form;
  }

  wxTextCtrl* createSizedTextCtrl(wxWindow* parent) {
    auto* ctrl = new wxTextCtrl(parent, wxID_ANY);
    ctrl->SetMinSize(wxSize(kDetailFieldWidth, -1));
    ctrl->SetMaxSize(wxSize(kDetailFieldWidth, -1));
    return ctrl;
  }

  void addDetailLabel(wxFlexGridSizer* form, wxStaticText*& label,
                      const wxString& text) {
    label = new wxStaticText(detailPanel_, wxID_ANY, text);
    setFixedLabelWidth(label);
    form->Add(label, 0, wxALIGN_CENTER_VERTICAL);
  }

  // Loads the hero bitmap for a given type. NFL gets its dedicated header
  // art at 281×58; every other type (currently just the generic HTTP
  // JSON tracker) uses the plugin's main `1tracker_hero.png` rendered at
  // the same width as the overview screen's hero (432 px), aspect-preserved.
  // The shared container reserves the max dimensions so the per-type
  // bitmap swap never reflows the dialog.
  wxBitmap heroBitmapForType(const std::string& type) const {
    if (tracker_pi::isNoForeignLandType(type)) {
      return tracker_plugin_ui::LoadBitmapFromPluginAsset(
          "1tracker_nfl_header.png",
          wxSize(kNflHeroWidth, kNflHeroHeight));
    }
    return tracker_plugin_ui::LoadBitmapFromPluginAssetWidth(
        "1tracker_hero.png", kGenericHeroWidth);
  }

  // Dialog-level hero sitting above Name + Type. Mirrors the list/overview
  // panel's hero layout exactly — wxStaticBitmap inside a horizontal row
  // with proportion=1 — so the hero sits identically on both screens.
  // Since type is chosen at creation and immutable thereafter, the bitmap
  // only changes when the user opens a different existing tracker;
  // updateDialogSizeForCurrentMode refits the frame.
  void buildSharedHero(wxBoxSizer* column) {
    auto* headerRow = new wxBoxSizer(wxHORIZONTAL);
    detailHero_ = new wxStaticBitmap(
        detailPanel_, wxID_ANY,
        heroBitmapForType(tracker_pi::kEndpointTypeNoForeignLand));
    headerRow->Add(detailHero_, 1, wxALIGN_CENTER_VERTICAL);
    column->Add(headerRow, 0, wxEXPAND | wxBOTTOM, 10);
  }

  // Builds the two shared-across-all-types fields: editable Name and a
  // read-only Type display. Type is immutable after creation (the picker
  // asks for it at Add time), so it renders as plain text rather than a
  // combobox.
  void buildSharedIdentityFields(wxBoxSizer* column) {
    auto* form = createDetailForm();

    addDetailLabel(form, endpointNameLabel_, _("Name"));
    endpointNameCtrl_ = createSizedTextCtrl(detailPanel_);
    form->Add(endpointNameCtrl_, 1, wxEXPAND);

    addDetailLabel(form, endpointTypeLabel_, _("Type"));
    endpointTypeText_ = new wxStaticText(detailPanel_, wxID_ANY, "");
    endpointTypeText_->SetMinSize(wxSize(kDetailFieldWidth, -1));
    // 4px left padding so the Type text aligns visually with the Name
    // textbox's content above (textboxes have ~4px internal padding,
    // static texts have none — without this nudge the two rows look
    // staggered).
    form->Add(endpointTypeText_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);

    column->Add(form, 0, wxEXPAND | wxALL, 4);
  }

  // Builds the wxSimplebook and registers one EndpointEditorPage per
  // known tracker type. Each page is a self-contained wxPanel owning its
  // hero image, type-specific fields and in-page error display. Switching
  // the selected page replaces the visible editor wholesale.
  //
  // Adding a new tracker type: append one more `endpointPages_.push_back(
  // new tracker_plugin_ui::FooEndpointPage(...))` line below, and ensure
  // `tracker_pi::listEndpointTypes()` includes the new type so the combo
  // picks it up.
  void buildEndpointEditorBook(wxBoxSizer* column) {
    endpointEditorBook_ =
        new wxSimplebook(detailPanel_, wxID_ANY, wxDefaultPosition,
                         wxDefaultSize, 0);
    endpointPages_.push_back(
        new tracker_plugin_ui::NflEndpointPage(endpointEditorBook_));
    endpointPages_.push_back(
        new tracker_plugin_ui::HttpJsonEndpointPage(endpointEditorBook_));
    for (auto* page : endpointPages_) {
      endpointEditorBook_->AddPage(page, "", false);
      // Let pages tell the dialog when their own size requirements change
      // (e.g. when the error-details toggle expands/collapses the
      // technical-details panel). Refit so the dialog frame grows or
      // shrinks with the visible page content.
      page->setOnContentChanged([this]() { updateDialogSizeForCurrentMode(); });
    }
    // No wxALL margin on the book itself — each page adds its own 4px
    // around its form. If the book were also padded, page fields would
    // sit 4px further right than the shared Name/Type form, making the
    // Name textbox look left-shifted relative to the rest.
    column->Add(endpointEditorBook_, 1, wxEXPAND);
  }

  // Returns the index of the page whose `type()` matches `type`, or -1 if
  // no matching page is registered.
  int pageIndexForType(const std::string& type) const {
    for (std::size_t i = 0; i < endpointPages_.size(); ++i) {
      if (endpointPages_[i] != nullptr &&
          endpointPages_[i]->type() == type) {
        return static_cast<int>(i);
      }
    }
    return -1;
  }

  tracker_plugin_ui::EndpointEditorPage* currentEditorPage() const {
    if (endpointEditorBook_ == nullptr) return nullptr;
    const int sel = endpointEditorBook_->GetSelection();
    if (sel < 0 || sel >= static_cast<int>(endpointPages_.size())) {
      return nullptr;
    }
    return endpointPages_[static_cast<std::size_t>(sel)];
  }

  void buildDetailPanel(wxBoxSizer* contentSizer) {
    detailPanel_ = new wxPanel(this, wxID_ANY, wxDefaultPosition,
                               wxDefaultSize, wxTAB_TRAVERSAL);
    auto* detailColumn = new wxBoxSizer(wxVERTICAL);
    buildSharedHero(detailColumn);
    buildSharedIdentityFields(detailColumn);
    buildEndpointEditorBook(detailColumn);
    detailPanel_->SetSizer(detailColumn);
    contentSizer->Add(detailPanel_, 1, wxLEFT | wxRIGHT | wxTOP | wxEXPAND, 12);
  }

  void buildInfoHeader(wxBoxSizer* infoSizer) {
    auto* infoHero = new wxStaticBitmap(
        infoPanel_, wxID_ANY,
        tracker_plugin_ui::LoadBitmapFromPluginAssetWidth("1tracker_hero.png",
                                                          kHeaderHeroWidth));
    infoSizer->Add(infoHero, 0, wxBOTTOM, 16);
  }

  void buildInfoBody(wxBoxSizer* infoSizer) {
    auto* infoBody = new wxStaticText(
        infoPanel_, wxID_ANY,
        _("The 1tracker plugin automatically sends your location to a website "
          "via API calls. You can configure a custom website, or a standard "
          "noforeignland endpoint."));
    infoBody->Wrap(kPreferencesDialogWidth - 80);
    infoSizer->Add(infoBody, 0, wxBOTTOM, 8);

    auto* manualLabel =
        new wxStaticText(infoPanel_, wxID_ANY, _("Online manual: "));
    infoSizer->Add(manualLabel, 0);
    infoManualLink_ = new wxHyperlinkCtrl(
        infoPanel_, wxID_ANY,
        "github.com/pa2wlt/1tracker_pi/blob/master/docs/manual.md",
        "https://github.com/pa2wlt/1tracker_pi/blob/master/docs/manual.md");
    infoManualLink_->SetNormalColour(wxColour(20, 78, 140));
    infoManualLink_->SetHoverColour(wxColour(14, 58, 106));
    infoManualLink_->SetVisitedColour(wxColour(84, 62, 132));
    infoSizer->Add(infoManualLink_, 0, wxBOTTOM, 4);

    auto* issuesLabel =
        new wxStaticText(infoPanel_, wxID_ANY, _("Report issues or requests: "));
    infoSizer->Add(issuesLabel, 0);
    infoIssuesLink_ = new wxHyperlinkCtrl(
        infoPanel_, wxID_ANY, "github.com/pa2wlt/1tracker_pi/issues",
        "https://github.com/pa2wlt/1tracker_pi/issues");
    infoIssuesLink_->SetNormalColour(wxColour(20, 78, 140));
    infoIssuesLink_->SetHoverColour(wxColour(14, 58, 106));
    infoIssuesLink_->SetVisitedColour(wxColour(84, 62, 132));
    infoSizer->Add(infoIssuesLink_, 0, wxBOTTOM, 12);
  }

  void buildInfoConfigPath(wxBoxSizer* infoSizer) {
    auto* infoConfigText = new wxStaticText(
        infoPanel_, wxID_ANY,
        _("Settings are stored in a configuration file here on your local machine: "));
    infoConfigText->Wrap(kPreferencesDialogWidth - 80);
    infoSizer->Add(infoConfigText, 0, wxBOTTOM, 4);
    const wxString configPathString =
        wxString::FromUTF8(configPath_.string().c_str());
    infoConfigPathLink_ = new wxHyperlinkCtrl(
        infoPanel_, wxID_ANY, configPathString, "file://" + configPathString);
    infoConfigPathLink_->SetNormalColour(wxColour(20, 78, 140));
    infoConfigPathLink_->SetHoverColour(wxColour(14, 58, 106));
    infoConfigPathLink_->SetVisitedColour(wxColour(84, 62, 132));
    infoSizer->Add(infoConfigPathLink_, 0, wxBOTTOM, 16);
  }

  void buildInfoButtons(wxBoxSizer* infoSizer) {
    infoSizer->AddStretchSpacer(1);
    infoButtonRow_ = new wxBoxSizer(wxHORIZONTAL);
    infoCloseButton_ = new wxButton(infoPanel_, wxID_ANY, _("Close"));
    infoButtonRow_->AddStretchSpacer(1);
    infoButtonRow_->Add(infoCloseButton_, 0);
    infoSizer->Add(infoButtonRow_, 0, wxEXPAND | wxTOP | wxBOTTOM, 8);
  }

  void buildInfoPanel(wxBoxSizer* contentSizer) {
    infoPanel_ = new wxPanel(this, wxID_ANY);
    auto* infoSizer = new wxBoxSizer(wxVERTICAL);
    buildInfoHeader(infoSizer);
    buildInfoBody(infoSizer);
    buildInfoConfigPath(infoSizer);
    buildInfoButtons(infoSizer);
    infoPanel_->SetSizer(infoSizer);
    contentSizer->Add(infoPanel_, 1, wxALL | wxEXPAND, 12);
  }

  void buildButtonRow(wxBoxSizer* root) {
    buttonRow_ = new wxBoxSizer(wxHORIZONTAL);
    detailHelpButton_ = new wxButton(this, wxID_ANY, _("Help"));
    detailCancelButton_ = new wxButton(this, wxID_ANY, _("Cancel"));
    detailOkButton_ = new wxButton(this, wxID_ANY, _("OK"));
    buttonRow_->Add(detailHelpButton_, 0);
    buttonRow_->AddStretchSpacer(1);
    buttonRow_->Add(detailCancelButton_, 0, wxLEFT, 8);
    buttonRow_->Add(detailOkButton_, 0, wxLEFT, 8);
    root->Add(buttonRow_, 0, wxALL | wxEXPAND, 12);
  }

  void applyGridColumnSizing() {
    endpointGrid_->SetColSize(0, 31);
    endpointGrid_->SetColSize(1, 155);
    endpointGrid_->SetColSize(2, 140);
    endpointGrid_->SetColSize(3, 150);
    endpointGrid_->SetColSize(4, 94);
    endpointGrid_->SetColSize(5, 30);
    endpointGrid_->SetColSize(6, 30);
  }

  void applyGridAppearance() {
    endpointGrid_->SetDefaultRowSize(std::max(28, GetCharHeight() * 2));
    endpointGrid_->SetColLabelSize(std::max(30, GetCharHeight() * 2));
    endpointGrid_->SetColLabelAlignment(wxALIGN_LEFT, wxALIGN_CENTRE);
    endpointGrid_->DisableDragColSize();
    endpointGrid_->DisableDragRowSize();
    endpointGrid_->DisableDragGridSize();
    endpointGrid_->EnableEditing(false);
    endpointGrid_->SetSelectionMode(wxGrid::wxGridSelectRows);
    endpointGrid_->SetRowLabelSize(0);
    endpointGrid_->SetDefaultCellBackgroundColour(*wxWHITE);
    endpointGrid_->SetDefaultCellTextColour(*wxBLACK);
    endpointGrid_->SetSelectionBackground(*wxWHITE);
    endpointGrid_->SetSelectionForeground(*wxBLACK);
    if (tracker_plugin_ui::IsWindowsPlatform()) {
      wxColour labelBg;
      wxColour labelFg;
      if (GetGlobalColor("DILG1", &labelBg)) {
        endpointGrid_->SetLabelBackgroundColour(labelBg);
      }
      if (GetGlobalColor("DILG3", &labelFg)) {
        endpointGrid_->SetLabelTextColour(labelFg);
      }
    }
    wxFont labelFont = endpointGrid_->GetLabelFont();
    labelFont.SetWeight(wxFONTWEIGHT_BOLD);
    endpointGrid_->SetLabelFont(labelFont);
    endpointGrid_->SetDefaultCellFont(GetFont());
  }

  void configureGridColumns() {
    auto* enableAttr = new wxGridCellAttr();
    enableAttr->SetAlignment(wxALIGN_CENTRE, wxALIGN_CENTRE);
    enableAttr->SetRenderer(new BitmapEnableCellRenderer(GetCharHeight()));
    enableAttr->SetEditor(new wxGridCellBoolEditor());
    endpointGrid_->SetColAttr(0, enableAttr);

    auto* trackerAttr = new wxGridCellAttr();
    trackerAttr->SetAlignment(wxALIGN_LEFT, wxALIGN_CENTRE);
    trackerAttr->SetReadOnly(true);
    endpointGrid_->SetColAttr(1, trackerAttr);

    auto* intervalAttr = new wxGridCellAttr();
    intervalAttr->SetAlignment(wxALIGN_LEFT, wxALIGN_CENTRE);
    intervalAttr->SetReadOnly(true);
    endpointGrid_->SetColAttr(2, intervalAttr);

    auto* lastSentAttr = new wxGridCellAttr();
    lastSentAttr->SetAlignment(wxALIGN_LEFT, wxALIGN_CENTRE);
    lastSentAttr->SetReadOnly(true);
    endpointGrid_->SetColAttr(3, lastSentAttr);

    auto* statusAttr = new wxGridCellAttr();
    statusAttr->SetAlignment(wxALIGN_CENTRE, wxALIGN_CENTRE);
    statusAttr->SetFont(GetFont());
    statusAttr->SetReadOnly(true);
    endpointGrid_->SetColAttr(4, statusAttr);

    auto* settingsAttr = new wxGridCellAttr();
    settingsAttr->SetAlignment(wxALIGN_CENTRE, wxALIGN_CENTRE);
    settingsAttr->SetFont(GetFont().Scale(1.3));
    settingsAttr->SetReadOnly(true);
    endpointGrid_->SetColAttr(5, settingsAttr);

    auto* trashAttr = new wxGridCellAttr();
    trashAttr->SetAlignment(wxALIGN_CENTRE, wxALIGN_CENTRE);
    trashAttr->SetFont(GetFont().Scale(1.3));
    trashAttr->SetReadOnly(true);
    endpointGrid_->SetColAttr(6, trashAttr);
  }

  void configureDetailTabOrder() {
    // Type is read-only text now and no longer a focusable control, so
    // there is nothing to splice into the tab order at the dialog level;
    // wx's default traversal handles Name → first focusable widget on the
    // active page.
  }

  void focusTabTarget(wxWindow* target, bool fromKeyboard) {
    if (target == nullptr || !target->IsEnabled() || !target->IsShown()) {
      return;
    }

    if (fromKeyboard) {
      target->SetFocusFromKbd();
      return;
    }

    target->SetFocus();
  }

  struct TabHop {
    wxWindow* target = nullptr;
    bool fromKeyboard = false;
  };

  bool tryTabHop(const TabHop& hop) {
    if (hop.target == nullptr || !hop.target->IsEnabled() ||
        !hop.target->IsShown()) {
      return false;
    }
    focusTabTarget(hop.target, hop.fromKeyboard);
    return true;
  }

  void bindTabNavigation(wxWindow* source, wxWindow* forwardTarget,
                         wxWindow* backwardTarget = nullptr,
                         bool forwardFromKeyboard = false,
                         bool backwardFromKeyboard = false) {
    if (source == nullptr) {
      return;
    }

    const TabHop forwardHop{forwardTarget, forwardFromKeyboard};
    const TabHop backwardHop{backwardTarget, backwardFromKeyboard};

    source->Bind(wxEVT_KEY_DOWN, [this, forwardHop, backwardHop](wxKeyEvent& event) {
      if (event.GetKeyCode() != WXK_TAB) {
        event.Skip();
        return;
      }
      const TabHop& hop = event.ShiftDown() ? backwardHop : forwardHop;
      if (!tryTabHop(hop)) {
        event.Skip();
      }
    });
  }

  void bindDetailTabNavigation() {
    // No dialog-level tab hops needed: Type is static text and pages
    // own their internal focus order.
  }

  void bindDetailFieldEvents() {
    configureDetailTabOrder();
    bindDetailTabNavigation();
    // Type is immutable after creation — no combo event to bind.
  }

  void bindGridEvents() {
    endpointGrid_->Bind(wxEVT_GRID_SELECT_CELL,
                        &TrackerDialog::onEndpointSelected, this);
    endpointGrid_->Bind(wxEVT_GRID_CELL_LEFT_CLICK,
                        &TrackerDialog::onEndpointGridClick, this);
  }

  void bindListEscapeHandling() {
    auto bindEscape = [this](wxWindow* window) {
      if (window == nullptr) {
        return;
      }

      window->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event) {
        if (event.GetKeyCode() != WXK_ESCAPE) {
          event.Skip();
          return;
        }

        wxCommandEvent commandEvent;
        onCloseDialog(commandEvent);
      });
    };

    bindEscape(endpointGrid_);
    bindEscape(addButton_);
    bindEscape(closeButton_);
  }

  void bindDialogButtonEvents() {
    addButton_->Bind(wxEVT_BUTTON, &TrackerDialog::onAddEndpoint, this);
    closeButton_->Bind(wxEVT_BUTTON, &TrackerDialog::onCloseDialog, this);
    infoCloseButton_->Bind(wxEVT_BUTTON, &TrackerDialog::onCloseInfo, this);
    detailHelpButton_->Bind(wxEVT_BUTTON, &TrackerDialog::onShowInfo, this);
    detailCancelButton_->Bind(wxEVT_BUTTON, &TrackerDialog::onDetailCancel, this);
    detailOkButton_->Bind(wxEVT_BUTTON, &TrackerDialog::onDetailOk, this);
  }

  void bindEscapeHandling() {
    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event) {
      if (event.GetKeyCode() != WXK_ESCAPE) {
        event.Skip();
        return;
      }

      wxCommandEvent commandEvent;
      if (detailPanel_ != nullptr && detailPanel_->IsShown()) {
        onDetailCancel(commandEvent);
        return;
      }

      if (infoPanel_ != nullptr && infoPanel_->IsShown()) {
        onCloseInfo(commandEvent);
        return;
      }

      onCloseDialog(commandEvent);
    });
  }

  void bindEvents() {
    bindDetailFieldEvents();
    bindGridEvents();
    bindDialogButtonEvents();
    bindListEscapeHandling();
    bindEscapeHandling();
  }

  void buildUi() {
    auto* root = new wxBoxSizer(wxVERTICAL);
    root->Add(new wxStaticLine(this), 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND,
              12);

    auto* contentSizer = new wxBoxSizer(wxVERTICAL);
    buildListPanel(contentSizer);
    buildDetailPanel(contentSizer);
    buildInfoPanel(contentSizer);

    root->Add(contentSizer, 1, wxEXPAND);
    buildButtonRow(root);

    SetSizer(root);
    Layout();
    Fit();
    SetMinSize(wxSize(kPreferencesDialogWidth, GetSize().GetHeight()));
    SetSize(wxSize(std::max(kPreferencesDialogWidth, GetSize().GetWidth()),
                   GetSize().GetHeight()));
    bindEvents();
    showListMode();
  }

  void refreshEndpointList() {
    const int currentRows = endpointGrid_->GetNumberRows();
    const int targetRows = static_cast<int>(config_.endpoints.size());
    if (currentRows < targetRows) {
      endpointGrid_->AppendRows(targetRows - currentRows);
    } else if (currentRows > targetRows) {
      endpointGrid_->DeleteRows(0, currentRows - targetRows);
    }

    for (std::size_t i = 0; i < config_.endpoints.size(); ++i) {
      refreshEndpointRow(static_cast<int>(i));
    }
    updateGridHeight();
  }

  void updateGridHeight() {
    const int rows = std::max(1, endpointGrid_->GetNumberRows());
    int width = 0;
    for (int col = 0; col < endpointGrid_->GetNumberCols(); ++col) {
      if (!endpointGrid_->IsColShown(col)) {
        continue;
      }
      width += endpointGrid_->GetColSize(col);
    }
    width += 4;
    const int height = endpointGrid_->GetColLabelSize() +
                       rows * endpointGrid_->GetDefaultRowSize() + 4;
    endpointGrid_->SetMinSize(wxSize(width, height));
    if (listPanel_ != nullptr) {
      listPanel_->Layout();
    }
    updateDialogSizeForCurrentMode();
    Layout();
  }

  tracker_pi::EndpointUiStatus endpointRowStatus(
      const tracker_pi::EndpointConfig& endpoint) const {
    if (!endpoint.enabled) {
      return tracker_pi::EndpointUiStatus::Disabled;
    }

    const auto statusIt =
        endpointStatuses_.find(tracker_pi::endpointStateKey(endpoint));
    return statusIt != endpointStatuses_.end()
               ? statusIt->second.status
               : tracker_pi::EndpointUiStatus::NoStats;
  }

  wxString endpointRowLastSent(const tracker_pi::EndpointConfig& endpoint) const {
    const auto statusIt =
        endpointStatuses_.find(tracker_pi::endpointStateKey(endpoint));
    return statusIt != endpointStatuses_.end()
               ? wxString::FromUTF8(statusIt->second.lastSentLocalTime.c_str())
               : "";
  }

  wxString endpointRowLabel(const tracker_pi::EndpointConfig& endpoint,
                            int index) const {
    return endpoint.name.empty()
               ? wxString::Format("endpoint-%d", index + 1)
               : wxString::FromUTF8(endpoint.name.c_str());
  }

  wxString endpointRowInterval(
      const tracker_pi::EndpointConfig& endpoint) const {
    // Plural form is crude (no wxPLURAL); if a language needs separate
    // grammar for 1 vs N, the two keys can be translated independently.
    return wxString::Format("%d %s", endpoint.sendIntervalMinutes,
                            endpoint.sendIntervalMinutes == 1 ? _("minute")
                                                              : _("minutes"));
  }

  void setEndpointRowValues(int index, const tracker_pi::EndpointConfig& endpoint,
                            const wxString& lastSent) {
    endpointGrid_->SetCellValue(index, 0, endpoint.enabled ? "1" : "");
    endpointGrid_->SetCellValue(index, 1, endpointRowLabel(endpoint, index));
    endpointGrid_->SetCellValue(index, 2, endpointRowInterval(endpoint));
    endpointGrid_->SetCellValue(index, 3, lastSent);
    endpointGrid_->SetCellValue(index, 4, "");
    endpointGrid_->SetCellValue(index, 5, "");
    endpointGrid_->SetCellValue(index, 6, "");
  }

  void setEndpointRowRenderers(int index, tracker_pi::EndpointUiStatus status) {
    endpointGrid_->SetCellRenderer(
        index, 0, new BitmapEnableCellRenderer(GetCharHeight()));
    endpointGrid_->SetCellRenderer(
        index, 4,
        new BitmapTextCellRenderer(statusBitmap(status), statusLabel(status),
                                   statusTextColour(status)));
    endpointGrid_->SetCellRenderer(index, 5,
                                   new BitmapCellRenderer(actionIcons_->settings));
    endpointGrid_->SetCellRenderer(index, 6,
                                   new BitmapCellRenderer(actionIcons_->trash));
  }

  void refreshEndpointRow(int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= config_.endpoints.size()) {
      return;
    }

    const auto& endpoint = config_.endpoints[static_cast<std::size_t>(index)];
    const auto status = endpointRowStatus(endpoint);
    setEndpointRowValues(index, endpoint, endpointRowLastSent(endpoint));
    setEndpointRowRenderers(index, status);
  }

  wxBitmap statusBitmap(tracker_pi::EndpointUiStatus status) const {
    switch (status) {
      case tracker_pi::EndpointUiStatus::Disabled:
        return actionIcons_->statusNoStats;
      case tracker_pi::EndpointUiStatus::Failure:
        return tracker_plugin_ui::TintBitmap(actionIcons_->statusFailure,
                                             wxColour(237, 59, 42));
      case tracker_pi::EndpointUiStatus::Success:
        return tracker_plugin_ui::TintBitmap(actionIcons_->statusSuccess,
                                             wxColour(39, 174, 96));
      case tracker_pi::EndpointUiStatus::NoStats:
      default:
        return actionIcons_->statusNoStats;
    }
  }

  wxString statusLabel(tracker_pi::EndpointUiStatus status) const {
    switch (status) {
      case tracker_pi::EndpointUiStatus::Success:
        return _("Active");
      case tracker_pi::EndpointUiStatus::Failure:
        return _("Issue");
      case tracker_pi::EndpointUiStatus::Disabled:
      case tracker_pi::EndpointUiStatus::NoStats:
      default:
        return _("Inactive");
    }
  }

  wxColour statusTextColour(tracker_pi::EndpointUiStatus status) const {
    switch (status) {
      case tracker_pi::EndpointUiStatus::Success:
        return wxColour(39, 174, 96);
      case tracker_pi::EndpointUiStatus::Failure:
        return wxColour(237, 59, 42);
      case tracker_pi::EndpointUiStatus::Disabled:
      case tracker_pi::EndpointUiStatus::NoStats:
      default:
        return *wxBLACK;
    }
  }

  void updateDialogSizeForCurrentMode() {
    if (GetSizer() == nullptr) {
      return;
    }

    Layout();
    const wxSize fitted = GetSizer()->ComputeFittingWindowSize(this);
    const int dialogWidth = std::max(kPreferencesDialogWidth, fitted.GetWidth());
    const int dialogHeight = fitted.GetHeight();
    SetMinSize(wxSize(kPreferencesDialogWidth, dialogHeight));
    SetSize(wxSize(dialogWidth, dialogHeight));
  }

  // User-facing label for a tracker type. Mirrors the picker's labels so
  // the static Type field in the editor shows the same friendly name the
  // user picked at creation time.
  static wxString displayLabelForType(const std::string& type) {
    if (type == tracker_pi::kEndpointTypeNoForeignLand) {
      return _("NoForeignLand");
    }
    if (type == tracker_pi::kEndpointTypeHttpJsonWithHeaderKey) {
      return _("Generic HTTP JSON with header key");
    }
    return wxString::FromUTF8(type.c_str());
  }

  void setWindowShown(wxWindow* window, bool shown) {
    if (window != nullptr) {
      window->Show(shown);
    }
  }

  void populateEditorFromEndpoint(const tracker_pi::EndpointConfig& endpoint) {
    endpointNameCtrl_->SetValue(wxString::FromUTF8(endpoint.name.c_str()));
    if (endpointTypeText_ != nullptr) {
      endpointTypeText_->SetLabel(
          displayLabelForType(endpoint.type));
    }
    if (detailHero_ != nullptr) {
      detailHero_->SetBitmap(heroBitmapForType(endpoint.type));
    }
    // Switch the book to the page owning this type and let the page
    // pull its fields from the endpoint. This is the single path that
    // selects the book page — type is immutable after creation so there
    // is no on-combo-event path to keep in sync.
    const int pageIndex = pageIndexForType(endpoint.type);
    if (pageIndex >= 0 && endpointEditorBook_ != nullptr) {
      endpointEditorBook_->SetSelection(static_cast<std::size_t>(pageIndex));
      endpointPages_[static_cast<std::size_t>(pageIndex)]->populate(endpoint);
    }
  }

  tracker_pi::EndpointConfig readEndpointFromEditor() const {
    tracker_pi::EndpointConfig endpoint;
    endpoint.name = endpointNameCtrl_->GetValue().ToStdString();
    // Type is immutable across an editor session — source it from whichever
    // page is currently visible in the book.
    if (auto* page = currentEditorPage()) {
      endpoint.type = page->type();
      page->readInto(endpoint);
    }
    return endpoint;
  }

  void resetEditorFields() {
    endpointNameCtrl_->Clear();
    if (endpointTypeText_ != nullptr) endpointTypeText_->SetLabel("");
    if (auto* page = currentEditorPage()) {
      tracker_pi::EndpointConfig def;
      def.type = page->type();
      tracker_pi::getEndpointTypeBehavior(def).applyDefaults(def);
      page->populate(def);
      page->setErrorState({});
    }
  }

  // Push the endpoint's error status into whichever page is currently
  // visible in the book. Each page renders its own error panel so the
  // dialog does not have to reflow around a shared one.
  void updateEndpointErrorUi(const tracker_pi::EndpointConfig* endpoint) {
    const auto state =
        tracker_pi::computeEndpointErrorUiState(endpoint, endpointStatuses_);
    if (auto* page = currentEditorPage()) {
      page->setErrorState(state);
    }
    if (detailPanel_ != nullptr) {
      detailPanel_->Layout();
    }
    updateDialogSizeForCurrentMode();
    Layout();
  }

  void loadEndpointControls(int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= config_.endpoints.size()) {
      enableEndpointEditor(false);
      currentEndpointIndex_ = wxNOT_FOUND;
      return;
    }

    currentEndpointIndex_ = index;
    enableEndpointEditor(true);

    const auto& endpoint = config_.endpoints[static_cast<std::size_t>(index)];
    populateEditorFromEndpoint(endpoint);
    updateEndpointErrorUi(&endpoint);
  }

  void saveCurrentEndpoint() {
    if (currentEndpointIndex_ == wxNOT_FOUND ||
        static_cast<std::size_t>(currentEndpointIndex_) >= config_.endpoints.size()) {
      return;
    }

    auto& endpoint =
        config_.endpoints[static_cast<std::size_t>(currentEndpointIndex_)];
    const bool wasEnabled = endpoint.enabled;
    const std::string endpointId = endpoint.id;
    endpoint = readEndpointFromEditor();
    endpoint.id = endpointId;
    endpoint.enabled = wasEnabled;
    if (tracker_pi::isNoForeignLandEndpoint(endpoint) &&
        endpoint.name.rfind("endpoint-", 0) == 0) {
      endpoint.name = tracker_pi::makeNflEndpointName(
          static_cast<std::size_t>(currentEndpointIndex_));
    }
    tracker_pi::normalizeEndpointConfig(endpoint);
  }

  void enableEndpointEditor(bool enabled) {
    endpointNameCtrl_->Enable(enabled);
    // endpointTypeText_ is static text — nothing to enable/disable.
    for (auto* page : endpointPages_) {
      if (page != nullptr) page->setEnabled(enabled);
    }
    if (!enabled) {
      resetEditorFields();
      updateEndpointErrorUi(nullptr);
    }
  }

  void showListMode() {
    listMode_ = true;
    if (listPanel_ != nullptr) {
      listPanel_->Show();
    }
    if (infoPanel_ != nullptr) {
      infoPanel_->Hide();
    }
    if (detailPanel_ != nullptr) {
      detailPanel_->Hide();
    }
    if (listButtonRow_ != nullptr) {
      listButtonRow_->ShowItems(true);
    }
    if (detailCancelButton_ != nullptr) {
      detailCancelButton_->Hide();
    }
    if (detailOkButton_ != nullptr) {
      detailOkButton_->Hide();
    }
    if (detailHelpButton_ != nullptr) {
      detailHelpButton_->Hide();
    }
    updateDialogSizeForCurrentMode();
    Layout();
  }

  void showDetailMode() {
    listMode_ = false;
    if (listPanel_ != nullptr) {
      listPanel_->Hide();
    }
    if (infoPanel_ != nullptr) {
      infoPanel_->Hide();
    }
    if (detailPanel_ != nullptr) {
      detailPanel_->Show();
    }
    if (listButtonRow_ != nullptr) {
      listButtonRow_->ShowItems(false);
    }
    if (detailCancelButton_ != nullptr) {
      detailCancelButton_->Show();
    }
    if (detailOkButton_ != nullptr) {
      detailOkButton_->Show();
    }
    if (detailHelpButton_ != nullptr) {
      detailHelpButton_->Show();
    }
    updateDialogSizeForCurrentMode();
    Layout();
  }

  void showInfoMode() {
    if (listPanel_ != nullptr) {
      listPanel_->Hide();
    }
    if (detailPanel_ != nullptr) {
      detailPanel_->Hide();
    }
    if (infoPanel_ != nullptr) {
      infoPanel_->Show();
      infoPanel_->Layout();
    }
    if (detailCancelButton_ != nullptr) {
      detailCancelButton_->Hide();
    }
    if (detailOkButton_ != nullptr) {
      detailOkButton_->Hide();
    }
    if (detailHelpButton_ != nullptr) {
      detailHelpButton_->Hide();
    }
    updateDialogSizeForCurrentMode();
    Layout();
  }

  void onEndpointSelected(wxGridEvent& event) {
    if (suppressSelectionEvents_) {
      event.Skip();
      return;
    }

    const int previousSelection = currentEndpointIndex_;
    const int selection = event.GetRow();
    if (selection == currentEndpointIndex_) {
      event.Skip();
      return;
    }

    saveCurrentEndpoint();
    if (previousSelection != wxNOT_FOUND) {
      refreshEndpointRow(previousSelection);
    }
    loadEndpointControls(selection);
    event.Skip();
  }

  void onAddEndpoint(wxCommandEvent&) {
    saveCurrentEndpoint();
    // Ask for the type BEFORE touching config_. On cancel, do nothing —
    // no placeholder rows, no list churn, no dialog transition.
    const auto chosenType = tracker_plugin_ui::PickEndpointType(this);
    if (!chosenType.has_value()) {
      return;
    }
    editingNewEndpoint_ = true;
    config_.endpoints.push_back(
        makeDefaultEndpoint(config_.endpoints.size(), *chosenType));
    refreshEndpointList();
    const int selection = static_cast<int>(config_.endpoints.size() - 1);
    selectEndpointRow(selection);
    loadEndpointControls(selection);
    originalEndpoint_ = tracker_pi::EndpointConfig{};
    // Clear the Name field so the user is prompted to enter a tracker
    // name; per-page field clearing happens inside each page's populate()
    // when loadEndpointControls runs.
    endpointNameCtrl_->Clear();
    showDetailMode();
    endpointNameCtrl_->SetFocus();
  }

  bool hasGridRow(int row) const {
    return row != wxNOT_FOUND && row >= 0 &&
           static_cast<std::size_t>(row) < config_.endpoints.size();
  }

  void selectAndLoadEndpointRow(int row) {
    saveCurrentEndpoint();
    selectEndpointRow(row);
    loadEndpointControls(row);
  }

  void toggleEndpointEnabled(int row) {
    saveCurrentEndpoint();
    auto& endpoint = config_.endpoints[static_cast<std::size_t>(row)];
    endpoint.enabled = !endpoint.enabled;
    if (!persistCurrentConfig()) {
      endpoint.enabled = !endpoint.enabled;
    }
    refreshEndpointRow(row);
    loadEndpointControls(row);
  }

  void openEndpointEditor(int row) {
    saveCurrentEndpoint();
    selectEndpointRow(row);
    editingNewEndpoint_ = false;
    originalEndpoint_ = config_.endpoints[static_cast<std::size_t>(row)];
    loadEndpointControls(row);
    showDetailMode();
    endpointNameCtrl_->SetFocus();
  }

  void onEndpointGridClick(wxGridEvent& event) {
    const int row = event.GetRow();
    const int col = event.GetCol();
    if (!hasGridRow(row)) {
      event.Skip();
      return;
    }

    if (col == 0) {
      toggleEndpointEnabled(row);
      return;
    }

    if (col == 5) {
      openEndpointEditor(row);
      return;
    }

    if (col == 6) {
      removeEndpoint(row);
      return;
    }

    selectAndLoadEndpointRow(row);
    event.Skip();
  }

  void onCloseDialog(wxCommandEvent&) { Close(); }

  void onShowInfo(wxCommandEvent&) { showInfoMode(); }

  void onCloseInfo(wxCommandEvent&) {
    if (entryMode_ == TrackerDialogEntryMode::Preferences) {
      Close();
      return;
    }
    showListMode();
  }

  void onWindowClose(wxCloseEvent&) {
    if (onClose_) {
      onClose_();
    }
    Destroy();
  }

  void onDetailCancel(wxCommandEvent&) {
    if (currentEndpointIndex_ != wxNOT_FOUND &&
        static_cast<std::size_t>(currentEndpointIndex_) < config_.endpoints.size()) {
      if (editingNewEndpoint_) {
        config_.endpoints.erase(
            config_.endpoints.begin() + currentEndpointIndex_);
        currentEndpointIndex_ = wxNOT_FOUND;
      } else {
        config_.endpoints[static_cast<std::size_t>(currentEndpointIndex_)] =
            originalEndpoint_;
      }
    }
    refreshEndpointList();
    if (!config_.endpoints.empty()) {
      const int selection =
          std::min<int>(std::max(currentEndpointIndex_, 0),
                        static_cast<int>(config_.endpoints.size() - 1));
      selectEndpointRow(selection);
      loadEndpointControls(selection);
    } else {
      loadEndpointControls(wxNOT_FOUND);
    }
    editingNewEndpoint_ = false;
    showListMode();
  }

  void onDetailOk(wxCommandEvent&) {
    saveCurrentEndpoint();
    if (!applyChanges(false)) {
      return;
    }
    editingNewEndpoint_ = false;
  }

  void removeEndpoint(int selection) {
    if (selection == wxNOT_FOUND ||
        static_cast<std::size_t>(selection) >= config_.endpoints.size()) {
      return;
    }

    saveCurrentEndpoint();
    config_.endpoints.erase(config_.endpoints.begin() + selection);
    currentEndpointIndex_ = wxNOT_FOUND;
    if (!persistCurrentConfig()) {
      return;
    }
    refreshEndpointList();

    if (config_.endpoints.empty()) {
      loadEndpointControls(wxNOT_FOUND);
      return;
    }

    const int nextSelection =
        std::min(selection, static_cast<int>(config_.endpoints.size() - 1));
    selectEndpointRow(nextSelection);
    loadEndpointControls(nextSelection);
  }

  const std::filesystem::path configPath_;
  tracker_pi::RuntimeConfig config_;
  int currentEndpointIndex_ = wxNOT_FOUND;
  bool suppressSelectionEvents_ = false;
  wxPanel* infoPanel_ = nullptr;
  wxHyperlinkCtrl* infoConfigPathLink_ = nullptr;
  wxHyperlinkCtrl* infoManualLink_ = nullptr;
  wxHyperlinkCtrl* infoIssuesLink_ = nullptr;
  wxPanel* listPanel_ = nullptr;
  wxPanel* detailPanel_ = nullptr;
  wxGrid* endpointGrid_ = nullptr;
  std::unique_ptr<ActionIcons> actionIcons_;
  std::map<std::string, tracker_pi::EndpointUiState> endpointStatuses_;
  TrackerDialogEntryMode entryMode_ = TrackerDialogEntryMode::Trackers;
  std::function<bool(const tracker_pi::RuntimeConfig&, std::string*)> applyConfigFn_;
  std::function<void()> onClose_;
  bool listMode_ = true;
  bool editingNewEndpoint_ = false;
  tracker_pi::EndpointConfig originalEndpoint_;
  wxBoxSizer* buttonRow_ = nullptr;
  wxBoxSizer* infoButtonRow_ = nullptr;
  wxBoxSizer* listButtonRow_ = nullptr;
  wxButton* addButton_ = nullptr;
  wxButton* closeButton_ = nullptr;
  wxButton* infoCloseButton_ = nullptr;
  wxButton* detailHelpButton_ = nullptr;
  wxButton* detailCancelButton_ = nullptr;
  wxButton* detailOkButton_ = nullptr;

  // Shared hero sitting above Name/Type. Raw wxStaticBitmap, mirroring
  // the info/overview screen's layout exactly. Since type is immutable
  // after creation, the bitmap only changes when the user opens a
  // different existing tracker (handled in populateEditorFromEndpoint).
  wxStaticBitmap* detailHero_ = nullptr;

  // Per-type editor pages inside a simplebook. Index order == registration
  // order in buildEndpointEditorBook(); page selection tracks the Type
  // combo.
  wxSimplebook* endpointEditorBook_ = nullptr;
  std::vector<tracker_plugin_ui::EndpointEditorPage*> endpointPages_;

  // Shared identity widgets that live outside the book (every type has a
  // name and a type). The matching labels are created by addDetailLabel,
  // which stores the label through a reference — we keep the pointers so
  // that signature continues to work without special-casing. `endpointTypeText_`
  // is a read-only wxStaticText displaying the current tracker's type;
  // changing the type post-creation is not supported (pick at Add time).
  wxStaticText* endpointNameLabel_ = nullptr;
  wxStaticText* endpointTypeLabel_ = nullptr;
  wxTextCtrl* endpointNameCtrl_ = nullptr;
  wxStaticText* endpointTypeText_ = nullptr;
};

TrackerDialog* CreateTrackerDialog(
    wxWindow* parent, const std::filesystem::path& configPath,
    const tracker_pi::RuntimeConfig& initialConfig,
    const std::map<std::string, tracker_pi::EndpointUiState>& endpointStatuses,
    std::function<bool(const tracker_pi::RuntimeConfig&, std::string*)> applyConfigFn,
    TrackerDialogEntryMode entryMode, std::function<void()> onClose) {
  return new TrackerDialog(parent, configPath, initialConfig, endpointStatuses,
                           std::move(applyConfigFn), entryMode,
                           std::move(onClose));
}

void ShowTrackerDialog(TrackerDialog* dialog) {
  if (dialog != nullptr) {
    dialog->Show();
    dialog->Raise();
    dialog->SetFocus();
    dialog->CallAfter([dialog]() {
      if (dialog->IsShown()) {
        dialog->Raise();
        dialog->SetFocus();
      }
    });
  }
}

void RaiseTrackerDialog(TrackerDialog* dialog) {
  if (dialog != nullptr) {
    dialog->Raise();
  }
}

void FocusTrackerDialog(TrackerDialog* dialog) {
  if (dialog != nullptr) {
    dialog->SetFocus();
  }
}

void DestroyTrackerDialog(TrackerDialog* dialog) {
  if (dialog != nullptr) {
    dialog->Destroy();
  }
}
