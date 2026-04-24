#pragma once

#include <functional>
#include <string>
#include <utility>

#include <wx/panel.h>

#include "1tracker_pi/endpoint_config.h"
#include "1tracker_pi/endpoint_error_summary.h"

namespace tracker_plugin_ui {

// Layout constants shared between the TrackerDialog frame (Name, Type combo,
// button row) and each EndpointEditorPage. Keeping them here means the Name
// field rendered by the dialog and a page's first field line up visually
// (same label column, same field width, same compact spin width).
inline constexpr int kDetailLabelWidth = 110;
inline constexpr int kDetailFieldWidth = 260;
inline constexpr int kCompactSpinWidth = 58;
inline constexpr int kHeaderNameFieldWidth = 90;
inline constexpr int kHeaderValueLabelWidth = 44;
inline constexpr int kDetailHeaderFieldIndent = 110;

// Width at which the in-page error panel wraps its text. Matches the
// visible width of the 2-column form above (label col + gutter + value
// col), so the error message occupies the same horizontal band as the
// fields it relates to instead of clumping in the left third.
inline constexpr int kErrorPanelWrapWidth =
    kDetailLabelWidth + 12 + kDetailFieldWidth;

// Abstract base for a per-tracker-type editor page. Each concrete subclass is
// a self-contained wxPanel that owns its type's hero image, field widgets and
// in-page error display, and knows how to round-trip itself to/from an
// EndpointConfig. The panels are placed into a wxSimplebook inside
// TrackerDialog; switching the selected page replaces the visible editor
// wholesale, so there is no shared show/hide state across types.
//
// Adding a new tracker type:
//   1. Declare the type's machine-identifier string in
//      include/1tracker_pi/endpoint_policy.h and register an
//      EndpointTypeBehavior implementation in
//      src/endpoint_type_registry.cpp (domain layer).
//   2. Subclass EndpointEditorPage with its own widgets and implement the
//      five virtuals below.
//   3. Add one line in TrackerDialog's book-construction code that
//      instantiates the new page and adds it to the book.
class EndpointEditorPage : public wxPanel {
 public:
  using wxPanel::wxPanel;
  ~EndpointEditorPage() override = default;

  // Stable machine identifier (matches one of the kEndpointType* constants
  // in include/1tracker_pi/endpoint_policy.h and EndpointConfig::type).
  virtual const std::string& type() const = 0;

  // Copy the type-relevant fields of `config` INTO this page's widgets.
  // Fields this type does not understand (e.g. URL for NFL) are ignored.
  virtual void populate(const tracker_pi::EndpointConfig& config) = 0;

  // Copy this page's widget values back INTO `config`. Fields this type
  // does not own are left untouched.
  virtual void readInto(tracker_pi::EndpointConfig& config) const = 0;

  // Enable or disable every input widget on the page. Called when the
  // editor becomes active / inactive (e.g. no tracker selected).
  virtual void setEnabled(bool enabled) = 0;

  // Apply the current endpoint's error state to the page's in-page error
  // display. Pass an `EndpointErrorUiState{}` to clear.
  virtual void setErrorState(
      const tracker_pi::EndpointErrorUiState& state) = 0;

  // Register a callback the page will fire when its own layout
  // requirements change (e.g. the error-details toggle was used and the
  // page now needs more or less vertical room). The owning dialog should
  // use this to re-fit its outer frame.
  void setOnContentChanged(std::function<void()> callback) {
    onContentChanged_ = std::move(callback);
  }

 protected:
  // Subclasses call this after any Show/Hide or text change that could
  // alter the page's minimum size.
  void notifyContentChanged() {
    if (onContentChanged_) onContentChanged_();
  }

 private:
  std::function<void()> onContentChanged_;
};

}  // namespace tracker_plugin_ui
