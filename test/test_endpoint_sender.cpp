#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

#include <wx/init.h>

#include "1tracker_pi/endpoint_policy.h"
#include "1tracker_pi/endpoint_sender.h"
#include "1tracker_pi/nfl_settings.h"

namespace {

void expect(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

struct ServerCapture {
  int port = 0;
  std::string request;
};

struct ServerContext {
  std::shared_ptr<ServerCapture> capture;
  std::thread thread;
};

std::optional<ServerContext> runServerOnce() {
  auto capture = std::make_shared<ServerCapture>();

  const int serverFd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (serverFd < 0) {
    return std::nullopt;
  }

  int opt = 1;
  ::setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in address{};
#ifdef __APPLE__
  address.sin_len = sizeof(address);
#endif
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = 0;

  if (::bind(serverFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    ::close(serverFd);
    return std::nullopt;
  }

  socklen_t addressLength = sizeof(address);
  if (::getsockname(serverFd, reinterpret_cast<sockaddr*>(&address), &addressLength) != 0) {
    ::close(serverFd);
    return std::nullopt;
  }

  capture->port = ntohs(address.sin_port);

  if (::listen(serverFd, 1) != 0) {
    ::close(serverFd);
    return std::nullopt;
  }

  std::thread serverThread([capture, serverFd] {
    const int clientFd = ::accept(serverFd, nullptr, nullptr);
    if (clientFd < 0) {
      ::close(serverFd);
      return;
    }

    char buffer[8192];
    ssize_t count = 0;
    while ((count = ::recv(clientFd, buffer, sizeof(buffer), 0)) > 0) {
      capture->request.append(buffer, static_cast<std::size_t>(count));
      if (capture->request.find("\r\n\r\n") != std::string::npos &&
          (capture->request.find("hello-body") != std::string::npos ||
           capture->request.find("boatApiKey") != std::string::npos)) {
        break;
      }
    }

    static constexpr char response[] =
        "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    ::send(clientFd, response, sizeof(response) - 1, 0);
    ::close(clientFd);
    ::close(serverFd);
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  return ServerContext{capture, std::move(serverThread)};
}

}  // namespace

int main() {
  wxInitializer initializer;
  expect(initializer.IsOk(), "wxWidgets initialization failed");

  auto context = runServerOnce();
  if (!context.has_value()) {
    return EXIT_SUCCESS;
  }

  tracker_pi::EndpointConfig endpoint;
  endpoint.name = "local";
  endpoint.url =
      "http://127.0.0.1:" + std::to_string(context->capture->port) + "/position";
  endpoint.timeoutSeconds = 2;
  endpoint.headerName = "X-API-Key";
  endpoint.headerValue = "SECRET1234";

  tracker_pi::EndpointSender sender;
  const auto result = sender.send(endpoint, "hello-body");

  expect(result.success, "sender should succeed");
  expect(result.httpStatus == 204, "unexpected HTTP status");

  context->thread.join();

  expect(context->capture->request.find("POST /position HTTP/1.1") != std::string::npos,
         "missing request line");
  expect(context->capture->request.find("Content-Type: application/json") !=
             std::string::npos,
         "missing content type");
  expect(context->capture->request.find("X-API-Key: SECRET1234") != std::string::npos,
         "missing auth header");
  expect(context->capture->request.find("hello-body") != std::string::npos,
         "missing body");

  auto nflContext = runServerOnce();
  if (!nflContext.has_value()) {
    return EXIT_SUCCESS;
  }

  tracker_pi::EndpointConfig nflEndpoint;
  nflEndpoint.name = "nfl";
  nflEndpoint.type = tracker_pi::kEndpointTypeNoForeignLand;
  nflEndpoint.url =
      "http://127.0.0.1:" + std::to_string(nflContext->capture->port) + "/track";
  nflEndpoint.timeoutSeconds = 2;
  nflEndpoint.headerValue = "424534f5-13bc-42e8-ad02-33f9e27f7750";
  tracker_pi::normalizeEndpointConfig(nflEndpoint);

  const auto nflResult = sender.send(
      nflEndpoint, "[[1710000000000,52.12345,4.98765]]");

  expect(nflResult.success, "NFL sender should succeed");
  expect(nflResult.httpStatus == 204, "unexpected NFL HTTP status");

  nflContext->thread.join();

  expect(nflContext->capture->request.find("POST /track HTTP/1.1") !=
             std::string::npos,
         "missing NFL request line");
  expect(nflContext->capture->request.find(
             "Content-Type: application/x-www-form-urlencoded") !=
             std::string::npos,
         "missing NFL form urlencoded content type");
  expect(nflContext->capture->request.find(
             std::string("X-NFL-API-Key: ") + tracker_pi::nfl::restApiKey()) !=
             std::string::npos,
         "missing NFL auth header");
  expect(nflContext->capture->request.find("boatApiKey=") != std::string::npos,
         "missing NFL boatApiKey field");
  expect(nflContext->capture->request.find(
             "424534f5-13bc-42e8-ad02-33f9e27f7750") != std::string::npos,
         "missing NFL boat key");
  expect(nflContext->capture->request.find("&track=") != std::string::npos,
         "missing NFL track field");
  expect(nflContext->capture->request.find(
             "%5B%5B1710000000000%2C52.12345%2C4.98765%5D%5D") !=
             std::string::npos,
         "missing NFL track payload");

  tracker_pi::EndpointConfig invalidNflEndpoint = nflEndpoint;
  invalidNflEndpoint.headerValue.clear();
  const auto missingBoatKeyResult =
      sender.send(invalidNflEndpoint, "[[1710000000000,52.12345,4.98765]]");
  expect(!missingBoatKeyResult.success,
         "NFL sender should reject empty boat key");
  expect(missingBoatKeyResult.message == "NFL boat key is required",
         "unexpected empty boat key error");

  invalidNflEndpoint.headerValue = "not-a-uuid";
  const auto invalidBoatKeyResult =
      sender.send(invalidNflEndpoint, "[[1710000000000,52.12345,4.98765]]");
  expect(!invalidBoatKeyResult.success,
         "NFL sender should reject invalid boat key format");
  expect(invalidBoatKeyResult.message == "NFL boat key format is invalid",
         "unexpected invalid boat key error");

  return EXIT_SUCCESS;
}
