#include <Windows.h>
#include <iostream>
#include <imagehlp.h>
#pragma comment(lib,"Imagehlp.lib")
int main(int argc, char** argv) {

    if (argc != 2) {
        std::cout << argv[0] << " <DLL path";
        exit(1);
    }

    HANDLE hFile = CreateFileA(argv[1], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cout << "cannot access file " << argv[1];
        exit(1);
    }
    DWORD dwFileSize = GetFileSize(hFile, NULL);
    LPVOID pFileData = HeapAlloc(GetProcessHeap(), 0, dwFileSize);
    ReadFile(hFile, pFileData, dwFileSize, NULL, NULL);

    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)pFileData;
    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        std::cout << "Invalid DOS signaure";
        exit(1);
    } 

    PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((PBYTE)pFileData + pDosHeader->e_lfanew);
    if (pNtHeader->Signature != IMAGE_NT_SIGNATURE) {
        std::cout << "Invalid NT signature";
        exit(1);
    }

    unsigned long cDirSize;
    _IMAGE_EXPORT_DIRECTORY*  pExpDir = (_IMAGE_EXPORT_DIRECTORY*)ImageDirectoryEntryToData(pFileData, false, IMAGE_DIRECTORY_ENTRY_EXPORT, &cDirSize);
    DWORD* pAddressOfNames = (DWORD*)ImageRvaToVa(pNtHeader, pFileData, pExpDir->AddressOfNames, NULL);
    for (ULONG i = 0; i < pExpDir->NumberOfNames; i++) {
        char* sName = (char*)ImageRvaToVa(pNtHeader, pFileData, pAddressOfNames[i], NULL);
        printf("#pragma comment(linker, \"/export:%s=%s.%s\")\n", sName, argv[1], sName);
    }
}