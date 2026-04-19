#pragma once

#include <wx/bitmap.h>
#include <wx/colour.h>
#include <wx/gdicmn.h>
#include <wx/string.h>

namespace tracker_plugin_ui {

bool IsWindowsPlatform();
wxBitmap TintBitmap(const wxBitmap& bitmap, const wxColour& colour);
wxString FindPluginAssetPath(const wxString& fileName);
wxBitmap LoadBitmapFromPluginAsset(const wxString& fileName, const wxSize& size);
wxBitmap LoadBitmapFromPluginAssetWidth(const wxString& fileName, int targetWidth);

}  // namespace tracker_plugin_ui
