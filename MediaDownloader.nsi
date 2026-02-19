; MediaDownloader NSIS Installer Script
; This script packages the PyInstaller output (bin/, exe, etc.) into a Windows installer.
; 
; Build Instructions:
; 1. Install NSIS from https://nsis.sourceforge.io/Download
; 2. Run: makensis /V2 MediaDownloader.nsi
; 3. Output: MediaDownloader-Setup-0.0.9.exe will be created in the project root

!include "MUI2.nsh"
!include "x64.nsh"

; ============================================================================
; Installer Configuration
; ============================================================================

Name "MediaDownloader"
OutFile "MediaDownloader-Setup-0.0.9.exe"
InstallDir "$PROGRAMFILES\MediaDownloader"

; Request admin privileges to write to Program Files
RequestExecutionLevel admin

; ============================================================================
; MUI Settings
; ============================================================================

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!define MUI_FINISHPAGE_SHOWREADME_TEXT "Create a desktop shortcut"
!define MUI_FINISHPAGE_SHOWREADME
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION CreateDesktopShortcut
!define MUI_FINISHPAGE_SHOWREADME_CHECKED
!define MUI_FINISHPAGE_RUN "$INSTDIR\\MediaDownloader.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch MediaDownloader"
!define MUI_FINISHPAGE_RUN_CHECKED
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

; ============================================================================
; Installer Section
; ============================================================================

Section "Install"
  SetOutPath "$INSTDIR"
  
  ; Copy the main executable and all bundled dependencies from dist/
  ; Assumes the build system has prepared dist/MediaDownloader/ with:
  ; - MediaDownloader.exe
  ; - bin/ (containing yt-dlp, ffmpeg, ffprobe for all platforms)
  ; - Any other runtime files
  
  ; Recursively copy the PyInstaller onedir output
  File /r "dist\MediaDownloader\*.*"
  
  ; Create Start Menu shortcuts
  CreateDirectory "$SMPROGRAMS\MediaDownloader"
  CreateShortCut "$SMPROGRAMS\MediaDownloader\MediaDownloader.lnk" "$INSTDIR\MediaDownloader.exe"
  CreateShortCut "$SMPROGRAMS\MediaDownloader\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
  
  ; Write uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  
  ; Registry entries for Add/Remove Programs
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MediaDownloader" \
    "DisplayName" "MediaDownloader"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MediaDownloader" \
    "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MediaDownloader" \
    "DisplayIcon" "$INSTDIR\MediaDownloader.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MediaDownloader" \
    "DisplayVersion" "0.0.9"
  
  ; Log completion
  DetailPrint "MediaDownloader installed successfully"
SectionEnd

; ============================================================================
; Uninstaller Section
; ============================================================================

Section "Uninstall"
  ; Remove Start Menu shortcut
  RMDir /r "$SMPROGRAMS\MediaDownloader"
  
  ; Remove Desktop shortcut
  Delete "$DESKTOP\MediaDownloader.lnk"
  
  ; Remove registry entry
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MediaDownloader"
  
  ; Remove application files
  RMDir /r "$INSTDIR"
SectionEnd

; ============================================================================
; Function: .onInstSuccess
; Handles post-install actions (e.g., launch app after install)
; ============================================================================

Function .onInstSuccess
  ; Optional: Launch the app after successful install
  ; Uncomment the following lines if desired:
  ; SetOutPath "$INSTDIR"
  ; Exec "$INSTDIR\MediaDownloader.exe"
FunctionEnd

Function CreateDesktopShortcut
  CreateShortCut "$DESKTOP\MediaDownloader.lnk" "$INSTDIR\MediaDownloader.exe"
FunctionEnd
