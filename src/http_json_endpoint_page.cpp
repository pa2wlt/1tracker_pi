#include "http_json_endpoint_page.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/colour.h>
#include <wx/intl.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/translation.h>

#include "1tracker_pi/endpoint_policy.h"

namespace tracker_plugin_ui {

HttpJsonEndpointPage::HttpJsonEndpointPage(wxWindow* parent)
    : EndpointEditorPage(parent, wxID_ANY) {
  auto* column = new wxBoxSizer(wxVERTICAL);
  buildForm(column);
  buildErrorPanel(column);
  SetSizer(column);
}

const std::string& HttpJsonEndpointPage::type() const {
  static const std::string t = tracker_pi::kEndpointTypeHttpJsonWithHeaderKey;
  return t;
}

void HttpJsonEndpointPage::buildForm(wxBoxSizer* column) {
  auto* form = new wxFlexGridSizer(0, 2, 8, 12);
  form->AddGrowableCol(1, 1);

  auto addLabel = [this, form](const wxString& text) {
    auto* label = new wxStaticText(this, wxID_ANY, text);
    label->SetMinSize(wxSize(kDetailLabelWidth, -1));
    form->Add(label, 0, wxALIGN_CENTER_VERTICAL);
  };

  // Send interval + min-distance gating row:
  //   [spin] minutes, if moved at least [spin] meters
  addLabel(_("Send interval"));
  auto* intervalRow = new wxBoxSizer(wxHORIZONTAL);
  intervalSpin_ = new wxSpinCtrl(this, wxID_ANY);
  intervalSpin_->SetRange(1, 10080);
  intervalSpin_->SetValue(tracker_pi::kDefaultSendIntervalMinutes);
  intervalSpin_->SetMinSize(wxSize(kCompactSpinWidth, -1));
  intervalSpin_->SetMaxSize(wxSize(kCompactSpinWidth, -1));
  intervalRow->Add(intervalSpin_, 0, wxALIGN_CENTER_VERTICAL);
  auto* intervalUnits =
      new wxStaticText(this, wxID_ANY, _("minutes, if moved at least"));
  intervalRow->Add(intervalUnits, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, 8);
  intervalRow->AddSpacer(8);
  minDistanceSpin_ = new wxSpinCtrl(this, wxID_ANY);
  minDistanceSpin_->SetRange(0, 100000);
  minDistanceSpin_->SetValue(tracker_pi::kDefaultMinDistanceMeters);
  minDistanceSpin_->SetMinSize(wxSize(kCompactSpinWidth, -1));
  minDistanceSpin_->SetMaxSize(wxSize(kCompactSpinWidth, -1));
  intervalRow->Add(minDistanceSpin_, 0, wxALIGN_CENTER_VERTICAL);
  auto* distanceUnits = new wxStaticText(this, wxID_ANY, _("meters"));
  intervalRow->Add(distanceUnits, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, 8);
  form->Add(intervalRow, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);

  // Include wind
  addLabel(_("Include wind"));
  includeWindCheck_ = new wxCheckBox(this, wxID_ANY, "", wxDefaultPosition,
                                     wxDefaultSize, wxCHK_2STATE | wxWANTS_CHARS);
  form->Add(includeWindCheck_, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);

  // URL
  addLabel(_("URL"));
  urlCtrl_ = new wxTextCtrl(this, wxID_ANY);
  urlCtrl_->SetMinSize(wxSize(kDetailFieldWidth, -1));
  urlCtrl_->SetMaxSize(wxSize(kDetailFieldWidth, -1));
  form->Add(urlCtrl_, 1, wxEXPAND);

  // Timeout
  addLabel(_("Timeout"));
  auto* timeoutRow = new wxBoxSizer(wxHORIZONTAL);
  timeoutSpin_ = new wxSpinCtrl(this, wxID_ANY);
  timeoutSpin_->SetRange(1, 600);
  timeoutSpin_->SetValue(10);
  timeoutSpin_->SetMinSize(wxSize(kCompactSpinWidth, -1));
  timeoutSpin_->SetMaxSize(wxSize(kCompactSpinWidth, -1));
  timeoutRow->Add(timeoutSpin_, 0, wxALIGN_CENTER_VERTICAL);
  auto* timeoutUnits = new wxStaticText(this, wxID_ANY, _("seconds"));
  timeoutRow->Add(timeoutUnits, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, 8);
  form->Add(timeoutRow, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);

  // Header name + Value — two inputs side-by-side under one label cell.
  addLabel(_("Header name"));
  auto* headerRow = new wxBoxSizer(wxHORIZONTAL);
  headerNameCtrl_ = new wxTextCtrl(this, wxID_ANY);
  headerNameCtrl_->SetMinSize(wxSize(kHeaderNameFieldWidth, -1));
  headerNameCtrl_->SetMaxSize(wxSize(kHeaderNameFieldWidth, -1));
  headerRow->Add(headerNameCtrl_, 0, wxALIGN_CENTER_VERTICAL);
  auto* headerValueLabel = new wxStaticText(this, wxID_ANY, _("Value"));
  headerValueLabel->SetMinSize(wxSize(kHeaderValueLabelWidth, -1));
  headerRow->Add(headerValueLabel, 0,
                 wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 8);
  headerValueCtrl_ = new wxTextCtrl(this, wxID_ANY);
  headerRow->Add(headerValueCtrl_, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);
  form->Add(headerRow, 1, wxEXPAND);

  column->Add(form, 0, wxEXPAND | wxALL, 4);
}

void HttpJsonEndpointPage::buildErrorPanel(wxBoxSizer* column) {
  errorPanel_ = new wxPanel(this, wxID_ANY);
  auto* errorSizer = new wxBoxSizer(wxVERTICAL);

  errorSummary_ = new wxStaticText(errorPanel_, wxID_ANY, "");
  errorSummary_->SetForegroundColour(wxColour(160, 32, 32));
  errorSummary_->Wrap(kErrorPanelWrapWidth);
  errorSizer->Add(errorSummary_, 0, wxBOTTOM, 4);

  errorMeta_ = new wxStaticText(errorPanel_, wxID_ANY, "");
  errorMeta_->SetForegroundColour(wxColour(96, 96, 96));
  errorSizer->Add(errorMeta_, 0, wxBOTTOM, 4);

  errorToggle_ = new wxButton(errorPanel_, wxID_ANY, _("Show technical details"));
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

  errorToggle_->Bind(wxEVT_BUTTON,
                     &HttpJsonEndpointPage::onToggleErrorDetails, this);
}

void HttpJsonEndpointPage::populate(const tracker_pi::EndpointConfig& config) {
  if (intervalSpin_ != nullptr) {
    intervalSpin_->SetValue(std::max(1, config.sendIntervalMinutes));
  }
  if (minDistanceSpin_ != nullptr) {
    minDistanceSpin_->SetValue(std::max(0, config.minDistanceMeters));
  }
  if (includeWindCheck_ != nullptr) {
    includeWindCheck_->SetValue(config.includeAwaAws);
  }
  if (urlCtrl_ != nullptr) {
    urlCtrl_->SetValue(wxString::FromUTF8(config.url.c_str()));
  }
  if (timeoutSpin_ != nullptr) {
    timeoutSpin_->SetValue(std::max(1, config.timeoutSeconds));
  }
  if (headerNameCtrl_ != nullptr) {
    headerNameCtrl_->SetValue(wxString::FromUTF8(config.headerName.c_str()));
  }
  if (headerValueCtrl_ != nullptr) {
    headerValueCtrl_->SetValue(wxString::FromUTF8(config.headerValue.c_str()));
  }
}

void HttpJsonEndpointPage::readInto(tracker_pi::EndpointConfig& config) const {
  config.type = tracker_pi::kEndpointTypeHttpJsonWithHeaderKey;
  if (intervalSpin_ != nullptr) {
    config.sendIntervalMinutes = intervalSpin_->GetValue();
  }
  if (minDistanceSpin_ != nullptr) {
    config.minDistanceMeters = minDistanceSpin_->GetValue();
  }
  if (includeWindCheck_ != nullptr) {
    config.includeAwaAws = includeWindCheck_->GetValue();
  }
  if (urlCtrl_ != nullptr) {
    config.url = urlCtrl_->GetValue().ToStdString();
  }
  if (timeoutSpin_ != nullptr) {
    config.timeoutSeconds = timeoutSpin_->GetValue();
  }
  if (headerNameCtrl_ != nullptr) {
    config.headerName = headerNameCtrl_->GetValue().ToStdString();
  }
  if (headerValueCtrl_ != nullptr) {
    config.headerValue = headerValueCtrl_->GetValue().ToStdString();
  }
}

void HttpJsonEndpointPage::setEnabled(bool enabled) {
  for (wxWindow* w : {static_cast<wxWindow*>(intervalSpin_),
                      static_cast<wxWindow*>(minDistanceSpin_),
                      static_cast<wxWindow*>(includeWindCheck_),
                      static_cast<wxWindow*>(urlCtrl_),
                      static_cast<wxWindow*>(timeoutSpin_),
                      static_cast<wxWindow*>(headerNameCtrl_),
                      static_cast<wxWindow*>(headerValueCtrl_)}) {
    if (w != nullptr) w->Enable(enabled);
  }
}

void HttpJsonEndpointPage::setErrorState(
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
  // state.summary is a TR_NOOP literal from core; translate at display time.
  errorSummary_->SetLabel(wxGetTranslation(
      wxString::FromUTF8(state.summary.c_str())));
  errorSummary_->Wrap(kErrorPanelWrapWidth);
  if (!state.lastSentLocalTime.empty()) {
    errorMeta_->SetLabel(wxString::Format(
        _("Last call: %s"),
        wxString::FromUTF8(state.lastSentLocalTime.c_str())));
    errorMeta_->Show();
  } else {
    errorMeta_->SetLabel("");
    errorMeta_->Hide();
  }
  // Only offer the details toggle when there's actually raw text behind
  // the summary; otherwise the user sees an empty reveal. Note: we do
  // NOT call Wrap() here — wxStaticText::Wrap measures the control's
  // current width, which is 0 while the control is hidden; wrapping at
  // that moment mutates the label into an empty / broken layout. Wrap
  // happens in onToggleErrorDetails when the control becomes visible.
  if (!state.details.empty()) {
    errorDetails_->SetLabel(wxString::FromUTF8(state.details.c_str()));
    errorDetails_->Hide();
    errorToggle_->SetLabel(_("Show technical details"));
    errorToggle_->Show();
  } else {
    errorDetails_->SetLabel("");
    errorDetails_->Hide();
    errorToggle_->Hide();
  }
  errorPanel_->Show();
}

void HttpJsonEndpointPage::onToggleErrorDetails(wxCommandEvent&) {
  if (errorPanel_ == nullptr || errorDetails_ == nullptr ||
      errorToggle_ == nullptr) {
    return;
  }
  errorDetailsExpanded_ = !errorDetailsExpanded_;
  errorDetails_->Show(errorDetailsExpanded_);
  if (errorDetailsExpanded_) {
    // Wrap the label now that the control is visible and has a real
    // width; wrapping while hidden measures zero width and produces an
    // empty layout.
    errorDetails_->Wrap(kErrorPanelWrapWidth);
  }
  errorToggle_->SetLabel(errorDetailsExpanded_ ? _("Hide technical details")
                                               : _("Show technical details"));
  // Invalidate cached best-sizes up the chain — otherwise
  // ComputeFittingWindowSize keeps reporting the pre-expanded dimensions
  // and neither the error panel's container nor the dialog frame grows
  // to include the newly-visible details text.
  errorPanel_->InvalidateBestSize();
  errorPanel_->Layout();
  InvalidateBestSize();
  Layout();
  notifyContentChanged();
}

}  // namespace tracker_plugin_ui
