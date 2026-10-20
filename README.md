# DLL Proxy Helper

Reads exported symbols from a PE DLL and either prints linker forward pragmas or writes a **compile-ready proxy `.cpp`**.

## Build

```bat
build.bat
```

Output: `bin\helper.exe`

## Usage

```text
helper.exe <source_dll> [-o proxy.cpp] [forward_dll]
```

| Argument | Description |
|----------|-------------|
| `source_dll` | DLL to read exports from |
| `-o proxy.cpp` | Write full proxy source (includes + export pragmas + generic `DllMain`) |
| `forward_dll` | Optional module to forward to (e.g. `apiclientt.dll`). If omitted, forwards use `source_dll` |

### Legacy mode (stdout only)

```bat
helper.exe c:/windows/system32/user32.dll
```

### Generate proxy source

```bat
helper.exe c:/users/neo/downloads/apiclient.dll -o apiclient_proxy.cpp apiclientt.dll
```

### Compile generated proxy

```bat
cl /LD /O2 /EHsc apiclient_proxy.cpp /Fe:apiclient.dll
```

Link an extra payload object if needed:

```bat
cl /LD /O2 /EHsc apiclient_proxy.cpp ..\shared\token_cmd_spawn.cpp /Fe:apiclient.dll
```
