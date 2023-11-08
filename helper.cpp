#include <Windows.h>
#include <iostream>

int main(int argc, char** argv){
    if(argc != 2){
        std::cout << argv[0] << " <DLL path";
        exit(1);
    }
    HANDLE hFile = CreateFileA(argv[1], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD dwFileSize = GetFileSize(hFile, NULL);
    LPVOID pFileData = HeapAlloc(GetProcessHeap(), 0 , dwFileSize);
    ReadFile(hFile, pFileData, dwFileSize, NULL, NULL);

    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)pFileData;
    if(pDosHeader->e_magic != IMAGE_DOS_SIGNATURE){
        std::cout << "Invalid DOS signaure";
        exit(1);
    }
    PIMAGE_NT_HEADERS pNtHeader = (PIMAGE_NT_HEADERS)((PBYTE)pFileData + pDosHeader->e_lfanew);
    if(pNtHeader->Signature != IMAGE_NT_SIGNATURE){
        std::cout << "Invalid NT signature";
        exit(1);
    }

    PIMAGE_EXPORT_DIRECTORY pExportDirectory = (PIMAGE_EXPORT_DIRECTORY)((PBYTE)pFileData + pNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
    PDWORD pFunctionNameArray = (PDWORD)((PBYTE)pFileData + pExportDirectory->AddressOfNames);
    for(int i =0; i< pExportDirectory->NumberOfFunctions; i++){
        char* functionName = (char*)((PBYTE)pFileData + pFunctionNameArray[i]);
        printf("#pragma comment(linker, \"/export:%s=%s.%s\")\n", functionName,argv[1],functionName);
    }
}