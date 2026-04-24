#include "endpoint_type_picker.h"

#include <vector>

#include <wx/arrstr.h>
#include <wx/choicdlg.h>
#include <wx/string.h>

#include "1tracker_pi/endpoint_policy.h"
#include "1tracker_pi/endpoint_type_behavior.h"

namespace tracker_plugin_ui {

namespace {

// User-facing label for a given type identifier. Keeps the raw machine
// string hidden from the picker. When a new type is registered in the
// domain layer, add a case here.
wxString displayLabelForType(const std::string& type) {
  if (type == tracker_pi::kEndpointTypeNoForeignLand) {
    return wxString("NoForeignLand");
  }
  if (type == tracker_pi::kEndpointTypeHttpJsonWithHeaderKey) {
    return wxString("Generic HTTP JSON with header key");
  }
  return wxString::FromUTF8(type.c_str());
}

}  // namespace

std::optional<std::string> PickEndpointType(wxWindow* parent) {
  const std::vector<std::string> types = tracker_pi::listEndpointTypes();
  if (types.empty()) {
    return std::nullopt;
  }

  wxArrayString labels;
  labels.Alloc(types.size());
  for (const auto& type : types) {
    labels.Add(displayLabelForType(type));
  }

  // Pre-select NFL if present, otherwise the first entry.
  int preselect = 0;
  for (std::size_t i = 0; i < types.size(); ++i) {
    if (types[i] == tracker_pi::kEndpointTypeNoForeignLand) {
      preselect = static_cast<int>(i);
      break;
    }
  }

  wxSingleChoiceDialog dialog(parent,
                              "Pick the tracker type. The type cannot be "
                              "changed after the tracker is created.",
                              "New tracker", labels);
  dialog.SetSelection(preselect);
  if (dialog.ShowModal() != wxID_OK) {
    return std::nullopt;
  }
  const int selection = dialog.GetSelection();
  if (selection < 0 || static_cast<std::size_t>(selection) >= types.size()) {
    return std::nullopt;
  }
  return types[static_cast<std::size_t>(selection)];
}

}  // namespace tracker_plugin_ui
