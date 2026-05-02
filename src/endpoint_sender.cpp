#include "1tracker_pi/endpoint_sender.h"
#include "1tracker_pi/endpoint_type_behavior.h"

#include <memory>
#include <mutex>
#include <sstream>

#include <curl/curl.h>
#include <wx/log.h>
#include <wx/string.h>

#include <wx/curl/base.h>
#include <wx/curl/http.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace tracker_pi {
namespace {

// Beta8 dies via SIGSEGV at addr=0x230 somewhere inside send() on the first
// scheduled HTTP post on Android, with no breadcrumb beyond "send start".
// Drop a step trace through the function so beta9's logcat pinpoints the
// exact line. wxLogMessage routes to opencpn.log on every platform; on
// Android we also mirror to logcat under tag "1tracker_pi" with ERROR level
// so `adb logcat -s 1tracker_pi:E '*:F'` captures it even if wx's buffered
// log fd doesn't flush before the crash.
void logSendStep(const char* step) {
#ifdef __ANDROID__
  __android_log_print(ANDROID_LOG_ERROR, "1tracker_pi",
                      "1tracker_pi: send step=%s", step);
#endif
  wxLogMessage("1tracker_pi: send step=%s", step);
}

class TrackerHttpClient final : public wxCurlHTTP {
public:
  explicit TrackerHttpClient(const wxString& url) : wxCurlHTTP(url) {}

  void AddHeader(const wxString& header) { m_arrHeaders.Add(header); }
  void ClearForms() { ResetPostData(); }
};

void initWxCurl() {
  static std::once_flag flag;
  std::call_once(flag, [] {
    wxCurlBase::Init();
    // Log the libcurl + bundled-SSL versions exactly once. curl_version()
    // returns "libcurl/X OpenSSL/Y zlib/Z ..." which tells us which copy
    // of OpenSSL we actually resolved against — useful for diagnosing the
    // "two OpenSSLs in one process" scenario on Android (libgorp's hidden
    // static copy vs. our bundled libssl/libcrypto).
    const char* ver = curl_version();
    const char* safe = ver != nullptr ? ver : "<null>";
    wxLogMessage("1tracker_pi: send curl_version=%s", safe);
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_ERROR, "1tracker_pi",
                        "1tracker_pi: send curl_version=%s", safe);
#endif
  });
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
  client.AddHeader(wxString::FromUTF8(("Content-Type: " + request.contentType).c_str()));
  for (const auto& header : request.headers) {
    client.AddHeader(wxString::FromUTF8(header.c_str()));
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
  logSendStep("entered");
  initWxCurl();
  logSendStep("after_init_wx_curl");
  const auto& behavior = getEndpointTypeBehavior(endpoint);
  logSendStep("after_get_behavior");
  if (const auto validationError = behavior.validate(endpoint);
      validationError.has_value()) {
    logSendStep("validation_failed");
    return validationFailureResult(*validationError);
  }
  logSendStep("after_validate");

  const EndpointRequest request = behavior.buildRequest(endpoint, payload);
  logSendStep("after_build_request");
  TrackerHttpClient client(wxString::FromUTF8(endpoint.url.c_str()));
  logSendStep("after_client_ctor");
  if (!client.IsOk()) {
    // wxCurlBase sets m_pCURL to whatever curl_easy_init() returned; IsOk()
    // is just `m_pCURL != NULL`. A NULL handle here means our bundled
    // libcurl/openssl static archives didn't initialise correctly inside
    // the dlopen'd plugin .so. Bail with a clear message instead of letting
    // the next SetOpt deref a NULL handle and crash OpenCPN.
    logSendStep("client_handle_null");
    Result result;
    result.message =
        "curl handle is NULL after wxCurlHTTP construction "
        "(curl_easy_init returned NULL — likely OpenSSL/libcurl init "
        "failure inside the bundled plugin .so)";
    return result;
  }
  addRequestHeaders(client, request);
  logSendStep("after_add_headers");
  configureTimeouts(client, endpoint);
  logSendStep("after_configure_timeouts");

  const bool posted = client.Post(request.body.data(), request.body.size());
  logSendStep(posted ? "post_returned_true" : "post_returned_false");
  if (!posted) {
    Result result;
    result.httpStatus = 0;
    result.message = buildFailureMessage(client, posted, 0, std::string{}, endpoint);
    return result;
  }

  const long httpStatus = client.GetResponseCode();
  const auto responseBody = client.GetResponseBody();
  logSendStep("after_read_response");
  if (behavior.responseIndicatesSuccess(httpStatus, responseBody)) {
    return makeSuccessResult(httpStatus, responseBody, endpoint);
  }

  Result result;
  result.httpStatus = httpStatus;
  result.message =
      buildFailureMessage(client, posted, httpStatus, responseBody, endpoint);
  return result;
}

void EndpointSender::Prewarm() {
  logSendStep("prewarm_entered");
  initWxCurl();
  // Construct + immediately destroy a wxCurlHTTP. Whatever the calling
  // thread is, this forces curl_easy_init/cleanup to run on it, which on
  // Android allocates TLS slots and triggers any lazy OpenSSL ctors that
  // CURL_GLOBAL_ALL didn't already cover. The URL is a placeholder; we
  // never call .Post() so no network I/O happens.
  TrackerHttpClient probe(wxT("http://prewarm.invalid/"));
  logSendStep(probe.IsOk() ? "prewarm_probe_ok" : "prewarm_probe_null_handle");
  logSendStep("prewarm_done");
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
