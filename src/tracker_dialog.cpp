#include "tracker_dialog.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include <wx/bmpbndl.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/dialog.h>
#include <wx/filename.h>
#include <wx/grid.h>
#include <wx/hyperlink.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statbmp.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/window.h>

#include "1tracker_pi/endpoint_policy.h"
#include "1tracker_pi/endpoint_type_behavior.h"
#include "1tracker_pi/nfl_settings.h"
#include "ocpn_plugin.h"
#include "plugin_ui_utils.h"

namespace {

constexpr int kPreferencesDialogWidth = 576;
constexpr int kPreferencesDialogHeight = 416;
constexpr int kCompactSpinWidth = 58;
constexpr int kDetailFieldWidth = 260;
constexpr int kHeaderNameFieldWidth = 90;
constexpr int kHeaderValueLabelWidth = 44;
constexpr int kDetailLabelWidth = 110;
constexpr int kDetailHeaderFieldIndent = 110;
constexpr int kHeaderIconHeight = 58;
constexpr int kHeaderHeroWidth = (kPreferencesDialogWidth * 3) / 4;
constexpr int kJsonDetailHeaderWidth = 281;
constexpr int kJsonDetailHeaderHeight = 75;
constexpr int kNflDetailHeaderWidth = 281;

bool containsText(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}

std::string summarizeEndpointError(const tracker_pi::EndpointConfig& endpoint,
                                   const std::string& rawError) {
  if (rawError.empty()) {
    return "";
  }

  if (containsText(rawError, "an active API key was not provided")) {
    return tracker_pi::isNoForeignLandType(endpoint.type)
               ? "Your NFL boat key is missing or invalid."
               : "The API key was not accepted by the server.";
  }

  if (containsText(rawError, "Invalid or expired token")) {
    return "The authorization token is invalid or expired.";
  }

  if (containsText(rawError, "Could not resolve host")) {
    return "The server name could not be resolved.";
  }

  if (containsText(rawError, "Failed to connect") ||
      containsText(rawError, "Connection refused")) {
    return "Could not connect to the server.";
  }

  if (containsText(rawError, "timed out")) {
    return "The server took too long to respond.";
  }

  if (containsText(rawError, "HTTP 401")) {
    return "The server rejected the credentials.";
  }

  if (containsText(rawError, "HTTP 403")) {
    return "The server refused this request.";
  }

  if (containsText(rawError, "HTTP 404")) {
    return "The tracking URL could not be found.";
  }

  if (containsText(rawError, "HTTP 200")) {
    return "The server responded, but rejected the tracking data.";
  }

  if (containsText(rawError, "HTTP ")) {
    return "The server returned an error while processing the tracking request.";
  }

  return "Tracking could not be sent. Check the technical details below.";
}

tracker_pi::EndpointConfig makeDefaultEndpoint(std::size_t index) {
  tracker_pi::EndpointConfig endpoint;
  endpoint.id = tracker_pi::makeEndpointId();
  endpoint.name = "endpoint-" + std::to_string(index + 1);
  endpoint.type = tracker_pi::kEndpointTypeHttpJsonWithHeaderKey;
  endpoint.enabled = true;
  endpoint.includeAwaAws = true;
  endpoint.timeoutSeconds = 10;
  endpoint.headerName.clear();
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
    return wxBitmapBundle::FromSVGFile(path.GetFullPath(), size).GetBitmap(size);
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
                     ? "1tracker Preferences"
                     : "Tracking",
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
      wxMessageBox(wxString::FromUTF8(errorMessage), "1tracker",
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
      wxMessageBox(wxString::FromUTF8(errorMessage), "1tracker",
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
    endpointGrid_->SetColLabelValue(1, "Name");
    endpointGrid_->SetColLabelValue(2, "Send interval");
    endpointGrid_->SetColLabelValue(3, "Last successful send");
    endpointGrid_->SetColLabelValue(4, "Status");
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
    addButton_ = new wxButton(listPanel_, wxID_ADD, "Add tracker");
    closeButton_ = new wxButton(listPanel_, wxID_CANCEL, "Close");
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
                      const char* text) {
    label = new wxStaticText(detailPanel_, wxID_ANY, text);
    setFixedLabelWidth(label);
    form->Add(label, 0, wxALIGN_CENTER_VERTICAL);
  }

  void buildDetailHeader(wxBoxSizer* detailColumn) {
    detailHeaderPanel_ = new wxPanel(detailPanel_, wxID_ANY);
    auto* detailHeaderRow = new wxBoxSizer(wxHORIZONTAL);
    detailHeaderIcon_ = new wxStaticBitmap(
        detailHeaderPanel_, wxID_ANY,
        tracker_plugin_ui::LoadBitmapFromPluginAssetWidth(
            "1tracker_json_header.png", kJsonDetailHeaderWidth));
    detailHeaderIcon_->SetMinSize(
        wxSize(kJsonDetailHeaderWidth, kJsonDetailHeaderHeight));
    detailHeaderRow->AddSpacer(kDetailHeaderFieldIndent);
    detailHeaderRow->Add(detailHeaderIcon_, 0, wxALIGN_CENTER_VERTICAL);
    detailHeaderPanel_->SetSizer(detailHeaderRow);
    detailHeaderPanel_->SetMinSize(
        wxSize(kDetailHeaderFieldIndent + kJsonDetailHeaderWidth,
               kJsonDetailHeaderHeight));
    detailColumn->Add(detailHeaderPanel_, 0, wxBOTTOM, 8);
  }

  void buildIdentityFields(wxFlexGridSizer* form) {
    addDetailLabel(form, endpointNameLabel_, "Name");
    endpointNameCtrl_ = createSizedTextCtrl(detailPanel_);
    form->Add(endpointNameCtrl_, 1, wxEXPAND);

    addDetailLabel(form, endpointTypeLabel_, "Type");
    endpointTypeChoice_ =
        new wxComboBox(detailPanel_, wxID_ANY, "", wxDefaultPosition,
                       wxDefaultSize, 0, nullptr, wxCB_READONLY);
    endpointTypeChoice_->SetMinSize(wxSize(kDetailFieldWidth, -1));
    endpointTypeChoice_->SetMaxSize(wxSize(kDetailFieldWidth, -1));
    for (const auto& type : tracker_pi::listEndpointTypes()) {
      endpointTypeChoice_->Append(wxString::FromUTF8(type.c_str()));
    }
    endpointTypeChoice_->SetSelection(0);
    form->Add(endpointTypeChoice_, 1, wxEXPAND);
  }

  void buildIntervalField(wxFlexGridSizer* form) {
    addDetailLabel(form, endpointIntervalLabel_, "Send interval");
    endpointIntervalRow_ = new wxBoxSizer(wxHORIZONTAL);
    endpointIntervalSpin_ = new wxSpinCtrl(detailPanel_, wxID_ANY);
    endpointIntervalSpin_->SetRange(1, 10080);
    endpointIntervalSpin_->SetMinSize(wxSize(kCompactSpinWidth, -1));
    endpointIntervalSpin_->SetMaxSize(wxSize(kCompactSpinWidth, -1));
    endpointIntervalRow_->Add(endpointIntervalSpin_, 0, wxALIGN_CENTER_VERTICAL);
    endpointIntervalUnitsLabel_ =
        new wxStaticText(detailPanel_, wxID_ANY,
                         "minutes, if moved at least");
    endpointIntervalRow_->Add(endpointIntervalUnitsLabel_, 0,
                              wxLEFT | wxALIGN_CENTER_VERTICAL, 8);
    endpointIntervalRow_->AddSpacer(8);
    endpointMinDistanceLabel_ =
        new wxStaticText(detailPanel_, wxID_ANY, "");
    endpointIntervalRow_->Add(endpointMinDistanceLabel_, 0,
                              wxALIGN_CENTER_VERTICAL);
    endpointIntervalRow_->AddSpacer(8);
    endpointMinDistanceSpin_ = new wxSpinCtrl(detailPanel_, wxID_ANY);
    endpointMinDistanceSpin_->SetRange(0, 100000);
    endpointMinDistanceSpin_->SetMinSize(wxSize(kCompactSpinWidth, -1));
    endpointMinDistanceSpin_->SetMaxSize(wxSize(kCompactSpinWidth, -1));
    endpointIntervalRow_->Add(endpointMinDistanceSpin_, 0,
                              wxALIGN_CENTER_VERTICAL);
    endpointMinDistanceUnitsLabel_ =
        new wxStaticText(detailPanel_, wxID_ANY, "meters");
    endpointIntervalRow_->Add(endpointMinDistanceUnitsLabel_, 0,
                              wxLEFT | wxALIGN_CENTER_VERTICAL, 8);
    form->Add(endpointIntervalRow_, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
  }

  void buildTransportFields(wxFlexGridSizer* form) {
    addDetailLabel(form, endpointUrlLabel_, "URL");
    endpointUrlCtrl_ = createSizedTextCtrl(detailPanel_);
    form->Add(endpointUrlCtrl_, 1, wxEXPAND);

    addDetailLabel(form, endpointTimeoutLabel_, "Timeout");
    endpointTimeoutRow_ = new wxBoxSizer(wxHORIZONTAL);
    endpointTimeoutSpin_ = new wxSpinCtrl(detailPanel_, wxID_ANY);
    endpointTimeoutSpin_->SetRange(1, 600);
    endpointTimeoutSpin_->SetMinSize(wxSize(kCompactSpinWidth, -1));
    endpointTimeoutSpin_->SetMaxSize(wxSize(kCompactSpinWidth, -1));
    endpointTimeoutRow_->Add(endpointTimeoutSpin_, 0, wxALIGN_CENTER_VERTICAL);
    endpointTimeoutUnitsLabel_ =
        new wxStaticText(detailPanel_, wxID_ANY, "seconds");
    endpointTimeoutRow_->Add(endpointTimeoutUnitsLabel_, 0,
                             wxLEFT | wxALIGN_CENTER_VERTICAL, 8);
    form->Add(endpointTimeoutRow_, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
  }

  void buildHeaderFields(wxFlexGridSizer* form) {
    auto* headerLabelPanel = new wxPanel(detailPanel_, wxID_ANY);
    auto* headerLabelSizer = new wxBoxSizer(wxVERTICAL);
    endpointHeaderNameLabel_ =
        new wxStaticText(headerLabelPanel, wxID_ANY, "Header name");
    setFixedLabelWidth(endpointHeaderNameLabel_);
    headerLabelSizer->Add(endpointHeaderNameLabel_, 0, wxALIGN_CENTER_VERTICAL);
    endpointNflBoatKeyLabel_ =
        new wxStaticText(headerLabelPanel, wxID_ANY, "My NFL boat key");
    setFixedLabelWidth(endpointNflBoatKeyLabel_);
    endpointNflBoatKeyLabel_->Hide();
    headerLabelSizer->Add(endpointNflBoatKeyLabel_, 0, wxALIGN_CENTER_VERTICAL);
    headerLabelPanel->SetSizer(headerLabelSizer);
    form->Add(headerLabelPanel, 0, wxALIGN_CENTER_VERTICAL);

    auto* headerRow = new wxBoxSizer(wxHORIZONTAL);

    endpointHeaderNameCtrl_ = new wxTextCtrl(detailPanel_, wxID_ANY);
    endpointHeaderNameCtrl_->SetMinSize(wxSize(kHeaderNameFieldWidth, -1));
    endpointHeaderNameCtrl_->SetMaxSize(wxSize(kHeaderNameFieldWidth, -1));
    headerRow->Add(endpointHeaderNameCtrl_, 0, wxALIGN_CENTER_VERTICAL);

    endpointHeaderValueLabel_ =
        new wxStaticText(detailPanel_, wxID_ANY, "Value");
    endpointHeaderValueLabel_->SetMinSize(wxSize(kHeaderValueLabelWidth, -1));
    headerRow->Add(endpointHeaderValueLabel_, 0,
                   wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 8);

    endpointHeaderValueCtrl_ = new wxTextCtrl(detailPanel_, wxID_ANY);
    headerRow->Add(endpointHeaderValueCtrl_, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);

    form->Add(headerRow, 1, wxEXPAND);
  }

  void buildHeaderHelp(wxFlexGridSizer* form) {
    form->AddSpacer(0);
    endpointHeaderValueHelpPanel_ = new wxPanel(detailPanel_, wxID_ANY);
    auto* endpointHeaderHelpSizer = new wxBoxSizer(wxHORIZONTAL);
    endpointHeaderValueHelpText_ = new wxStaticText(
        endpointHeaderValueHelpPanel_, wxID_ANY, "To get your boat key, open");
    endpointHeaderValueHelpText_->SetForegroundColour(wxColour(72, 72, 72));
    endpointHeaderHelpSizer->Add(endpointHeaderValueHelpText_, 0,
                                 wxRIGHT | wxALIGN_CENTER_VERTICAL, 4);
    endpointHeaderValueLink_ = new wxHyperlinkCtrl(
        endpointHeaderValueHelpPanel_, wxID_ANY, "Boat Tracking Settings",
        tracker_pi::nfl::boatTrackingSettingsUrl());
    endpointHeaderValueLink_->SetNormalColour(wxColour(20, 78, 140));
    endpointHeaderValueLink_->SetHoverColour(wxColour(14, 58, 106));
    endpointHeaderValueLink_->SetVisitedColour(wxColour(84, 62, 132));
    endpointHeaderHelpSizer->Add(endpointHeaderValueLink_, 0,
                                 wxRIGHT | wxALIGN_CENTER_VERTICAL, 4);
    endpointHeaderValueHelpTailText_ = new wxStaticText(
        endpointHeaderValueHelpPanel_, wxID_ANY, "in NFL.");
    endpointHeaderValueHelpTailText_->SetForegroundColour(wxColour(72, 72, 72));
    endpointHeaderHelpSizer->Add(endpointHeaderValueHelpTailText_, 0,
                                 wxALIGN_CENTER_VERTICAL);
    endpointHeaderValueHelpPanel_->SetSizer(endpointHeaderHelpSizer);
    form->Add(endpointHeaderValueHelpPanel_, 0, wxTOP | wxEXPAND, 2);
  }

  void buildWindField(wxFlexGridSizer* form) {
    addDetailLabel(form, includeAwaAwsLabel_, "Include wind");
    includeAwaAwsCheck_ =
        new wxCheckBox(detailPanel_, wxID_ANY, "", wxDefaultPosition,
                       wxDefaultSize, wxCHK_2STATE | wxWANTS_CHARS);
    form->Add(includeAwaAwsCheck_, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
  }

  void buildDetailErrorText(wxBoxSizer* detailColumn) {
    endpointErrorPanel_ = new wxPanel(detailPanel_, wxID_ANY);
    auto* errorSizer = new wxBoxSizer(wxVERTICAL);

    endpointErrorSummaryText_ = new wxStaticText(endpointErrorPanel_, wxID_ANY, "");
    endpointErrorSummaryText_->SetForegroundColour(wxColour(160, 32, 32));
    endpointErrorSummaryText_->Wrap(kDetailFieldWidth);
    errorSizer->Add(endpointErrorSummaryText_, 0, wxBOTTOM, 4);

    endpointErrorMetaText_ = new wxStaticText(endpointErrorPanel_, wxID_ANY, "");
    endpointErrorMetaText_->SetForegroundColour(wxColour(96, 96, 96));
    errorSizer->Add(endpointErrorMetaText_, 0, wxBOTTOM, 4);

    endpointErrorDetailsToggle_ =
        new wxButton(endpointErrorPanel_, wxID_ANY, "Show technical details");
    errorSizer->Add(endpointErrorDetailsToggle_, 0, wxBOTTOM, 4);

    endpointErrorText_ = new wxStaticText(endpointErrorPanel_, wxID_ANY, "");
    endpointErrorText_->SetForegroundColour(wxColour(128, 44, 44));
    endpointErrorText_->Wrap(kDetailFieldWidth);
    errorSizer->Add(endpointErrorText_, 0);

    endpointErrorPanel_->SetSizer(errorSizer);
    detailColumn->Add(endpointErrorPanel_, 0,
                      wxTOP | wxLEFT | wxRIGHT | wxEXPAND, 4);
    endpointErrorPanel_->Hide();
    endpointErrorMetaText_->Hide();
    endpointErrorDetailsToggle_->Hide();
    endpointErrorText_->Hide();
  }

  void buildDetailPanel(wxBoxSizer* contentSizer) {
    detailPanel_ = new wxPanel(this, wxID_ANY, wxDefaultPosition,
                               wxDefaultSize, wxTAB_TRAVERSAL);
    auto* detailColumn = new wxBoxSizer(wxVERTICAL);
    buildDetailHeader(detailColumn);
    auto* form = createDetailForm();
    buildIdentityFields(form);
    buildIntervalField(form);
    buildWindField(form);
    buildTransportFields(form);
    buildHeaderFields(form);
    buildHeaderHelp(form);

    detailColumn->Add(form, 1, wxEXPAND | wxALL, 4);
    buildDetailErrorText(detailColumn);
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
        "The 1tracker plugin automatically sends your location to a website "
        "via API calls. You can configure a custom website, or a standard "
        "noforeignland endpoint.\n\nSee the online manual for specific "
        "configuration options. For functional changes or issues, please "
        "contact the author.");
    infoBody->Wrap(kPreferencesDialogWidth - 80);
    infoSizer->Add(infoBody, 0, wxBOTTOM, 12);
  }

  void buildInfoConfigPath(wxBoxSizer* infoSizer) {
    auto* infoConfigText = new wxStaticText(
        infoPanel_, wxID_ANY,
        "Settings are stored in a configuration file here on your local machine: ");
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
    infoCloseButton_ = new wxButton(infoPanel_, wxID_ANY, "Close");
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
    detailHelpButton_ = new wxButton(this, wxID_ANY, "Help");
    detailCancelButton_ = new wxButton(this, wxID_ANY, "Cancel");
    detailOkButton_ = new wxButton(this, wxID_ANY, "OK");
    buttonRow_->Add(detailHelpButton_, 0);
    buttonRow_->AddStretchSpacer(1);
    buttonRow_->Add(detailCancelButton_, 0, wxLEFT, 8);
    buttonRow_->Add(detailOkButton_, 0, wxLEFT, 8);
    root->Add(buttonRow_, 0, wxALL | wxEXPAND, 12);
  }

  void applyGridColumnSizing() {
    endpointGrid_->SetColSize(0, 31);
    endpointGrid_->SetColSize(1, 155);
    endpointGrid_->SetColSize(2, 90);
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
    endpointGrid_->SetSelectionBackground(*wxWHITE);
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
    wxColour labelFg;
    if (GetGlobalColor("DILG3", &labelFg)) {
      endpointGrid_->SetSelectionForeground(labelFg);
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
    endpointTypeChoice_->MoveAfterInTabOrder(endpointNameCtrl_);
    endpointIntervalSpin_->MoveAfterInTabOrder(endpointTypeChoice_);
    endpointMinDistanceSpin_->MoveAfterInTabOrder(endpointIntervalSpin_);
    endpointUrlCtrl_->MoveAfterInTabOrder(endpointMinDistanceSpin_);
    endpointTimeoutSpin_->MoveAfterInTabOrder(endpointUrlCtrl_);
    endpointHeaderNameCtrl_->MoveAfterInTabOrder(endpointTimeoutSpin_);
    endpointHeaderValueCtrl_->MoveAfterInTabOrder(endpointHeaderNameCtrl_);
    includeAwaAwsCheck_->MoveAfterInTabOrder(endpointHeaderValueCtrl_);
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

  void bindTabNavigation(wxWindow* source, wxWindow* forwardTarget,
                         wxWindow* backwardTarget = nullptr,
                         bool forwardFromKeyboard = false,
                         bool backwardFromKeyboard = false) {
    if (source == nullptr) {
      return;
    }

    source->Bind(wxEVT_KEY_DOWN, [this, forwardTarget, backwardTarget,
                                  forwardFromKeyboard,
                                  backwardFromKeyboard](wxKeyEvent& event) {
      if (event.GetKeyCode() != WXK_TAB) {
        event.Skip();
        return;
      }

      if (event.ShiftDown()) {
        if (backwardTarget != nullptr) {
          if (!backwardTarget->IsEnabled() || !backwardTarget->IsShown()) {
            event.Skip();
            return;
          }
          focusTabTarget(backwardTarget, backwardFromKeyboard);
          return;
        }
      } else if (forwardTarget != nullptr) {
        if (!forwardTarget->IsEnabled() || !forwardTarget->IsShown()) {
          event.Skip();
          return;
        }
        focusTabTarget(forwardTarget, forwardFromKeyboard);
        return;
      }

      event.Skip();
    });
  }

  void bindDetailTabNavigation() {
    bindTabNavigation(endpointNameCtrl_, endpointTypeChoice_);
    bindTabNavigation(endpointTypeChoice_, endpointIntervalSpin_,
                      endpointNameCtrl_);
    bindTabNavigation(endpointIntervalSpin_, endpointMinDistanceSpin_,
                      endpointTypeChoice_);
    bindTabNavigation(endpointHeaderValueCtrl_, includeAwaAwsCheck_, nullptr,
                      true);
    bindTabNavigation(includeAwaAwsCheck_, nullptr, endpointHeaderValueCtrl_,
                      false, true);
  }

  void bindDetailFieldEvents() {
    configureDetailTabOrder();
    bindDetailTabNavigation();
    endpointTypeChoice_->Bind(wxEVT_COMBOBOX,
                              &TrackerDialog::onEndpointTypeChanged, this);
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
    endpointErrorDetailsToggle_->Bind(wxEVT_BUTTON,
                                      &TrackerDialog::onToggleErrorDetails, this);
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
    return wxString::Format("%d %s", endpoint.sendIntervalMinutes,
                            endpoint.sendIntervalMinutes == 1 ? "minute"
                                                              : "minutes");
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
        return "Active";
      case tracker_pi::EndpointUiStatus::Failure:
        return "Issue";
      case tracker_pi::EndpointUiStatus::Disabled:
      case tracker_pi::EndpointUiStatus::NoStats:
      default:
        return "Inactive";
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

  std::string selectedEndpointType() const {
    return endpointTypeChoice_ != nullptr
               ? endpointTypeChoice_->GetStringSelection().ToStdString()
               : tracker_pi::kEndpointTypeHttpJsonWithHeaderKey;
  }

  void setWindowShown(wxWindow* window, bool shown) {
    if (window != nullptr) {
      window->Show(shown);
    }
  }

  void setGenericTransportFieldsShown(bool shown) {
    setWindowShown(endpointUrlLabel_, shown);
    setWindowShown(endpointUrlCtrl_, shown);
    setWindowShown(endpointTimeoutLabel_, shown);
    setWindowShown(endpointTimeoutSpin_, shown);
    setWindowShown(endpointTimeoutUnitsLabel_, shown);
    setWindowShown(endpointHeaderNameLabel_, shown);
    setWindowShown(endpointHeaderNameCtrl_, shown);
    setWindowShown(includeAwaAwsLabel_, shown);
    setWindowShown(includeAwaAwsCheck_, shown);
  }

  void setMinDistanceFieldsShown(bool shown) {
    setWindowShown(endpointMinDistanceLabel_, shown);
    setWindowShown(endpointMinDistanceSpin_, shown);
    setWindowShown(endpointMinDistanceUnitsLabel_, shown);
  }

  void populateEditorFromEndpoint(const tracker_pi::EndpointConfig& endpoint) {
    includeAwaAwsCheck_->SetValue(endpoint.includeAwaAws);
    endpointNameCtrl_->SetValue(wxString::FromUTF8(endpoint.name.c_str()));
    if (!endpointTypeChoice_->SetStringSelection(
            wxString::FromUTF8(endpoint.type.c_str()))) {
      endpointTypeChoice_->SetSelection(0);
    }
    endpointIntervalSpin_->SetValue(endpoint.sendIntervalMinutes);
    endpointMinDistanceSpin_->SetValue(endpoint.minDistanceMeters);
    endpointUrlCtrl_->SetValue(wxString::FromUTF8(endpoint.url.c_str()));
    endpointTimeoutSpin_->SetValue(endpoint.timeoutSeconds);
    endpointHeaderNameCtrl_->SetValue(
        wxString::FromUTF8(endpoint.headerName.c_str()));
    endpointHeaderValueCtrl_->SetValue(
        wxString::FromUTF8(endpoint.headerValue.c_str()));
  }

  tracker_pi::EndpointConfig readEndpointFromEditor() const {
    tracker_pi::EndpointConfig endpoint;
    endpoint.includeAwaAws = includeAwaAwsCheck_->GetValue();
    endpoint.name = endpointNameCtrl_->GetValue().ToStdString();
    endpoint.type = endpointTypeChoice_->GetStringSelection().ToStdString();
    endpoint.sendIntervalMinutes = endpointIntervalSpin_->GetValue();
    endpoint.minDistanceMeters = endpointMinDistanceSpin_->GetValue();
    endpoint.url = endpointUrlCtrl_->GetValue().ToStdString();
    endpoint.timeoutSeconds = endpointTimeoutSpin_->GetValue();
    endpoint.headerName = endpointHeaderNameCtrl_->GetValue().ToStdString();
    endpoint.headerValue = endpointHeaderValueCtrl_->GetValue().ToStdString();
    return endpoint;
  }

  void resetEditorFields() {
    includeAwaAwsCheck_->SetValue(true);
    endpointNameCtrl_->Clear();
    endpointTypeChoice_->SetSelection(0);
    endpointIntervalSpin_->SetRange(1, 10080);
    endpointIntervalSpin_->SetValue(tracker_pi::kDefaultSendIntervalMinutes);
    endpointMinDistanceSpin_->SetRange(0, 100000);
    endpointMinDistanceSpin_->SetValue(tracker_pi::kDefaultMinDistanceMeters);
    endpointUrlCtrl_->Clear();
    endpointTimeoutSpin_->SetValue(10);
    endpointHeaderNameCtrl_->Clear();
    endpointHeaderValueCtrl_->Clear();
  }

  void applyHeaderUiMetadata(
      const tracker_pi::EndpointUiMetadata& metadata) {
    const bool isNfl = tracker_pi::isNoForeignLandType(selectedEndpointType());
    if (endpointNflBoatKeyLabel_ != nullptr) {
      endpointNflBoatKeyLabel_->SetLabel(
          wxString::FromUTF8(metadata.headerValueLabel.c_str()));
    }
    if (endpointHeaderValueLabel_ != nullptr) {
      endpointHeaderValueLabel_->SetLabel(
          wxString::FromUTF8(metadata.headerValueLabel.c_str()));
    }
    if (endpointHeaderValueLabel_ != nullptr) {
      endpointHeaderValueLabel_->SetToolTip(
          wxString::FromUTF8(metadata.headerValueTooltip.c_str()));
    }
    if (endpointHeaderValueCtrl_ != nullptr) {
      endpointHeaderValueCtrl_->SetToolTip(
          wxString::FromUTF8(metadata.headerValueTooltip.c_str()));
    }
    if (endpointHeaderValueHelpPanel_ != nullptr) {
      endpointHeaderValueHelpPanel_->Show(!metadata.headerValueTooltip.empty());
    }
  }

  void applyTransportFieldVisibility(
      const tracker_pi::EndpointUiMetadata& metadata) {
    if (!metadata.supportsAwaAws && includeAwaAwsCheck_ != nullptr) {
      includeAwaAwsCheck_->SetValue(false);
    }
    const bool isNfl = tracker_pi::isNoForeignLandType(selectedEndpointType());
    if (endpointIntervalUnitsLabel_ != nullptr) {
      endpointIntervalUnitsLabel_->SetLabel(
          isNfl ? "minutes" : "minutes, if moved at least");
    }
    setGenericTransportFieldsShown(metadata.showsGenericTransportFields);
    setWindowShown(endpointHeaderNameLabel_,
                   !isNfl && metadata.showsGenericTransportFields);
    setWindowShown(endpointNflBoatKeyLabel_, isNfl);
    setWindowShown(endpointHeaderNameCtrl_, !isNfl);
    setWindowShown(endpointHeaderValueLabel_, !isNfl);
    setMinDistanceFieldsShown(!isNfl);
    if (endpointMinDistanceSpin_ != nullptr) {
      if (isNfl) {
        endpointMinDistanceSpin_->SetValue(tracker_pi::nfl::kMinDistanceMeters);
      }
      endpointMinDistanceSpin_->Enable(!isNfl);
    }
  }

  void applyTypeHeaderArtwork() {
    if (detailHeaderPanel_ != nullptr) {
      detailHeaderPanel_->Show();
    }
  }

  void updateEndpointTypeUi(const std::string& type) {
    const auto metadata = tracker_pi::getEndpointTypeBehavior(type).uiMetadata();
    applyHeaderUiMetadata(metadata);
    applyTransportFieldVisibility(metadata);
    applyTypeHeaderArtwork();
    if (detailPanel_ != nullptr) {
      detailPanel_->Layout();
    }
    Layout();
  }

  void updateEndpointErrorUi(const tracker_pi::EndpointConfig* endpoint) {
    if (endpointErrorPanel_ == nullptr || endpointErrorSummaryText_ == nullptr ||
        endpointErrorText_ == nullptr || endpointErrorDetailsToggle_ == nullptr) {
      return;
    }

    const auto currentEndpointError = [this](
                                          const tracker_pi::EndpointConfig* current)
        -> std::tuple<wxString, wxString, bool> {
      if (current == nullptr) {
        return {"", "", false};
      }

      const auto statusIt =
          endpointStatuses_.find(tracker_pi::endpointStateKey(*current));
      const bool hasError =
          statusIt != endpointStatuses_.end() &&
          !statusIt->second.lastErrorMessage.empty();
      const wxString lastCall =
          statusIt != endpointStatuses_.end()
              ? wxString::FromUTF8(statusIt->second.lastSentLocalTime.c_str())
              : "";
      return {hasError ? wxString::FromUTF8(statusIt->second.lastErrorMessage.c_str())
                       : "",
              lastCall, hasError};
    };

    const auto [errorText, lastCallText, hasError] = currentEndpointError(endpoint);
    endpointErrorDetailsExpanded_ = false;
    const int errorWrapWidth =
        std::max(kDetailFieldWidth,
                 detailPanel_ != nullptr ? detailPanel_->GetClientSize().GetWidth() - 8
                                         : kDetailFieldWidth);

    if (!hasError || endpoint == nullptr) {
      endpointErrorSummaryText_->SetLabel("");
      endpointErrorMetaText_->SetLabel("");
      endpointErrorText_->SetLabel("");
      endpointErrorMetaText_->Hide();
      endpointErrorText_->Hide();
      endpointErrorDetailsToggle_->Hide();
      endpointErrorPanel_->Hide();
    } else {
      endpointErrorSummaryText_->SetLabel(wxString::FromUTF8(
          summarizeEndpointError(*endpoint, errorText.ToStdString()).c_str()));
      endpointErrorSummaryText_->Wrap(errorWrapWidth);
      if (!lastCallText.empty()) {
        endpointErrorMetaText_->SetLabel("Last call: " + lastCallText);
        endpointErrorMetaText_->Show();
      } else {
        endpointErrorMetaText_->SetLabel("");
        endpointErrorMetaText_->Hide();
      }
      endpointErrorText_->SetLabel(errorText);
      endpointErrorText_->Wrap(kDetailFieldWidth);
      endpointErrorText_->Hide();
      endpointErrorDetailsToggle_->SetLabel("Show technical details");
      endpointErrorDetailsToggle_->Show();
      endpointErrorPanel_->Show();
    }

    if (detailPanel_ != nullptr) {
      detailPanel_->Layout();
    }
    updateDialogSizeForCurrentMode();
    Layout();
  }

  void onToggleErrorDetails(wxCommandEvent&) {
    if (endpointErrorPanel_ == nullptr || endpointErrorText_ == nullptr ||
        endpointErrorDetailsToggle_ == nullptr) {
      return;
    }

    endpointErrorDetailsExpanded_ = !endpointErrorDetailsExpanded_;
    endpointErrorText_->Show(endpointErrorDetailsExpanded_);
    endpointErrorDetailsToggle_->SetLabel(endpointErrorDetailsExpanded_
                                              ? "Hide technical details"
                                              : "Show technical details");

    if (detailPanel_ != nullptr) {
      detailPanel_->Layout();
    }
    updateDialogSizeForCurrentMode();
    Layout();
  }

  int adjustedIntervalForTypeChange(const tracker_pi::EndpointConfig& endpoint,
                                    int currentInterval,
                                    bool preserveBoatKey) const {
    if (!tracker_pi::isNoForeignLandType(endpoint.type)) {
      return std::max(currentInterval, tracker_pi::kDefaultSendIntervalMinutes);
    }

    return std::max(preserveBoatKey ? tracker_pi::kNflMinSendIntervalMinutes
                                    : tracker_pi::kNflDefaultSendIntervalMinutes,
                    currentInterval);
  }

  void writeEndpointDefaultsToEditor(const tracker_pi::EndpointConfig& endpoint,
                                     const std::string& preservedBoatKey,
                                     bool preserveBoatKey) {
    endpointIntervalSpin_->SetRange(
        tracker_pi::isNoForeignLandType(endpoint.type)
            ? tracker_pi::kNflMinSendIntervalMinutes
            : tracker_pi::kDefaultSendIntervalMinutes,
        10080);
    endpointIntervalSpin_->SetValue(endpoint.sendIntervalMinutes);
    endpointMinDistanceSpin_->SetValue(endpoint.minDistanceMeters);
    endpointMinDistanceSpin_->Enable(
        !tracker_pi::isNoForeignLandType(endpoint.type));
    endpointUrlCtrl_->SetValue(wxString::FromUTF8(endpoint.url.c_str()));
    endpointTimeoutSpin_->SetValue(endpoint.timeoutSeconds);
    endpointHeaderNameCtrl_->SetValue(
        wxString::FromUTF8(endpoint.headerName.c_str()));
    includeAwaAwsCheck_->SetValue(endpoint.includeAwaAws);
    if (!preserveBoatKey) {
      endpointHeaderValueCtrl_->Clear();
    } else {
      endpointHeaderValueCtrl_->SetValue(
          wxString::FromUTF8(preservedBoatKey.c_str()));
    }
  }

  void applyEndpointTypeDefaultsInEditor(bool preserveBoatKey) {
    auto endpoint = readEndpointFromEditor();
    const std::string preservedBoatKey = endpoint.headerValue;
    const int currentInterval = endpoint.sendIntervalMinutes;

    tracker_pi::getEndpointTypeBehavior(endpoint).applyDefaults(endpoint);
    endpoint.sendIntervalMinutes = adjustedIntervalForTypeChange(
        endpoint, currentInterval, preserveBoatKey);
    writeEndpointDefaultsToEditor(endpoint, preservedBoatKey, preserveBoatKey);
    updateEndpointTypeUi(endpoint.type);
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
    applyEndpointTypeDefaultsInEditor(true);
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
    includeAwaAwsCheck_->Enable(enabled);
    endpointNameCtrl_->Enable(enabled);
    endpointTypeChoice_->Enable(enabled);
    endpointIntervalSpin_->Enable(enabled);
    endpointMinDistanceSpin_->Enable(
        enabled && !tracker_pi::isNoForeignLandType(selectedEndpointType()));
    endpointUrlCtrl_->Enable(enabled);
    endpointTimeoutSpin_->Enable(enabled);
    endpointHeaderNameCtrl_->Enable(enabled);
    endpointHeaderValueCtrl_->Enable(enabled);

    if (!enabled) {
      resetEditorFields();
      updateEndpointTypeUi(tracker_pi::kEndpointTypeHttpJsonWithHeaderKey);
      updateEndpointErrorUi(nullptr);
    }
  }

  void onEndpointTypeChanged(wxCommandEvent&) {
    if (endpointTypeChoice_ == nullptr) {
      return;
    }
    const bool isNfl = tracker_pi::isNoForeignLandType(selectedEndpointType());
    if (isNfl && endpointNameCtrl_ != nullptr &&
        endpointNameCtrl_->GetValue().StartsWith("endpoint-")) {
      endpointNameCtrl_->SetValue(wxString::FromUTF8(
          tracker_pi::makeNflEndpointName(static_cast<std::size_t>(
              std::max(currentEndpointIndex_, 0)))
              .c_str()));
    }
    applyEndpointTypeDefaultsInEditor(false);
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

  wxBitmap detailHeaderBitmapForType(const std::string& type) const {
    return tracker_pi::isNoForeignLandType(type)
               ? tracker_plugin_ui::LoadBitmapFromPluginAsset(
                     "1tracker_nfl_header.png",
                     wxSize(kNflDetailHeaderWidth, kHeaderIconHeight))
               : tracker_plugin_ui::LoadBitmapFromPluginAssetWidth(
                     "1tracker_json_header.png", kJsonDetailHeaderWidth);
  }

  void applyDetailHeaderBitmap(const wxBitmap& headerBitmap) {
    detailHeaderIcon_->SetBitmap(headerBitmap);
    if (headerBitmap.IsOk()) {
      detailHeaderIcon_->SetMinSize(headerBitmap.GetSize());
      detailHeaderPanel_->SetMinSize(
          wxSize(kDetailHeaderFieldIndent + headerBitmap.GetWidth(),
                 headerBitmap.GetHeight()));
      return;
    }

    detailHeaderIcon_->SetMinSize(
        wxSize(kJsonDetailHeaderWidth, kJsonDetailHeaderHeight));
    detailHeaderPanel_->SetMinSize(
        wxSize(kDetailHeaderFieldIndent + kJsonDetailHeaderWidth,
               kJsonDetailHeaderHeight));
  }

  void refreshDetailHeaderLayout() {
    detailHeaderIcon_->Show();
    detailHeaderIcon_->InvalidateBestSize();
    detailHeaderPanel_->InvalidateBestSize();
    if (detailHeaderPanel_ != nullptr) {
      detailHeaderPanel_->Layout();
    }
    if (detailPanel_ != nullptr) {
      detailPanel_->Layout();
    }
  }

  void updateDetailHeading() {
    if (detailHeaderIcon_ == nullptr) {
      return;
    }

    applyDetailHeaderBitmap(detailHeaderBitmapForType(selectedEndpointType()));
    refreshDetailHeaderLayout();
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
    updateDetailHeading();
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
    editingNewEndpoint_ = true;
    config_.endpoints.push_back(makeDefaultEndpoint(config_.endpoints.size()));
    refreshEndpointList();
    const int selection = static_cast<int>(config_.endpoints.size() - 1);
    selectEndpointRow(selection);
    loadEndpointControls(selection);
    originalEndpoint_ = tracker_pi::EndpointConfig{};
    endpointNameCtrl_->Clear();
    endpointUrlCtrl_->Clear();
    endpointHeaderNameCtrl_->Clear();
    endpointHeaderValueCtrl_->Clear();
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
  wxPanel* listPanel_ = nullptr;
  wxPanel* detailPanel_ = nullptr;
  wxPanel* detailHeaderPanel_ = nullptr;
  wxStaticBitmap* detailHeaderIcon_ = nullptr;
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
  wxPanel* endpointErrorPanel_ = nullptr;
  wxStaticText* endpointErrorSummaryText_ = nullptr;
  wxStaticText* endpointErrorMetaText_ = nullptr;
  wxButton* endpointErrorDetailsToggle_ = nullptr;
  wxStaticText* endpointErrorText_ = nullptr;
  bool endpointErrorDetailsExpanded_ = false;
  wxPanel* endpointHeaderValueHelpPanel_ = nullptr;
  wxStaticText* endpointHeaderValueHelpText_ = nullptr;
  wxHyperlinkCtrl* endpointHeaderValueLink_ = nullptr;
  wxStaticText* endpointHeaderValueHelpTailText_ = nullptr;
  wxStaticText* endpointNameLabel_ = nullptr;
  wxStaticText* endpointTypeLabel_ = nullptr;
  wxStaticText* endpointIntervalLabel_ = nullptr;
  wxStaticText* endpointIntervalUnitsLabel_ = nullptr;
  wxStaticText* endpointMinDistanceLabel_ = nullptr;
  wxStaticText* endpointMinDistanceUnitsLabel_ = nullptr;
  wxStaticText* endpointUrlLabel_ = nullptr;
  wxStaticText* endpointTimeoutLabel_ = nullptr;
  wxStaticText* endpointTimeoutUnitsLabel_ = nullptr;
  wxStaticText* endpointHeaderNameLabel_ = nullptr;
  wxStaticText* endpointNflBoatKeyLabel_ = nullptr;
  wxStaticText* endpointHeaderValueLabel_ = nullptr;
  wxStaticText* includeAwaAwsLabel_ = nullptr;
  wxSizer* endpointIntervalRow_ = nullptr;
  wxSizer* endpointTimeoutRow_ = nullptr;
  wxCheckBox* includeAwaAwsCheck_ = nullptr;
  wxTextCtrl* endpointNameCtrl_ = nullptr;
  wxComboBox* endpointTypeChoice_ = nullptr;
  wxSpinCtrl* endpointIntervalSpin_ = nullptr;
  wxSpinCtrl* endpointMinDistanceSpin_ = nullptr;
  wxTextCtrl* endpointUrlCtrl_ = nullptr;
  wxSpinCtrl* endpointTimeoutSpin_ = nullptr;
  wxTextCtrl* endpointHeaderNameCtrl_ = nullptr;
  wxTextCtrl* endpointHeaderValueCtrl_ = nullptr;
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
