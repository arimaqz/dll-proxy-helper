#include <Windows.h>
#include <TlHelp32.h>
#include <imagehlp.h>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Imagehlp.lib")

struct Options {
    std::wstring userFilter;
    std::wstring processFilter;
};

static std::wstring ToWide(const char* text) {
    if (!text || !text[0]) {
        return {};
    }
    const int needed = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
    if (needed <= 0) {
        return {};
    }
    std::wstring wide(static_cast<size_t>(needed - 1), L'\0');
    MultiByteToWideChar(CP_ACP, 0, text, -1, &wide[0], needed);
    return wide;
}

static std::string ToUtf8(const std::wstring& wide) {
    if (wide.empty()) {
        return {};
    }
    const int needed = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return {};
    }
    std::string utf8(static_cast<size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &utf8[0], needed, nullptr, nullptr);
    return utf8;
}

static std::wstring Basename(const std::wstring& path) {
    const size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) {
        return path;
    }
    return path.substr(pos + 1);
}

static bool EqualsIgnoreCase(const std::wstring& a, const std::wstring& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (towlower(a[i]) != towlower(b[i])) {
            return false;
        }
    }
    return true;
}

static bool ContainsIgnoreCase(const std::wstring& haystack, const std::wstring& needle) {
    if (needle.empty()) {
        return true;
    }
    for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < needle.size(); ++j) {
            if (towlower(haystack[i + j]) != towlower(needle[j])) {
                match = false;
                break;
            }
        }
        if (match) {
            return true;
        }
    }
    return false;
}

static void PrintUsage(const char* exe) {
    std::cerr
        << "Usage:\n"
        << "  " << exe << " [--user <name>] [--process <image.exe>]\n\n"
        << "  --user <name>      Only processes owned by this account (case-insensitive).\n"
        << "                     Matches user name or DOMAIN\\user (e.g. SYSTEM, Administrator).\n"
        << "  --process <name>   Only processes with this executable name (e.g. notepad.exe).\n"
        << "                     Lists every matching PID.\n\n"
        << "With no options, lists all accessible processes and their loaded modules.\n";
}

static bool ParseArgs(int argc, char** argv, Options& opt) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            return false;
        }
        if (strcmp(argv[i], "--user") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "Missing value after --user\n";
                return false;
            }
            opt.userFilter = ToWide(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--process") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "Missing value after --process\n";
                return false;
            }
            opt.processFilter = Basename(ToWide(argv[++i]));
            continue;
        }
        if (argv[i][0] == '-') {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            return false;
        }
        std::cerr << "Unexpected argument: " << argv[i] << "\n";
        return false;
    }
    return true;
}

static bool MatchProcessName(const wchar_t* exeFile, const std::wstring& filter) {
    if (filter.empty()) {
        return true;
    }
    return EqualsIgnoreCase(exeFile, filter.c_str());
}

static bool MatchUserFilter(const std::wstring& domain, const std::wstring& user,
    const std::wstring& filter) {
    if (filter.empty()) {
        return true;
    }
    const std::wstring qualified = domain.empty() ? user : (domain + L"\\" + user);
    if (EqualsIgnoreCase(user, filter) || EqualsIgnoreCase(qualified, filter)) {
        return true;
    }
    const size_t slash = filter.find(L'\\');
    if (slash != std::wstring::npos) {
        const std::wstring fDomain = filter.substr(0, slash);
        const std::wstring fUser = filter.substr(slash + 1);
        if (EqualsIgnoreCase(domain, fDomain) && EqualsIgnoreCase(user, fUser)) {
            return true;
        }
    }
    return ContainsIgnoreCase(qualified, filter) || ContainsIgnoreCase(user, filter);
}

static bool QueryProcessUser(DWORD pid, std::wstring& outQualified, std::wstring& outUserOnly) {
    outQualified.clear();
    outUserOnly.clear();

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
        return false;
    }

    HANDLE token = nullptr;
    if (!OpenProcessToken(process, TOKEN_QUERY, &token)) {
        CloseHandle(process);
        return false;
    }

    DWORD tokenInfoSize = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &tokenInfoSize);
    if (tokenInfoSize == 0) {
        CloseHandle(token);
        CloseHandle(process);
        return false;
    }

    std::vector<BYTE> tokenInfo(tokenInfoSize);
    if (!GetTokenInformation(token, TokenUser, tokenInfo.data(), tokenInfoSize, &tokenInfoSize)) {
        CloseHandle(token);
        CloseHandle(process);
        return false;
    }

    const auto* tokenUser = reinterpret_cast<const TOKEN_USER*>(tokenInfo.data());
    wchar_t name[256] = {};
    wchar_t domain[256] = {};
    DWORD nameLen = static_cast<DWORD>(std::size(name));
    DWORD domainLen = static_cast<DWORD>(std::size(domain));
    SID_NAME_USE use = SidTypeUnknown;

    if (!LookupAccountSidW(nullptr, tokenUser->User.Sid, name, &nameLen, domain, &domainLen, &use)) {
        CloseHandle(token);
        CloseHandle(process);
        return false;
    }

    outUserOnly = name;
    outQualified = domain[0] ? (std::wstring(domain) + L"\\" + name) : name;

    CloseHandle(token);
    CloseHandle(process);
    return true;
}

struct ModuleEntry {
    std::wstring name;
    std::wstring path;
};

static bool CollectModules(DWORD pid, std::vector<ModuleEntry>& modules) {
    modules.clear();

    const HANDLE snapshot =
        CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Module32FirstW(snapshot, &entry)) {
        CloseHandle(snapshot);
        return false;
    }

    do {
        ModuleEntry mod{};
        mod.name = entry.szModule;
        mod.path = entry.szExePath;
        modules.push_back(std::move(mod));
    } while (Module32NextW(snapshot, &entry));

    CloseHandle(snapshot);
    return !modules.empty();
}

static void LogAccessDenied(DWORD pid, const wchar_t* image, const char* detail) {
    std::cerr << "access denied: PID " << pid << " (" << ToUtf8(image) << ") " << detail << "\n";
}

enum class ExportCountStatus { Ok, NoTable, Unavailable };

static bool ReadPeFileWide(const std::wstring& path, std::vector<BYTE>& fileData) {
    fileData.clear();
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    const DWORD fileSize = GetFileSize(file, nullptr);
    if (fileSize == INVALID_FILE_SIZE || fileSize < sizeof(IMAGE_DOS_HEADER)) {
        CloseHandle(file);
        return false;
    }

    fileData.resize(fileSize);
    DWORD read = 0;
    if (!ReadFile(file, fileData.data(), fileSize, &read, nullptr) || read != fileSize) {
        CloseHandle(file);
        fileData.clear();
        return false;
    }

    CloseHandle(file);
    return true;
}

static bool CountPeExports(const std::vector<BYTE>& fileData, DWORD& exportCount, ExportCountStatus& status) {
    exportCount = 0;
    status = ExportCountStatus::Unavailable;

    if (fileData.size() < sizeof(IMAGE_DOS_HEADER)) {
        return false;
    }

    PVOID imageBase = const_cast<PVOID>(static_cast<const void*>(fileData.data()));
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(imageBase);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }

    if (static_cast<size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS) > fileData.size()) {
        return false;
    }

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(static_cast<const BYTE*>(imageBase) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }

    unsigned long dirSize = 0;
    const auto* expDir = static_cast<const IMAGE_EXPORT_DIRECTORY*>(
        ImageDirectoryEntryToData(imageBase, FALSE, IMAGE_DIRECTORY_ENTRY_EXPORT, &dirSize));
    if (!expDir) {
        status = ExportCountStatus::NoTable;
        exportCount = 0;
        return true;
    }

    exportCount = expDir->NumberOfFunctions;
    status = ExportCountStatus::Ok;
    return true;
}

static ExportCountStatus GetExportCountCached(const std::wstring& path, DWORD& exportCount,
    std::map<std::wstring, std::pair<ExportCountStatus, DWORD>>& cache) {
    const auto found = cache.find(path);
    if (found != cache.end()) {
        exportCount = found->second.second;
        return found->second.first;
    }

    std::vector<BYTE> fileData;
    ExportCountStatus status = ExportCountStatus::Unavailable;
    exportCount = 0;
    if (ReadPeFileWide(path, fileData) && CountPeExports(fileData, exportCount, status)) {
        cache.emplace(path, std::make_pair(status, exportCount));
        return status;
    }

    cache.emplace(path, std::make_pair(ExportCountStatus::Unavailable, 0));
    return ExportCountStatus::Unavailable;
}

static void PrintModuleLine(const ModuleEntry& mod, std::map<std::wstring, std::pair<ExportCountStatus, DWORD>>& cache) {
    if (!mod.path.empty()) {
        DWORD exportCount = 0;
        const ExportCountStatus status = GetExportCountCached(mod.path, exportCount, cache);
        std::cout << "  " << ToUtf8(mod.path) << "  exports: ";
        if (status == ExportCountStatus::Ok) {
            std::cout << exportCount;
        } else if (status == ExportCountStatus::NoTable) {
            std::cout << "0";
        } else {
            std::cout << "unavailable";
        }
        std::cout << "\n";
    } else if (!mod.name.empty()) {
        std::cout << "  " << ToUtf8(mod.name) << " (path unavailable)\n";
    }
}

int main(int argc, char** argv) {
    Options opt{};
    if (!ParseArgs(argc, argv, opt)) {
        PrintUsage(argv[0]);
        return argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) ? 0 : 1;
    }

    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        std::cerr << "CreateToolhelp32Snapshot failed (error " << GetLastError() << ")\n";
        return 1;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(snapshot, &entry)) {
        std::cerr << "Process32First failed (error " << GetLastError() << ")\n";
        CloseHandle(snapshot);
        return 1;
    }

    int printed = 0;
    std::map<std::wstring, std::pair<ExportCountStatus, DWORD>> exportCache;
    do {
        if (entry.th32ProcessID == 0) {
            continue;
        }

        if (!MatchProcessName(entry.szExeFile, opt.processFilter)) {
            continue;
        }

        std::wstring domainUser;
        std::wstring userOnly;
        const bool haveUser = QueryProcessUser(entry.th32ProcessID, domainUser, userOnly);
        if (!haveUser) {
            LogAccessDenied(entry.th32ProcessID, entry.szExeFile, "(user)");
        }

        if (opt.userFilter.empty()) {
            // include
        } else if (!haveUser) {
            continue;
        } else {
            const size_t slash = domainUser.find(L'\\');
            const std::wstring domain = slash != std::wstring::npos ? domainUser.substr(0, slash) : L"";
            const std::wstring user =
                slash != std::wstring::npos ? domainUser.substr(slash + 1) : domainUser;
            if (!MatchUserFilter(domain, user, opt.userFilter)) {
                continue;
            }
        }

        std::cout << "PID " << entry.th32ProcessID << "  " << ToUtf8(entry.szExeFile) << "  User: ";
        if (haveUser) {
            std::cout << ToUtf8(domainUser);
        } else {
            std::cout << "access denied";
        }
        std::cout << "\n";

        std::vector<ModuleEntry> modules;
        if (!CollectModules(entry.th32ProcessID, modules)) {
            LogAccessDenied(entry.th32ProcessID, entry.szExeFile, "(modules)");
        } else {
            for (const auto& mod : modules) {
                PrintModuleLine(mod, exportCache);
            }
        }

        std::cout << "---\n";
        ++printed;
    } while (Process32NextW(snapshot, &entry));

    CloseHandle(snapshot);

    if (printed == 0 && (!opt.processFilter.empty() || !opt.userFilter.empty())) {
        std::cerr << "No matching processes.\n";
        return 2;
    }

    return 0;
}
