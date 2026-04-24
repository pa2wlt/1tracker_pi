#pragma once

#include "endpoint_editor_page.h"

class wxBoxSizer;
class wxButton;
class wxCommandEvent;
class wxHyperlinkCtrl;
class wxPanel;
class wxSpinCtrl;
class wxStaticText;
class wxTextCtrl;

namespace tracker_plugin_ui {

// Editor page for `kEndpointTypeNoForeignLand`. Fields: send interval (no
// min-distance), NFL boat key, in-page help link, in-page error panel.
class NflEndpointPage : public EndpointEditorPage {
 public:
  explicit NflEndpointPage(wxWindow* parent);

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
  wxTextCtrl* boatKeyCtrl_ = nullptr;

  wxPanel* errorPanel_ = nullptr;
  wxStaticText* errorSummary_ = nullptr;
  wxStaticText* errorMeta_ = nullptr;
  wxStaticText* errorDetails_ = nullptr;
  wxButton* errorToggle_ = nullptr;
  bool errorDetailsExpanded_ = false;
};

}  // namespace tracker_plugin_ui
