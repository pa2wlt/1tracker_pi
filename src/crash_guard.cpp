#include "crash_guard.h"

#include <sstream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
// dbghelp.h must come after windows.h
#include <dbghelp.h>
#include <eh.h>

#include <cstdio>
#include <mutex>

#pragma comment(lib, "Dbghelp.lib")
#endif

namespace tracker_pi {

#ifdef _WIN32
namespace {

std::filesystem::path g_dumpDir;
std::once_flag g_crashHandlerOnce;

void seTranslator(unsigned int code, EXCEPTION_POINTERS* info) {
  char buffer[256];
  void* faultIp = nullptr;
  const char* kind = "?";
  unsigned long long badAddr = 0;
  if (info != nullptr && info->ExceptionRecord != nullptr) {
    const auto* rec = info->ExceptionRecord;
    faultIp = rec->ExceptionAddress;
    if (rec->NumberParameters >= 2) {
      const auto op = rec->ExceptionInformation[0];
      badAddr = static_cast<unsigned long long>(rec->ExceptionInformation[1]);
      kind = (op == 0 ? "read" : op == 1 ? "write" : op == 8 ? "exec" : "?");
    }
  }
  std::snprintf(buffer, sizeof(buffer),
                "Windows structured exception 0x%08x at IP=%p kind=%s addr=0x%llx",
                code, faultIp, kind, badAddr);
  throw std::runtime_error(buffer);
}

LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* info) {
  if (g_dumpDir.empty()) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  std::error_code ec;
  std::filesystem::create_directories(g_dumpDir, ec);

  SYSTEMTIME st;
  GetLocalTime(&st);
  wchar_t name[64];
  swprintf_s(name, 64,
             L"%04u%02u%02u-%02u%02u%02u-1tracker-crash.dmp",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

  const auto path = g_dumpDir / name;
  HANDLE file = CreateFileW(path.wstring().c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
  dumpInfo.ThreadId = GetCurrentThreadId();
  dumpInfo.ExceptionPointers = info;
  dumpInfo.ClientPointers = FALSE;

  MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
                    MiniDumpNormal, &dumpInfo, nullptr, nullptr);
  CloseHandle(file);

  // Let the rest of the unwinding continue — OCPN may still terminate, but
  // now we have a dump file on disk explaining why.
  return EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace

void installSehTranslator() { _set_se_translator(seTranslator); }

void installCrashHandler(const std::filesystem::path& dumpDir) {
  std::call_once(g_crashHandlerOnce, [&] {
    g_dumpDir = dumpDir;
    SetUnhandledExceptionFilter(unhandledExceptionFilter);
  });
}

#else  // !_WIN32

void installSehTranslator() {}

#ifdef __ANDROID__
// On Android, OpenCPN dies silently with no log breadcrumb when a plugin
// crashes via SIGSEGV/SIGBUS/etc — `adb logcat` shows the kernel-level
// fault but nothing from us. Install a minimal signal handler that emits
// one async-signal-safe line to Android's logcat (tag "1tracker_pi") with
// the signal number and fault address, then re-raises to the default
// handler so OpenCPN still terminates as the OS expects. Re-raising is
// important: swallowing the signal would corrupt process state.
//
// Limited to async-signal-safe operations: snprintf is technically not on
// the POSIX safe list, but in practice glibc/bionic implementations are
// reentrant for simple format strings, and Android's __android_log_write
// uses a kernel device write internally. Worst case the handler itself
// crashes during logging, in which case we get the same outcome as before
// (silent OpenCPN death).

#include <android/log.h>
#include <signal.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <mutex>

namespace {

void crashSignalHandler(int sig, siginfo_t* info, void* /*ctx*/) {
  char buffer[256];
  void* faultAddr = info != nullptr ? info->si_addr : nullptr;
  std::snprintf(buffer, sizeof(buffer),
                "1tracker_pi: caught signal %d (%s) at addr=%p tid=%d",
                sig, strsignal(sig), faultAddr,
                static_cast<int>(gettid()));
  __android_log_write(ANDROID_LOG_ERROR, "1tracker_pi", buffer);

  // Reset to default handler and re-raise so the process actually dies.
  // Without this we'd loop forever if the same signal re-fires.
  struct sigaction dfl = {};
  dfl.sa_handler = SIG_DFL;
  sigemptyset(&dfl.sa_mask);
  sigaction(sig, &dfl, nullptr);
  raise(sig);
}

}  // namespace

void installCrashHandler(const std::filesystem::path&) {
  static std::once_flag once;
  std::call_once(once, [] {
    struct sigaction sa = {};
    sa.sa_sigaction = crashSignalHandler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    for (int sig : {SIGSEGV, SIGBUS, SIGABRT, SIGFPE, SIGILL}) {
      sigaction(sig, &sa, nullptr);
    }
  });
}

#else  // !__ANDROID__

void installCrashHandler(const std::filesystem::path&) {}

#endif  // __ANDROID__

#endif  // _WIN32

bool runGuarded(const char* tag, const std::function<void()>& fn,
                const std::function<void(const std::string&)>& logFn,
                std::string* errorOut) {
  try {
    fn();
    return true;
  } catch (const std::exception& error) {
    std::ostringstream stream;
    stream << "1tracker_pi: " << (tag ? tag : "<untagged>")
           << " caught exception: " << error.what();
    if (logFn) {
      logFn(stream.str());
    }
    if (errorOut != nullptr) {
      *errorOut = error.what();
    }
    return false;
  } catch (...) {
    std::ostringstream stream;
    stream << "1tracker_pi: " << (tag ? tag : "<untagged>")
           << " caught unknown exception";
    if (logFn) {
      logFn(stream.str());
    }
    if (errorOut != nullptr) {
      *errorOut = "unknown exception";
    }
    return false;
  }
}

}  // namespace tracker_pi
