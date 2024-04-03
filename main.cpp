#pragma comment(lib, "User32.lib")
#define NOMINMAX
#include <Windows.h>
#include <codecvt>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace window_utils {
std::vector<HWND> get_all_windows() {
  std::vector<HWND> windows;
  EnumWindows(
      [](HWND h, LPARAM d) -> int {
        ((std::vector<HWND> *)d)->push_back(h);
        return TRUE;
      },
      (LPARAM)&windows);
  std::sort(windows.begin(), windows.end());
  windows.erase(std::unique(windows.begin(), windows.end()), windows.end());
  return windows;
}
std::wstring get_window_process_image_name(HWND window) {
  DWORD pid = -1;
  GetWindowThreadProcessId(window, &pid);
  if (pid == -1)
    return L"";
  auto process =
      OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, TRUE, pid);
  if (process == INVALID_HANDLE_VALUE)
    return L"";
  std::wstring path;
  path.resize(MAX_PATH);
  DWORD len = MAX_PATH;
  if (QueryFullProcessImageNameW(process, 0, path.data(), &len) == 0)
    return L"";
  path.resize(len);
  CloseHandle(process);
  if (path == L"C:\\Windows\\System32\\ApplicationFrameHost.exe") {
    std::wstring res;
    EnumChildWindows(
        window,
        [](HWND window, LPARAM res) -> int {
          DWORD pid = -1;
          GetWindowThreadProcessId(window, &pid);
          if (pid == -1)
            return TRUE;
          auto process = OpenProcess(
              PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, TRUE, pid);
          if (process == INVALID_HANDLE_VALUE)
            return TRUE;
          std::wstring path;
          path.resize(MAX_PATH);
          DWORD len = MAX_PATH;
          if (QueryFullProcessImageNameW(process, 0, path.data(), &len) == 0)
            return TRUE;
          path.resize(len);
          CloseHandle(process);
          if (path != L"C:\\Windows\\System32\\ApplicationFrameHost.exe") {
            *((std::wstring *)res) = path;
            return FALSE;
          }
          return TRUE;
        },
        (LPARAM)&res);
    return res == L"" ? L"C:\\Windows\\System32\\ApplicationFrameHost.exe"
                      : L"C:\\Windows\\System32\\ApplicationFrameHost.exe(" +
                            res + L")";
  }
  return path;
}

std::wstring get_window_title(HWND window) {
  auto len = 1024;
  std::wstring title;
  title.resize(len);
  len = GetWindowTextW(window, title.data(), len);
  if (len == 0)
    return L"";
  title.resize(std::max(len, 0));
  if (len <= 1024)
    return title;
  else {
    GetWindowTextW(window, title.data(), len);
    return title;
  }
}

std::wstring get_window_class_name(HWND window) {
  std::wstring class_name;
  int len = 1024;
  class_name.resize(len);
  len = GetClassNameW(window, class_name.data(), len);
  if (len == 0)
    return L"";
  class_name.resize(len);
  if (len <= 1024) {
    return class_name;
  } else {
    GetClassNameW(window, class_name.data(), len);
    return class_name;
  }
}

DWORD get_window_pid(HWND window) {
  DWORD pid = -1;
  GetWindowThreadProcessId(window, &pid);
  return pid;
}

void set_opacity(HWND window, BYTE alpha) {
  LONG windowLong = GetWindowLongA(window, GWL_EXSTYLE);
  SetWindowLongA(window, GWL_EXSTYLE, windowLong | WS_EX_LAYERED);
  SetLayeredWindowAttributes(window, 0, alpha, LWA_ALPHA);
}
} // namespace window_utils
std::vector<std::pair<std::wstring, BYTE>> names;
void init_config() {
  std::wcout << "init config\n";
  if (!std::filesystem::exists("./config.txt")) {
    std::ofstream config("./config.txt");
    config << "";
    config.close();
  }
  std::wifstream config("./config.txt");
  std::wstring line;
  while (std::getline(config, line)) {
    std::wstring alpha;
    std::getline(config, alpha);
    names.push_back({line, std::stoi(alpha)});
  }
}
void init_commandline(int argc, wchar_t const *argv[]) {
  std::wcout << "init commandline\n";
  bool is_name = true;
  for (int i = 1; i < argc; i++) {
    if (is_name) {
      names.push_back({argv[i], std::stoi(argv[i + 1])});
    }
    is_name = !is_name;
  }
}
void apply_opacity() {
  if (names.size() == 0)
    return;
  for (auto h : window_utils::get_all_windows()) {
    for (auto [name, v] : names) {
      if ((name == L"*" || window_utils::get_window_class_name(h) == name ||
           window_utils::get_window_process_image_name(h) == name ||
           window_utils::get_window_title(h) == name ||
           std::to_wstring(window_utils::get_window_pid(h)) == name) &&
          window_utils::get_window_process_image_name(h) != L"") {
        window_utils::set_opacity(h, v);
        std::wcout << "set " << window_utils::get_window_process_image_name(h)
                   << " to " << v << "\n";
      }
    }
  }
}

int wmain(int argc, wchar_t const *argv[]) {
  for (auto h : window_utils::get_all_windows()) {
    std::wcout << "class name:" << window_utils::get_window_class_name(h);
    std::wcout << " ";
    std::wcout << "title:" << window_utils::get_window_title(h);
    std::wcout.clear();
    std::wcout << " ";
    std::wcout << "image name:"
               << window_utils::get_window_process_image_name(h);
    std::wcout << " ";
    std::wcout << "pid:" << window_utils::get_window_pid(h);
    std::wcout << "\n";
  }

  init_config();
  init_commandline(argc, argv);
  apply_opacity();

  return 0;
}
