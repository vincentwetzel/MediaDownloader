!include "MUI2.nsh"

; General Settings
Name "MediaDownloader"
OutFile "MediaDownloader_Installer.exe"
InstallDir "$PROGRAMFILES64\MediaDownloader"
InstallDirRegKey HKLM "Software\MediaDownloader" "Install_Dir"
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

Section "MediaDownloader" SecMain
    SetOutPath "$INSTDIR"

    ; --- SEAMLESS UPDATE CLEANUP ---
    ; Clean up old Python (PyInstaller) artifacts so they don't waste space
    RMDir /r "$INSTDIR\_internal"
    Delete "$INSTDIR\MediaDownloader.exe"
    
    ; Assuming CMake places all deployment-ready files (exe, DLLs, bin/) in a folder named 'deploy'
    ; Change 'deploy\*.*' to your actual packaging output directory
    File /r "deploy\*.*"

    ; Write the installation path into the registry
    WriteRegStr HKLM "Software\MediaDownloader" "Install_Dir" "$INSTDIR"

    ; Write the uninstall keys for Windows Add/Remove Programs
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MediaDownloader" "DisplayName" "MediaDownloader"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MediaDownloader" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MediaDownloader" "DisplayIcon" '"$INSTDIR\MediaDownloader.exe"'
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MediaDownloader" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MediaDownloader" "NoRepair" 1
    WriteUninstaller "$INSTDIR\uninstall.exe"

    ; Create shortcuts
    CreateDirectory "$SMPROGRAMS\MediaDownloader"
    CreateShortcut "$SMPROGRAMS\MediaDownloader\MediaDownloader.lnk" "$INSTDIR\MediaDownloader.exe"
    CreateShortcut "$DESKTOP\MediaDownloader.lnk" "$INSTDIR\MediaDownloader.exe"
SectionEnd

Section "Uninstall"
    ; Remove registry keys
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MediaDownloader"
    DeleteRegKey HKLM "Software\MediaDownloader"

    ; Remove all files and directories
    RMDir /r "$INSTDIR"
    RMDir /r "$SMPROGRAMS\MediaDownloader"
    Delete "$DESKTOP\MediaDownloader.lnk"
SectionEnd