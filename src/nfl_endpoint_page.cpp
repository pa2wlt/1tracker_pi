#include "nfl_endpoint_page.h"

#include <wx/button.h>
#include <wx/colour.h>
#include <wx/hyperlink.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/textctrl.h>

#include "1tracker_pi/endpoint_policy.h"
#include "1tracker_pi/nfl_settings.h"

namespace tracker_plugin_ui {

NflEndpointPage::NflEndpointPage(wxWindow* parent)
    : EndpointEditorPage(parent, wxID_ANY) {
  auto* column = new wxBoxSizer(wxVERTICAL);
  buildForm(column);
  buildErrorPanel(column);
  SetSizer(column);
}

const std::string& NflEndpointPage::type() const {
  static const std::string t = tracker_pi::kEndpointTypeNoForeignLand;
  return t;
}

void NflEndpointPage::buildForm(wxBoxSizer* column) {
  auto* form = new wxFlexGridSizer(0, 2, 8, 12);
  form->AddGrowableCol(1, 1);

  auto addLabel = [this, form](const wxString& text) {
    auto* label = new wxStaticText(this, wxID_ANY, text);
    label->SetMinSize(wxSize(kDetailLabelWidth, -1));
    form->Add(label, 0, wxALIGN_CENTER_VERTICAL);
  };

  // Send interval — NFL has no min-distance column, just "N minutes".
  addLabel("Send interval");
  auto* intervalRow = new wxBoxSizer(wxHORIZONTAL);
  intervalSpin_ = new wxSpinCtrl(this, wxID_ANY);
  intervalSpin_->SetRange(tracker_pi::kNflMinSendIntervalMinutes, 10080);
  intervalSpin_->SetValue(tracker_pi::kNflDefaultSendIntervalMinutes);
  intervalSpin_->SetMinSize(wxSize(kCompactSpinWidth, -1));
  intervalSpin_->SetMaxSize(wxSize(kCompactSpinWidth, -1));
  intervalRow->Add(intervalSpin_, 0, wxALIGN_CENTER_VERTICAL);
  auto* intervalUnits = new wxStaticText(this, wxID_ANY, "minutes");
  intervalRow->Add(intervalUnits, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, 8);
  form->Add(intervalRow, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);

  // NFL API key (NFL themselves call this a "boat key" in their UI; we
  // label it "API key" here to match the Type-specific field's role).
  addLabel("My NFL API key");
  boatKeyCtrl_ = new wxTextCtrl(this, wxID_ANY);
  boatKeyCtrl_->SetMinSize(wxSize(kDetailFieldWidth, -1));
  boatKeyCtrl_->SetMaxSize(wxSize(kDetailFieldWidth, -1));
  form->Add(boatKeyCtrl_, 1, wxEXPAND);

  // Help line under the boat key: "To get your API key, open <link> in NFL."
  form->AddSpacer(0);
  auto* helpRow = new wxBoxSizer(wxHORIZONTAL);
  auto* helpLead = new wxStaticText(this, wxID_ANY, "To get your API key, open");
  helpLead->SetForegroundColour(wxColour(72, 72, 72));
  // 4px left padding matches the Type static text above so static and
  // textbox-content rows share the same visual left edge.
  helpRow->Add(helpLead, 0,
               wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 4);
  auto* link = new wxHyperlinkCtrl(this, wxID_ANY, "Boat Tracking Settings",
                                   tracker_pi::nfl::boatTrackingSettingsUrl());
  link->SetNormalColour(wxColour(20, 78, 140));
  link->SetHoverColour(wxColour(14, 58, 106));
  link->SetVisitedColour(wxColour(84, 62, 132));
  helpRow->Add(link, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 4);
  auto* helpTail = new wxStaticText(this, wxID_ANY, "in NFL.");
  helpTail->SetForegroundColour(wxColour(72, 72, 72));
  helpRow->Add(helpTail, 0, wxALIGN_CENTER_VERTICAL);
  form->Add(helpRow, 0, wxTOP | wxEXPAND, 2);

  column->Add(form, 0, wxEXPAND | wxALL, 4);
}

void NflEndpointPage::buildErrorPanel(wxBoxSizer* column) {
  errorPanel_ = new wxPanel(this, wxID_ANY);
  auto* errorSizer = new wxBoxSizer(wxVERTICAL);

  errorSummary_ = new wxStaticText(errorPanel_, wxID_ANY, "");
  errorSummary_->SetForegroundColour(wxColour(160, 32, 32));
  errorSummary_->Wrap(kErrorPanelWrapWidth);
  errorSizer->Add(errorSummary_, 0, wxBOTTOM, 4);

  errorMeta_ = new wxStaticText(errorPanel_, wxID_ANY, "");
  errorMeta_->SetForegroundColour(wxColour(96, 96, 96));
  errorSizer->Add(errorMeta_, 0, wxBOTTOM, 4);

  errorToggle_ = new wxButton(errorPanel_, wxID_ANY, "Show technical details");
  errorSizer->Add(errorToggle_, 0, wxBOTTOM, 4);

  errorDetails_ = new wxStaticText(errorPanel_, wxID_ANY, "");
  errorDetails_->SetForegroundColour(wxColour(128, 44, 44));
  errorDetails_->Wrap(kErrorPanelWrapWidth);
  errorSizer->Add(errorDetails_, 0);

  errorPanel_->SetSizer(errorSizer);
  column->Add(errorPanel_, 0, wxTOP | wxLEFT | wxRIGHT | wxEXPAND, 4);

  errorPanel_->Hide();
  errorMeta_->Hide();
  errorToggle_->Hide();
  errorDetails_->Hide();

  errorToggle_->Bind(wxEVT_BUTTON, &NflEndpointPage::onToggleErrorDetails,
                     this);
}

void NflEndpointPage::populate(const tracker_pi::EndpointConfig& config) {
  if (intervalSpin_ != nullptr) {
    intervalSpin_->SetValue(std::max(tracker_pi::kNflMinSendIntervalMinutes,
                                     config.sendIntervalMinutes));
  }
  if (boatKeyCtrl_ != nullptr) {
    boatKeyCtrl_->SetValue(wxString::FromUTF8(config.headerValue.c_str()));
  }
}

void NflEndpointPage::readInto(tracker_pi::EndpointConfig& config) const {
  config.type = tracker_pi::kEndpointTypeNoForeignLand;
  if (intervalSpin_ != nullptr) {
    config.sendIntervalMinutes = intervalSpin_->GetValue();
  }
  if (boatKeyCtrl_ != nullptr) {
    config.headerValue = boatKeyCtrl_->GetValue().ToStdString();
  }
  config.minDistanceMeters = tracker_pi::nfl::kMinDistanceMeters;
  config.includeAwaAws = false;
}

void NflEndpointPage::setEnabled(bool enabled) {
  if (intervalSpin_ != nullptr) intervalSpin_->Enable(enabled);
  if (boatKeyCtrl_ != nullptr) boatKeyCtrl_->Enable(enabled);
}

void NflEndpointPage::setErrorState(
    const tracker_pi::EndpointErrorUiState& state) {
  if (errorPanel_ == nullptr) return;
  errorDetailsExpanded_ = false;
  if (!state.visible) {
    errorSummary_->SetLabel("");
    errorMeta_->SetLabel("");
    errorDetails_->SetLabel("");
    errorMeta_->Hide();
    errorDetails_->Hide();
    errorToggle_->Hide();
    errorPanel_->Hide();
    return;
  }
  errorSummary_->SetLabel(wxString::FromUTF8(state.summary.c_str()));
  errorSummary_->Wrap(kErrorPanelWrapWidth);
  if (!state.metaText.empty()) {
    errorMeta_->SetLabel(wxString::FromUTF8(state.metaText.c_str()));
    errorMeta_->Show();
  } else {
    errorMeta_->SetLabel("");
    errorMeta_->Hide();
  }
  if (!state.details.empty()) {
    errorDetails_->SetLabel(wxString::FromUTF8(state.details.c_str()));
    errorDetails_->Hide();
    errorToggle_->SetLabel("Show technical details");
    errorToggle_->Show();
  } else {
    errorDetails_->SetLabel("");
    errorDetails_->Hide();
    errorToggle_->Hide();
  }
  errorPanel_->Show();
}

void NflEndpointPage::onToggleErrorDetails(wxCommandEvent&) {
  if (errorPanel_ == nullptr || errorDetails_ == nullptr ||
      errorToggle_ == nullptr) {
    return;
  }
  errorDetailsExpanded_ = !errorDetailsExpanded_;
  errorDetails_->Show(errorDetailsExpanded_);
  if (errorDetailsExpanded_) {
    errorDetails_->Wrap(kErrorPanelWrapWidth);
  }
  errorToggle_->SetLabel(errorDetailsExpanded_ ? "Hide technical details"
                                               : "Show technical details");
  errorPanel_->InvalidateBestSize();
  errorPanel_->Layout();
  InvalidateBestSize();
  Layout();
  notifyContentChanged();
}

}  // namespace tracker_plugin_ui
