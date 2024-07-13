#pragma comment(lib, "User32.lib")
#define NOMINMAX
#include <Windows.h>
#include <comdef.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>


#include "GLFW/glfw3.h"
#include "gl/GL.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "CTC/CTStr.hpp"
#include "CTC/Options.hpp"


namespace window_utils {
std::vector<HWND> get_all_windows() {
    std::vector<HWND> windows;
    EnumWindows(
        [](HWND h, LPARAM d) -> int {
            ((std::vector<HWND>*)d)->push_back(h);
            return TRUE;
        },
        (LPARAM)&windows
    );
    std::sort(windows.begin(), windows.end());
    windows.erase(std::unique(windows.begin(), windows.end()), windows.end());
    return windows;
}

std::wstring get_window_process_image_name(HWND window) {
    DWORD pid = -1;
    GetWindowThreadProcessId(window, &pid);
    if (pid == -1) return L"";
    auto process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, TRUE, pid);
    if (process == INVALID_HANDLE_VALUE) return L"";
    std::wstring path;
    path.resize(MAX_PATH);
    DWORD len = MAX_PATH;
    if (QueryFullProcessImageNameW(process, 0, path.data(), &len) == 0) return L"";
    path.resize(len);
    CloseHandle(process);
    if (path == L"C:\\Windows\\System32\\ApplicationFrameHost.exe") {
        std::wstring res;
        EnumChildWindows(
            window,
            [](HWND window, LPARAM res) -> int {
                DWORD pid = -1;
                GetWindowThreadProcessId(window, &pid);
                if (pid == -1) return TRUE;
                auto process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, TRUE, pid);
                if (process == INVALID_HANDLE_VALUE) return TRUE;
                std::wstring path;
                path.resize(MAX_PATH);
                DWORD len = MAX_PATH;
                if (QueryFullProcessImageNameW(process, 0, path.data(), &len) == 0) return TRUE;
                path.resize(len);
                CloseHandle(process);
                if (path != L"C:\\Windows\\System32\\ApplicationFrameHost.exe") {
                    *((std::wstring*)res) = path;
                    return FALSE;
                }
                return TRUE;
            },
            (LPARAM)&res
        );
        return res == L"" ? L"C:\\Windows\\System32\\ApplicationFrameHost.exe"
                          : L"C:\\Windows\\System32\\ApplicationFrameHost.exe(" + res + L")";
    }
    return path;
}

std::wstring get_window_title(HWND window) {
    auto         len = 1024;
    std::wstring title;
    title.resize(len);
    len = GetWindowTextW(window, title.data(), len);
    if (len == 0) return L"";
    title.resize(std::max(len + 1, 0));
    if (len <= 1024) return title;
    else {
        GetWindowTextW(window, title.data(), len);
        return title;
    }
}

std::wstring get_window_class_name(HWND window) {
    std::wstring class_name;
    int          len = 1024;
    class_name.resize(len);
    len = GetClassNameW(window, class_name.data(), len);
    if (len == 0) return L"";
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


bool set_style(HWND window) {
    static std::unordered_map<HWND, bool> windows;
    if (windows.contains(window)) {
        return windows[window];
    }
    LONG windowLong = GetWindowLongA(window, GWL_EXSTYLE);
    if (windowLong & CS_CLASSDC || windowLong & CS_OWNDC) {
        windows[window] = false;
        return false;
    }
    SetWindowLongA(window, GWL_EXSTYLE, windowLong | WS_EX_LAYERED);
    windows[window] = true;
    return true;
}

void set_opacity(HWND window, BYTE alpha) {
    if (!set_style(window)) return;
    SetLayeredWindowAttributes(window, 0, alpha, LWA_ALPHA);
    return;
}

BYTE get_opacity(HWND window) {
    if (!set_style(window)) return 255;
    COLORREF key;
    BYTE     alpha;
    DWORD    flags;
    if (GetLayeredWindowAttributes(window, &key, &alpha, &flags) == 0) return 255;
    return alpha;
}

std::wstring str2wstr(const std::string& str) {
    std::wstring res;
    auto         len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.size(), nullptr, 0);
    if (len < 0) return res;
    auto buffer = new wchar_t[len + 1];
    if (!buffer) return res;
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.size(), buffer, len);
    buffer[len] = '\0';
    res         = buffer;
    delete[] buffer;
    return res;
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
    std::wstring   line;
    while (std::getline(config, line)) {
        std::wstring alpha;
        std::getline(config, alpha);
        names.push_back({line, std::stoi(alpha)});
    }
}

void init_commandline(int argc, wchar_t const* argv[]) {
    std::wcout << "init commandline\n";
    bool is_name = true;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == L'-') continue;
        if (is_name) {
            names.push_back({argv[i], std::stoi(argv[i + 1])});
        }
        is_name = !is_name;
    }
}

void apply_opacity() {
    if (names.size() == 0) return;
    for (auto h : window_utils::get_all_windows()) {
        for (auto [name, v] : names) {
            if ((name == L"*" || window_utils::get_window_class_name(h) == name
                 || window_utils::get_window_process_image_name(h) == name || window_utils::get_window_title(h) == name
                 || std::to_wstring(window_utils::get_window_pid(h)) == name)
                && window_utils::get_window_process_image_name(h) != L"") {
                window_utils::set_opacity(h, v);
                std::wcout << "set " << window_utils::get_window_process_image_name(h) << " to " << v << "\n";
            }
        }
    }
}

void init_imgui() {
    glfwSetErrorCallback([](auto error, auto desc) { std::cerr << "error:" << error << "\ndesc:" << desc << std::endl; });
    if (!glfwInit()) std::terminate();
    const char* glsl_ver = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    GLFWwindow* window = glfwCreateWindow(1024, 720, "opacity", nullptr, nullptr);
    if (!window) std::terminate();
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    ImGui::CreateContext();
    ImGuiIO& io     = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        {
            int width, height;
            glfwGetWindowSize(window, &width, &height);
            ImGui::SetNextWindowSize({(float)width, (float)height});
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowCollapsed(false);
            ImGui::Begin(
                "All Windows",
                nullptr,
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar
            );
            static char* search = new char[1024](0);
            ImGui::InputText("search", search, 1024);
            ImGui::SameLine();
            static bool useRegex = false;
            ImGui::Checkbox("use regex", &useRegex);
            bool regexErrorShowed = false;
            auto match            = [&](auto h) {
                try {
                    if (useRegex) {
                        std::wregex reg((std::wstring(window_utils::str2wstr(search))));
                        if (std::regex_match((window_utils::get_window_class_name(h)), reg)) return true;
                        if (std::regex_match((window_utils::get_window_process_image_name(h)), reg)) return true;
                        std::wstringstream wstream;
                        wstream << window_utils::get_window_title(h);
                        if (std::regex_match(wstream.str(), reg)) return true;
                    } else {
                        auto        s = window_utils::str2wstr(search);
                        std::wregex reg((L"[\\w\\W]*" + std::wstring(window_utils::str2wstr(search)) + L"[\\w\\W]*"));
                        if (std::regex_match((window_utils::get_window_class_name(h)), reg)) return true;
                        if (std::regex_match((window_utils::get_window_process_image_name(h)), reg)) return true;
                        std::wstringstream wstream;
                        wstream << window_utils::get_window_title(h);
                        if (std::regex_match(wstream.str(), reg)) return true;
                    }
                    if (std::to_string(window_utils::get_window_pid(h)) == search) return true;
                } catch (std::regex_error& e) {
                    if (useRegex) {
                        if (!regexErrorShowed) {
                            ImGui::TextColored(ImVec4{1.0, 0, 0, 1}, "regex error");
                            regexErrorShowed = true;
                        }
                        return true;
                    }
                } catch (...) {}

                return false;
            };
            auto sorted = [] {
                auto res = window_utils::get_all_windows();
                std::sort(res.begin(), res.end(), [](const auto& a, const auto& b) {
                    return window_utils::get_window_process_image_name(a) < window_utils::get_window_process_image_name(b);
                });
                return res;
            }();
            for (auto h : sorted) {
                if (search[0] != '\0' && !match(h)) continue;
                ImGui::Text("class name:%ws", window_utils::get_window_class_name(h).c_str());
                ImGui::SameLine();
                ImGui::Text("image name:%ws", window_utils::get_window_process_image_name(h).c_str());
                ImGui::SameLine();
                ImGui::Text("pid:%lu", window_utils::get_window_pid(h));
                ImGui::Text("title:%ws", window_utils::get_window_title(h).c_str());
                static std::unordered_map<void*, float> opmap = {};
                if (!opmap.contains(h)) {
                    opmap[h] = window_utils::get_opacity(h) * 1.0f / 255;
                }
                ImGui::SliderFloat(std::to_string((unsigned long long)h).c_str(), &opmap[h], 0.0f, 1.0f);
                if (opmap[h] != window_utils::get_opacity(h) * 1.0f / 255) {
                    window_utils::set_opacity(h, 255 * opmap[h]);
                }
            }
            ImGui::End();
        }
        ImGui::Render();
        int    display_w, display_h;
        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(
            clear_color.x * clear_color.w,
            clear_color.y * clear_color.w,
            clear_color.z * clear_color.w,
            clear_color.w
        );
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);

        static auto current_window_handle = []() -> HWND {
            for (auto h : window_utils::get_all_windows()) {
                if (window_utils::get_window_pid(h) == GetCurrentProcessId()
                    && window_utils::get_window_title(h) == L"opacity") {
                    return h;
                }
            }
            return nullptr;
        }();
        using namespace std::chrono_literals;
        if (current_window_handle && !IsWindowVisible(current_window_handle)) {
            std::this_thread::sleep_for(500ms);
        }
    }
}

int wmain(int argc, wchar_t const* argv[]) {
    opt_helper::Parser parser{argc, argv};
    if (!parser.exist<CTC::CTStr{L"--no_info_all_windows"}>())
        for (auto h : window_utils::get_all_windows()) {
            std::wcout << "class name:" << window_utils::get_window_class_name(h);
            std::wcout << " ";
            std::wcout << "title:" << window_utils::get_window_title(h);
            std::wcout.clear();
            std::wcout << " ";
            std::wcout << "image name:" << window_utils::get_window_process_image_name(h);
            std::wcout << " ";
            std::wcout << "pid:" << window_utils::get_window_pid(h);
            std::wcout << " ";
            std::wcout << "alpha:" << window_utils::get_opacity(h);
            std::wcout << "\n";
        }

    init_config();
    init_commandline(argc, argv);
    apply_opacity();
    if (!parser.exist<CTC::CTStr{L"--no_gui"}>()) init_imgui();
    return 0;
}
