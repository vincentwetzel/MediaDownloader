!include "MUI2.nsh"

; General Settings
Name "LzyDownloader"
OutFile "LzyDownloader_Installer.exe"
InstallDir "$PROGRAMFILES64\LzyDownloader"
InstallDirRegKey HKLM "Software\LzyDownloader" "Install_Dir"
RequestExecutionLevel admin

; UI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "src\ui\assets\icon.ico" ; Make sure this path points to your actual icon
!define MUI_UNICON "src\ui\assets\icon.ico"

; Pages
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Language
!insertmacro MUI_LANGUAGE "English"

Section "LzyDownloader" SecMain
    SetOutPath "$INSTDIR"

    ; --- SEAMLESS UPDATE CLEANUP ---
    ; Clean up old Python (PyInstaller) artifacts so they don't waste space
    RMDir /r "$INSTDIR\_internal"
    Delete "$INSTDIR\LzyDownloader.exe"

    ; Assuming CMake places all deployment-ready files (exe, DLLs, bin/) in a folder named 'deploy'
    ; Change 'deploy\*.*' to your actual packaging output directory
    File /r "deploy\*.*"

    ; Write the installation path into the registry
    WriteRegStr HKLM "Software\LzyDownloader" "Install_Dir" "$INSTDIR"

    ; Write the uninstall keys for Windows Add/Remove Programs
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LzyDownloader" "DisplayName" "LzyDownloader"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LzyDownloader" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LzyDownloader" "DisplayIcon" '"$INSTDIR\LzyDownloader.exe"'
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LzyDownloader" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LzyDownloader" "NoRepair" 1
    WriteUninstaller "$INSTDIR\uninstall.exe"

    ; Create shortcuts
    CreateDirectory "$SMPROGRAMS\LzyDownloader"
    CreateShortcut "$SMPROGRAMS\LzyDownloader\LzyDownloader.lnk" "$INSTDIR\LzyDownloader.exe"
    CreateShortcut "$DESKTOP\LzyDownloader.lnk" "$INSTDIR\LzyDownloader.exe"
SectionEnd

Section "Uninstall"
    ; Remove registry keys
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LzyDownloader"
    DeleteRegKey HKLM "Software\LzyDownloader"

    ; Remove all files and directories
    RMDir /r "$INSTDIR"
    RMDir /r "$SMPROGRAMS\LzyDownloader"
    Delete "$DESKTOP\LzyDownloader.lnk"
SectionEnd