#pragma once

#include "endpoint_editor_page.h"

class wxBoxSizer;
class wxButton;
class wxCheckBox;
class wxCommandEvent;
class wxPanel;
class wxSpinCtrl;
class wxStaticText;
class wxTextCtrl;

namespace tracker_plugin_ui {

// Editor page for `kEndpointTypeHttpJsonWithHeaderKey`. Fields: send interval
// + min-distance gating, include-wind toggle, URL, timeout, header name +
// value, in-page error panel.
class HttpJsonEndpointPage : public EndpointEditorPage {
 public:
  explicit HttpJsonEndpointPage(wxWindow* parent);

  const std::string& type() const override;
  void populate(const tracker_pi::EndpointConfig& config) override;
  void readInto(tracker_pi::EndpointConfig& config) const override;
  void setEnabled(bool enabled) override;
  void setErrorState(const tracker_pi::EndpointErrorUiState& state) override;

 private:
  void buildForm(wxBoxSizer* column);
  void buildErrorPanel(wxBoxSizer* column);
  void onToggleErrorDetails(wxCommandEvent& event);

  wxSpinCtrl* intervalSpin_ = nullptr;
  wxSpinCtrl* minDistanceSpin_ = nullptr;
  wxCheckBox* includeWindCheck_ = nullptr;
  wxTextCtrl* urlCtrl_ = nullptr;
  wxSpinCtrl* timeoutSpin_ = nullptr;
  wxTextCtrl* headerNameCtrl_ = nullptr;
  wxTextCtrl* headerValueCtrl_ = nullptr;

  wxPanel* errorPanel_ = nullptr;
  wxStaticText* errorSummary_ = nullptr;
  wxStaticText* errorMeta_ = nullptr;
  wxStaticText* errorDetails_ = nullptr;
  wxButton* errorToggle_ = nullptr;
  bool errorDetailsExpanded_ = false;
};

}  // namespace tracker_plugin_ui
