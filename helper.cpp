#include <Windows.h>
#include <imagehlp.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#pragma comment(lib, "Imagehlp.lib")

struct Options {
    const char* sourceDll = nullptr;
    const char* outputCpp = nullptr;
    const char* forwardDll = nullptr;
};

static void PrintUsage(const char* exe) {
    std::cerr
        << "Usage:\n"
        << "  " << exe << " <source_dll> [-o proxy.cpp] [forward_dll]\n\n"
        << "  source_dll   PE file to read exports from.\n"
        << "  -o proxy.cpp Write a compile-ready proxy .cpp (includes DllMain).\n"
        << "  forward_dll  Optional forward target (e.g. apiclientt.dll). If omitted,\n"
        << "               forwards use source_dll as today: source_dll.ExportName\n\n"
        << "Legacy (no -o): print #pragma export lines to stdout only.\n";
}

static bool ParseArgs(int argc, char** argv, Options& opt) {
    if (argc < 2) {
        return false;
    }

    opt.sourceDll = argv[1];

    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "Missing path after " << argv[i] << "\n";
                return false;
            }
            opt.outputCpp = argv[++i];
        } else if (argv[i][0] == '-') {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            return false;
        } else if (!opt.forwardDll) {
            opt.forwardDll = argv[i];
        } else {
            std::cerr << "Unexpected argument: " << argv[i] << "\n";
            return false;
        }
    }

    return true;
}

static bool ReadPeFile(const char* path, std::vector<BYTE>& fileData) {
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        std::cerr << "Cannot access file: " << path << " (error " << GetLastError() << ")\n";
        return false;
    }

    const DWORD fileSize = GetFileSize(file, nullptr);
    if (fileSize == INVALID_FILE_SIZE || fileSize == 0) {
        std::cerr << "Invalid file size: " << path << "\n";
        CloseHandle(file);
        return false;
    }

    fileData.resize(fileSize);
    DWORD read = 0;
    if (!ReadFile(file, fileData.data(), fileSize, &read, nullptr) || read != fileSize) {
        std::cerr << "Failed to read file: " << path << "\n";
        CloseHandle(file);
        return false;
    }

    CloseHandle(file);
    return true;
}

static bool CollectExportNames(const std::vector<BYTE>& fileData, std::vector<std::string>& names) {
    PVOID imageBase = const_cast<PVOID>(static_cast<const void*>(fileData.data()));

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(imageBase);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        std::cerr << "Invalid DOS signature\n";
        return false;
    }

    auto* nt = reinterpret_cast<PIMAGE_NT_HEADERS>(static_cast<PBYTE>(imageBase) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        std::cerr << "Invalid NT signature\n";
        return false;
    }

    unsigned long dirSize = 0;
    const auto* expDir = static_cast<const IMAGE_EXPORT_DIRECTORY*>(
        ImageDirectoryEntryToData(imageBase, FALSE, IMAGE_DIRECTORY_ENTRY_EXPORT, &dirSize));
    if (!expDir || expDir->NumberOfNames == 0) {
        std::cerr << "No named exports found\n";
        return false;
    }

    const DWORD* nameRvas = static_cast<const DWORD*>(ImageRvaToVa(nt, imageBase, expDir->AddressOfNames, nullptr));
    if (!nameRvas) {
        std::cerr << "Failed to resolve export names\n";
        return false;
    }

    for (ULONG i = 0; i < expDir->NumberOfNames; ++i) {
        const char* exportName = static_cast<const char*>(ImageRvaToVa(nt, imageBase, nameRvas[i], nullptr));
        if (exportName && exportName[0] != '\0') {
            names.emplace_back(exportName);
        }
    }

    if (names.empty()) {
        std::cerr << "Export directory contained no usable names\n";
        return false;
    }

    return true;
}

static void PrintPragmaLine(const std::string& exportName, const char* forwardModule) {
    std::printf("#pragma comment(linker, \"/export:%s=%s.%s\")\n", exportName.c_str(), forwardModule,
        exportName.c_str());
}

static bool WriteProxyCpp(const char* outputPath, const char* sourceDll, const char* forwardModule,
    const std::vector<std::string>& names) {
    std::ofstream out(outputPath, std::ios::binary);
    if (!out) {
        std::cerr << "Cannot open output file: " << outputPath << "\n";
        return false;
    }

    out << "// Auto-generated DLL proxy source\n";
    out << "// Source exports: " << sourceDll << "\n";
    out << "// Forward target:  " << forwardModule << "\n";
    out << "// Build (example): cl /LD /O2 /EHsc " << outputPath << " /Fe:YourProxy.dll\n\n";
    out << "#include <windows.h>\n\n";

    for (const auto& name : names) {
        out << "#pragma comment(linker, \"/export:" << name << "=" << forwardModule << "." << name << "\")\n";
    }

    out << "\nBOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {\n";
    out << "    (void)reserved;\n";
    out << "    if (reason == DLL_PROCESS_ATTACH) {\n";
    out << "        DisableThreadLibraryCalls(module);\n";
    out << "    }\n";
    out << "    return TRUE;\n";
    out << "}\n";

    if (!out.good()) {
        std::cerr << "Failed while writing: " << outputPath << "\n";
        return false;
    }

    return true;
}

int main(int argc, char** argv) {
    Options opt{};
    if (!ParseArgs(argc, argv, opt)) {
        PrintUsage(argv[0]);
        return 1;
    }

    std::vector<BYTE> fileData;
    if (!ReadPeFile(opt.sourceDll, fileData)) {
        return 1;
    }

    std::vector<std::string> exportNames;
    if (!CollectExportNames(fileData, exportNames)) {
        return 1;
    }

    const char* forwardModule = opt.forwardDll ? opt.forwardDll : opt.sourceDll;

    if (opt.outputCpp) {
        if (!WriteProxyCpp(opt.outputCpp, opt.sourceDll, forwardModule, exportNames)) {
            return 1;
        }
        std::cout << "Wrote " << exportNames.size() << " export forwards to " << opt.outputCpp << "\n";
        return 0;
    }

    for (const auto& name : exportNames) {
        PrintPragmaLine(name, forwardModule);
    }

    return 0;
}
