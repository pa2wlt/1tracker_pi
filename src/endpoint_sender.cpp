#include "1tracker_pi/endpoint_sender.h"
#include "1tracker_pi/endpoint_type_behavior.h"

#include <memory>
#include <mutex>
#include <sstream>

#include <curl/curl.h>
#include <wx/string.h>

#include <wx/curl/base.h>
#include <wx/curl/http.h>

namespace tracker_pi {
namespace {

class TrackerHttpClient final : public wxCurlHTTP {
public:
  explicit TrackerHttpClient(const wxString& url) : wxCurlHTTP(url) {}

  void AddHeader(const wxString& header) { m_arrHeaders.Add(header); }
  void ClearForms() { ResetPostData(); }
};

void initWxCurl() {
  static std::once_flag flag;
  std::call_once(flag, [] { wxCurlBase::Init(); });
}

std::string replaceAll(std::string text, const std::string& needle,
                       const std::string& replacement) {
  if (needle.empty()) {
    return text;
  }

  std::size_t position = 0;
  while ((position = text.find(needle, position)) != std::string::npos) {
    text.replace(position, needle.size(), replacement);
    position += replacement.size();
  }
  return text;
}

EndpointSender::Result validationFailureResult(const std::string& message) {
  EndpointSender::Result result;
  result.message = message;
  return result;
}

void addRequestHeaders(TrackerHttpClient& client, const EndpointRequest& request) {
  client.AddHeader(wxString::FromUTF8("Content-Type: " + request.contentType));
  for (const auto& header : request.headers) {
    client.AddHeader(wxString::FromUTF8(header));
  }
}

void configureTimeouts(TrackerHttpClient& client, const EndpointConfig& endpoint) {
  client.SetOpt(CURLOPT_NOSIGNAL, 1L);
  client.SetOpt(CURLOPT_TIMEOUT, static_cast<long>(endpoint.timeoutSeconds));
  client.SetOpt(CURLOPT_CONNECTTIMEOUT, static_cast<long>(endpoint.timeoutSeconds));
}

EndpointSender::Result makeSuccessResult(long httpStatus,
                                         const std::string& responseBody,
                                         const EndpointConfig& endpoint) {
  EndpointSender::Result result;
  result.success = true;
  result.httpStatus = httpStatus;

  std::ostringstream stream;
  stream << "HTTP " << httpStatus;
  if (!responseBody.empty()) {
    stream << ", responseBody="
           << EndpointSender::RedactSensitiveText(responseBody, endpoint);
  }
  result.message = stream.str();
  return result;
}

std::string buildFailureMessage(const TrackerHttpClient& client, bool posted,
                                long httpStatus, const std::string& responseBody,
                                const EndpointConfig& endpoint) {
  std::ostringstream stream;
  if (!posted) {
    stream << EndpointSender::RedactSensitiveText(client.GetErrorString(),
                                                  endpoint);
    const auto detailed = client.GetDetailedErrorString();
    const auto redactedDetailed =
        EndpointSender::RedactSensitiveText(detailed, endpoint);
    if (!redactedDetailed.empty() && redactedDetailed != stream.str()) {
      stream << " (" << redactedDetailed << ")";
    }
  } else {
    stream << "HTTP " << httpStatus;
  }

  if (!responseBody.empty()) {
    stream << " body="
           << EndpointSender::RedactSensitiveText(responseBody, endpoint);
  }

  return stream.str();
}

}  // namespace

EndpointSender::Result EndpointSender::send(const EndpointConfig& endpoint,
                                            const std::string& payload) const {
  initWxCurl();
  const auto& behavior = getEndpointTypeBehavior(endpoint);
  if (const auto validationError = behavior.validate(endpoint);
      validationError.has_value()) {
    return validationFailureResult(*validationError);
  }

  const EndpointRequest request = behavior.buildRequest(endpoint, payload);
  TrackerHttpClient client(wxString::FromUTF8(endpoint.url));
  addRequestHeaders(client, request);
  configureTimeouts(client, endpoint);

  const bool posted = client.Post(request.body.data(), request.body.size());
  const long httpStatus = client.GetResponseCode();
  const auto responseBody = client.GetResponseBody();
  if (posted && behavior.responseIndicatesSuccess(httpStatus, responseBody)) {
    return makeSuccessResult(httpStatus, responseBody, endpoint);
  }

  Result result;
  result.httpStatus = httpStatus;
  result.message =
      buildFailureMessage(client, posted, httpStatus, responseBody, endpoint);
  return result;
}

std::string EndpointSender::MaskSecret(const std::string& secret) {
  if (secret.empty()) {
    return "";
  }

  if (secret.size() <= 4) {
    return std::string(secret.size(), '*');
  }

  return std::string(secret.size() - 4, '*') + secret.substr(secret.size() - 4);
}

std::string EndpointSender::RedactSensitiveText(std::string text,
                                                const EndpointConfig& endpoint) {
  if (endpoint.headerValue.empty()) {
    return text;
  }

  return replaceAll(std::move(text), endpoint.headerValue,
                    MaskSecret(endpoint.headerValue));
}

}  // namespace tracker_pi
