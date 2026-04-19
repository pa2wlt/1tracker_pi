#include "plugin_ui_utils.h"

#include <filesystem>
#include <vector>

#include <wx/bmpbndl.h>
#include <wx/filename.h>
#include <wx/image.h>
#include <wx/platform.h>

#include "ocpn_plugin.h"

namespace tracker_plugin_ui {
namespace {

const char* kPluginName = "1tracker_pi";

wxString existingAssetPath(const std::filesystem::path& path) {
  return std::filesystem::exists(path) ? wxString::FromUTF8(path.string().c_str())
                                       : "";
}

std::vector<std::filesystem::path> pluginAssetSearchRoots() {
  std::vector<std::filesystem::path> roots;
  const wxString pluginDataDir = GetPluginDataDir(kPluginName);
  if (!pluginDataDir.empty()) {
    roots.emplace_back(pluginDataDir.ToStdString());
  }

  const std::filesystem::path sourceDataDir =
      std::filesystem::path(__FILE__).parent_path().parent_path() / "data" /
      "icons";
  if (std::filesystem::exists(sourceDataDir)) {
    roots.push_back(sourceDataDir);
  }
  return roots;
}

std::vector<std::filesystem::path> candidateAssetPaths(
    const std::filesystem::path& root, const std::filesystem::path& fileNamePath) {
  return {root / "data" / fileNamePath, root / "data" / "icons" / fileNamePath,
          root / fileNamePath};
}

}  // namespace

bool IsWindowsPlatform() {
  return wxPlatformInfo::Get().GetOperatingSystemId() & wxOS_WINDOWS;
}

wxBitmap TintBitmap(const wxBitmap& bitmap, const wxColour& colour) {
  if (!bitmap.IsOk()) {
    return wxNullBitmap;
  }

  wxImage image = bitmap.ConvertToImage();
  if (!image.IsOk()) {
    return wxNullBitmap;
  }

  if (!image.HasAlpha()) {
    image.InitAlpha();
  }

  unsigned char* data = image.GetData();
  unsigned char* alpha = image.GetAlpha();
  if (data == nullptr) {
    return wxNullBitmap;
  }

  const int pixelCount = image.GetWidth() * image.GetHeight();
  for (int i = 0; i < pixelCount; ++i) {
    if (alpha != nullptr && alpha[i] == 0) {
      continue;
    }
    data[i * 3] = colour.Red();
    data[i * 3 + 1] = colour.Green();
    data[i * 3 + 2] = colour.Blue();
  }

  return wxBitmap(image);
}

wxString FindPluginAssetPath(const wxString& fileName) {
  const auto roots = pluginAssetSearchRoots();
  const std::filesystem::path fileNamePath(fileName.ToStdString());
  for (const auto& root : roots) {
    for (const auto& candidate : candidateAssetPaths(root, fileNamePath)) {
      const wxString path = existingAssetPath(candidate);
      if (!path.empty()) {
        return path;
      }
    }
  }

  return "";
}

wxBitmap LoadBitmapFromPluginAsset(const wxString& fileName, const wxSize& size) {
  const wxString path = FindPluginAssetPath(fileName);
  if (path.empty()) {
    return wxNullBitmap;
  }

  wxFileName assetPath(path);
  if (assetPath.GetExt().CmpNoCase("svg") == 0) {
    return wxBitmapBundle::FromSVGFile(path, size).GetBitmap(size);
  }

  wxImage image;
  if (!image.LoadFile(path)) {
    return wxNullBitmap;
  }
  if (size.IsFullySpecified() && size.GetWidth() > 0 && size.GetHeight() > 0 &&
      (image.GetWidth() != size.GetWidth() ||
       image.GetHeight() != size.GetHeight())) {
    image.Rescale(size.GetWidth(), size.GetHeight(), wxIMAGE_QUALITY_HIGH);
  }
  return wxBitmap(image);
}

wxBitmap LoadBitmapFromPluginAssetWidth(const wxString& fileName,
                                        int targetWidth) {
  const wxString path = FindPluginAssetPath(fileName);
  if (path.empty() || targetWidth <= 0) {
    return wxNullBitmap;
  }

  wxFileName assetPath(path);
  if (assetPath.GetExt().CmpNoCase("svg") == 0) {
    return LoadBitmapFromPluginAsset(fileName, wxSize(targetWidth, targetWidth));
  }

  wxImage image;
  if (!image.LoadFile(path) || image.GetWidth() <= 0 || image.GetHeight() <= 0) {
    return wxNullBitmap;
  }

  const int targetHeight =
      std::max(1, (image.GetHeight() * targetWidth) / image.GetWidth());
  if (image.GetWidth() != targetWidth || image.GetHeight() != targetHeight) {
    image.Rescale(targetWidth, targetHeight, wxIMAGE_QUALITY_HIGH);
  }

  return wxBitmap(image);
}

}  // namespace tracker_plugin_ui
