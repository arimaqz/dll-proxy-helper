# DLL Proxy Helper
DLL Proxy Helper is a tool designed to help with DLL proxying by printing a list of target DLL exported functions.
## Usage
```
helper.exe <DLL path>
```

### Example
```
./helper.exe c:/windows/system32/user32.dll
```

```cpp
#pragma comment(linker, "/export:ActivateKeyboardLayout=c:/windows/system32/user32.dll.ActivateKeyboardLayout")
#pragma comment(linker, "/export:AddClipboardFormatListener=c:/windows/system32/user32.dll.AddClipboardFormatListener")
#pragma comment(linker, "/export:AddVisualIdentifier=c:/windows/system32/user32.dll.AddVisualIdentifier")
#pragma comment(linker, "/export:AdjustWindowRect=c:/windows/system32/user32.dll.AdjustWindowRect")
#pragma comment(linker, "/export:AdjustWindowRectEx=c:/windows/system32/user32.dll.AdjustWindowRectEx")
#pragma comment(linker, "/export:AdjustWindowRectExForDpi=c:/windows/system32/user32.dll.AdjustWindowRectExForDpi")
#pragma comment(linker, "/export:AlignRects=c:/windows/system32/user32.dll.AlignRects")
#pragma comment(linker, "/export:AllowForegroundActivation=c:/windows/system32/user32.dll.AllowForegroundActivation")
#pragma comment(linker, "/export:AllowSetForegroundWindow=c:/windows/system32/user32.dll.AllowSetForegroundWindow")
#pragma comment(linker, "/export:AnimateWindow=c:/windows/system32/user32.dll.AnimateWindow")
#pragma comment(linker, "/export:AnyPopup=c:/windows/system32/user32.dll.AnyPopup")
#pragma comment(linker, "/export:AppendMenuA=c:/windows/system32/user32.dll.AppendMenuA")
#pragma comment(linker, "/export:AppendMenuW=c:/windows/system32/user32.dll.AppendMenuW")
...
```
