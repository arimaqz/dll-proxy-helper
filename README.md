# DLL Proxy Helper

Reads exported symbols from a PE DLL and either prints linker forward pragmas or writes a **compile-ready proxy `.cpp`**.

## Build

```bat
build.bat
```

Output: `bin\helper.exe`, `bin\procdll.exe`

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

## Process / module listing (`procdll.exe`)

Lists running processes, loaded module paths, and each module’s PE **export count** (`IMAGE_EXPORT_DIRECTORY.NumberOfFunctions`). Use this to see where a game or app loads a DLL from before building a proxy.

```text
procdll.exe [--user <name>] [--process <image.exe>]
```

| Option | Description |
|--------|-------------|
| `--user <name>` | Only processes for that account (case-insensitive). Matches `USER` or `DOMAIN\USER` (e.g. `SYSTEM`, `Administrator`). |
| `--process <name>` | Only processes with that executable name (e.g. `notepad.exe`). Every matching PID is listed. |

Examples:

```bat
bin\procdll.exe
bin\procdll.exe --user SYSTEM
bin\procdll.exe --process notepad.exe
bin\procdll.exe --user neo --process chrome.exe
```

Processes or modules that cannot be opened are reported on **stderr** as `access denied` (common without an elevated shell). Export counts show `unavailable` when the PE file cannot be read. Matching filters with no results exits with code `2`.
