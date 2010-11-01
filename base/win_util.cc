// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win_util.h"

#include <aclapi.h>
#include <shobjidl.h>  // Must be before propkey.
#include <propkey.h>
#include <propvarutil.h>
#include <sddl.h>
#include <shlobj.h>

#include "base/logging.h"
#include "base/win/registry.h"
#include "base/scoped_handle.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/win/windows_version.h"

namespace win_util {

#define SIZEOF_STRUCT_WITH_SPECIFIED_LAST_MEMBER(struct_name, member) \
    offsetof(struct_name, member) + \
    (sizeof static_cast<struct_name*>(NULL)->member)
#define NONCLIENTMETRICS_SIZE_PRE_VISTA \
    SIZEOF_STRUCT_WITH_SPECIFIED_LAST_MEMBER(NONCLIENTMETRICS, lfMessageFont)

void GetNonClientMetrics(NONCLIENTMETRICS* metrics) {
  DCHECK(metrics);

  static const UINT SIZEOF_NONCLIENTMETRICS =
      (base::win::GetVersion() >= base::win::VERSION_VISTA) ?
      sizeof(NONCLIENTMETRICS) : NONCLIENTMETRICS_SIZE_PRE_VISTA;
  metrics->cbSize = SIZEOF_NONCLIENTMETRICS;
  const bool success = !!SystemParametersInfo(SPI_GETNONCLIENTMETRICS,
                                              SIZEOF_NONCLIENTMETRICS, metrics,
                                              0);
  DCHECK(success);
}

bool GetUserSidString(std::wstring* user_sid) {
  // Get the current token.
  HANDLE token = NULL;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
    return false;
  ScopedHandle token_scoped(token);

  DWORD size = sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE;
  scoped_array<BYTE> user_bytes(new BYTE[size]);
  TOKEN_USER* user = reinterpret_cast<TOKEN_USER*>(user_bytes.get());

  if (!::GetTokenInformation(token, TokenUser, user, size, &size))
    return false;

  if (!user->User.Sid)
    return false;

  // Convert the data to a string.
  wchar_t* sid_string;
  if (!::ConvertSidToStringSid(user->User.Sid, &sid_string))
    return false;

  *user_sid = sid_string;

  ::LocalFree(sid_string);

  return true;
}

#pragma warning(push)
#pragma warning(disable:4312 4244)
WNDPROC SetWindowProc(HWND hwnd, WNDPROC proc) {
  // The reason we don't return the SetwindowLongPtr() value is that it returns
  // the orignal window procedure and not the current one. I don't know if it is
  // a bug or an intended feature.
  WNDPROC oldwindow_proc =
      reinterpret_cast<WNDPROC>(GetWindowLongPtr(hwnd, GWLP_WNDPROC));
  SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(proc));
  return oldwindow_proc;
}

void* SetWindowUserData(HWND hwnd, void* user_data) {
  return
      reinterpret_cast<void*>(SetWindowLongPtr(hwnd, GWLP_USERDATA,
          reinterpret_cast<LONG_PTR>(user_data)));
}

void* GetWindowUserData(HWND hwnd) {
  return reinterpret_cast<void*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
}

#pragma warning(pop)

bool IsShiftPressed() {
  return (::GetKeyState(VK_SHIFT) & 0x8000) == 0x8000;
}

bool IsCtrlPressed() {
  return (::GetKeyState(VK_CONTROL) & 0x8000) == 0x8000;
}

bool IsAltPressed() {
  return (::GetKeyState(VK_MENU) & 0x8000) == 0x8000;
}

std::wstring GetClassName(HWND window) {
  // GetClassNameW will return a truncated result (properly null terminated) if
  // the given buffer is not large enough.  So, it is not possible to determine
  // that we got the entire class name if the result is exactly equal to the
  // size of the buffer minus one.
  DWORD buffer_size = MAX_PATH;
  while (true) {
    std::wstring output;
    DWORD size_ret =
        GetClassNameW(window, WriteInto(&output, buffer_size), buffer_size);
    if (size_ret == 0)
      break;
    if (size_ret < (buffer_size - 1)) {
      output.resize(size_ret);
      return output;
    }
    buffer_size *= 2;
  }
  return std::wstring();  // error
}

bool UserAccountControlIsEnabled() {
  base::win::RegKey key(HKEY_LOCAL_MACHINE,
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
      KEY_READ);
  DWORD uac_enabled;
  if (!key.ReadValueDW(L"EnableLUA", &uac_enabled))
    return true;
  // Users can set the EnableLUA value to something arbitrary, like 2, which
  // Vista will treat as UAC enabled, so we make sure it is not set to 0.
  return (uac_enabled != 0);
}

std::wstring FormatMessage(unsigned messageid) {
  wchar_t* string_buffer = NULL;
  unsigned string_length = ::FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS, NULL, messageid, 0,
      reinterpret_cast<wchar_t *>(&string_buffer), 0, NULL);

  std::wstring formatted_string;
  if (string_buffer) {
    formatted_string = string_buffer;
    LocalFree(reinterpret_cast<HLOCAL>(string_buffer));
  } else {
    // The formating failed. simply convert the message value into a string.
    base::SStringPrintf(&formatted_string, L"message number %d", messageid);
  }
  return formatted_string;
}

std::wstring FormatLastWin32Error() {
  return FormatMessage(GetLastError());
}

bool SetAppIdForPropertyStore(IPropertyStore* property_store,
                              const wchar_t* app_id) {
  DCHECK(property_store);

  // App id should be less than 128 chars and contain no space. And recommended
  // format is CompanyName.ProductName[.SubProduct.ProductNumber].
  // See http://msdn.microsoft.com/en-us/library/dd378459%28VS.85%29.aspx
  DCHECK(lstrlen(app_id) < 128 && wcschr(app_id, L' ') == NULL);

  PROPVARIANT property_value;
  if (FAILED(InitPropVariantFromString(app_id, &property_value)))
    return false;

  HRESULT result = property_store->SetValue(PKEY_AppUserModel_ID,
                                            property_value);
  if (S_OK == result)
    result = property_store->Commit();

  PropVariantClear(&property_value);
  return SUCCEEDED(result);
}

static const char16 kAutoRunKeyPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

bool AddCommandToAutoRun(HKEY root_key, const string16& name,
                         const string16& command) {
  base::win::RegKey autorun_key(root_key, kAutoRunKeyPath, KEY_SET_VALUE);
  return autorun_key.WriteValue(name.c_str(), command.c_str());
}

bool RemoveCommandFromAutoRun(HKEY root_key, const string16& name) {
  base::win::RegKey autorun_key(root_key, kAutoRunKeyPath, KEY_SET_VALUE);
  return autorun_key.DeleteValue(name.c_str());
}

}  // namespace win_util

#ifdef _MSC_VER
//
// If the ASSERT below fails, please install Visual Studio 2005 Service Pack 1.
//
extern char VisualStudio2005ServicePack1Detection[10];
COMPILE_ASSERT(sizeof(&VisualStudio2005ServicePack1Detection) == sizeof(void*),
               VS2005SP1Detect);
//
// Chrome requires at least Service Pack 1 for Visual Studio 2005.
//
#endif  // _MSC_VER

#ifndef COPY_FILE_COPY_SYMLINK
#error You must install the Windows 2008 or Vista Software Development Kit and \
set it as your default include path to build this library. You can grab it by \
searching for "download windows sdk 2008" in your favorite web search engine.  \
Also make sure you register the SDK with Visual Studio, by selecting \
"Integrate Windows SDK with Visual Studio 2005" from the Windows SDK \
menu (see Start - All Programs - Microsoft Windows SDK - \
Visual Studio Registration).
#endif
