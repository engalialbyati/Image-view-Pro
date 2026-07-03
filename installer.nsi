!include "MUI2.nsh"

!define APPNAME "Image Viewer Pro"
!define APPVER "1.0.0"
!define PROGID "ImageViewerPro.Image"
!define PUB "Image Viewer Pro"

Name "${APPNAME}"
OutFile "ImageViewerPro-Setup.exe"
Unicode true
ShowInstDetails show
ShowUnInstDetails show
InstallDir "$PROGRAMFILES64\${APPNAME}"
InstallDirRegKey HKLM "Software\${APPNAME}" "InstallDir"
RequestExecutionLevel admin

VIProductVersion "1.0.0.0"
VIAddVersionKey "ProductName" "${APPNAME}"
VIAddVersionKey "FileDescription" "${APPNAME} Installer"
VIAddVersionKey "FileVersion" "${APPVER}"
VIAddVersionKey "ProductVersion" "${APPVER}"
VIAddVersionKey "CompanyName" "${PUB}"
VIAddVersionKey "LegalCopyright" "${PUB}"

!define MUI_ABORTWARNING
!define MUI_ICON "app.ico"
!define MUI_UNICON "app.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APPNAME}.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch ${APPNAME}"
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

!macro AssocExt EXT
  WriteRegStr HKLM "Software\Classes\.${EXT}\OpenWithProgids" "${PROGID}" ""
!macroend
!macro UnassocExt EXT
  DeleteRegValue HKLM "Software\Classes\.${EXT}\OpenWithProgids" "${PROGID}"
!macroend

Section "Install"
  SectionIn RO
  SetShellVarContext all
  SetOutPath "$INSTDIR"
  File "${APPNAME}.exe"
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  WriteRegStr HKLM "Software\${APPNAME}" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayName" "${APPNAME}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayIcon" '"$INSTDIR\${APPNAME}.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "DisplayVersion" "${APPVER}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "Publisher" "${PUB}"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}" "NoRepair" 1

  WriteRegStr HKLM "Software\Classes\${PROGID}" "" "${APPNAME} Image"
  WriteRegStr HKLM "Software\Classes\${PROGID}\DefaultIcon" "" '"$INSTDIR\${APPNAME}.exe"'
  WriteRegStr HKLM "Software\Classes\${PROGID}\shell\open\command" "" '"$INSTDIR\${APPNAME}.exe" "%1"'

  !insertmacro AssocExt "jpg"
  !insertmacro AssocExt "jpeg"
  !insertmacro AssocExt "jpe"
  !insertmacro AssocExt "jfif"
  !insertmacro AssocExt "png"
  !insertmacro AssocExt "gif"
  !insertmacro AssocExt "bmp"
  !insertmacro AssocExt "dib"
  !insertmacro AssocExt "tif"
  !insertmacro AssocExt "tiff"
  !insertmacro AssocExt "webp"
  !insertmacro AssocExt "svg"
  !insertmacro AssocExt "ico"
  !insertmacro AssocExt "heic"
  !insertmacro AssocExt "heif"
  !insertmacro AssocExt "wmf"
  !insertmacro AssocExt "emf"

  CreateDirectory "$SMPROGRAMS\${APPNAME}"
  CreateShortcut "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk" "$INSTDIR\${APPNAME}.exe"
  CreateShortcut "$SMPROGRAMS\${APPNAME}\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
  CreateShortcut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\${APPNAME}.exe"

  SetAutoClose false
SectionEnd

Section "Uninstall"
  SetShellVarContext all
  Delete "$INSTDIR\${APPNAME}.exe"
  Delete "$INSTDIR\Uninstall.exe"
  Delete "$DESKTOP\${APPNAME}.lnk"
  RMDir /r "$SMPROGRAMS\${APPNAME}"

  DeleteRegKey HKLM "Software\${APPNAME}"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"
  DeleteRegKey HKLM "Software\Classes\${PROGID}"

  !insertmacro UnassocExt "jpg"
  !insertmacro UnassocExt "jpeg"
  !insertmacro UnassocExt "jpe"
  !insertmacro UnassocExt "jfif"
  !insertmacro UnassocExt "png"
  !insertmacro UnassocExt "gif"
  !insertmacro UnassocExt "bmp"
  !insertmacro UnassocExt "dib"
  !insertmacro UnassocExt "tif"
  !insertmacro UnassocExt "tiff"
  !insertmacro UnassocExt "webp"
  !insertmacro UnassocExt "svg"
  !insertmacro UnassocExt "ico"
  !insertmacro UnassocExt "heic"
  !insertmacro UnassocExt "heif"
  !insertmacro UnassocExt "wmf"
  !insertmacro UnassocExt "emf"

  RMDir "$INSTDIR"
  SetAutoClose false
SectionEnd
