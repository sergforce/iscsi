;NSIS Modern User Interface version 1.70
;Welcome/Finish Page Example Script
;Written by Joost Verburg

;--------------------------------
;Include Modern UI

  !include "MUI.nsh"

;--------------------------------
;General
	SetCompressor lzma
	OutFile "setup.exe"
	
	XPStyle on

  ;Name and file
  Name "iSCSITarget"
	
	InstType "Full"
	
	  ;Default installation folder
  InstallDir "$PROGRAMFILES\iSCSITarget"
  
  ;Get installation folder from registry if available
  InstallDirRegKey HKLM "SOFTWARE\iSCSI Target" ""
	
;---------------------------------
; Vars

  Var STARTMENU_FOLDER
	
;--------------------------------
;Interface Settings

  !define MUI_ABORTWARNING
  

;--------------------------------
;Pages
	!define MUI_COMPONENTSPAGE_SMALLDESC
	
  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_LICENSE "License.rtf"
	
;  !insertmacro MUI_PAGE_COMPONENTS
	!insertmacro MUI_PAGE_DIRECTORY
	

  ;Start Menu Folder Page Configuration
  !define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKLM" 
  !define MUI_STARTMENUPAGE_REGISTRY_KEY "SOFTWARE\iSCSI Target" 
  !define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"
  
  !insertmacro MUI_PAGE_STARTMENU Application $STARTMENU_FOLDER
  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_PAGE_FINISH
  
	
  !insertmacro MUI_UNPAGE_WELCOME
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES
  !insertmacro MUI_UNPAGE_FINISH



;--------------------------------
;Languages
 
  !insertmacro MUI_LANGUAGE "English"
	
;--------------------------------

 !insertmacro MUI_RESERVEFILE_INSTALLOPTIONS
 
 Section "Programs" SecPrograms
	SectionIn 1 RO
	
	SetOutPath "$INSTDIR"
	
	File  "daemon.exe"
	File  "init.exe"
 
	;Store installation folder
  WriteRegStr HKLM "SOFTWARE\iSCSI Target" "" $INSTDIR
  ;Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"
	
	;Store service path
  WriteRegStr HKLM "SOFTWARE\iSCSI Target" "ServiceFullPath" "$INSTDIR\daemon.exe"
	
	WriteRegStr HKLM "SOFTWARE\iSCSI Target" "Targets" ""
	
	;WriteRegStr HKLM "SOFTWARE\iSCSI Target\Targets" "" ""

  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application    
    ;Create shortcuts
    CreateDirectory "$SMPROGRAMS\$STARTMENU_FOLDER"
		CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\iSCSI Target configuration utility.lnk" "$INSTDIR\init.exe" "" "mmc.exe" 1
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
	!insertmacro MUI_STARTMENU_WRITE_END
	
	ExecWait '"$INSTDIR\init.exe" --installservice' $1
	StrCmp $1 "-1" 0 +2
		MessageBox MB_ICONSTOP|MB_OK "Can't install service setting!"
	
	
SectionEnd	

	

 
;Section "Finish"
;  
;
;SectionEnd

;--------------------------------
;Uninstaller Section

Section "Uninstall"

  ;ADD YOUR OWN FILES HERE...
	ExecWait '"$INSTDIR\init.exe" --deleteservice' $1
	StrCmp $1 "-1" 0 +2
		MessageBox MB_ICONSTOP|MB_OK "Can't uninstall service!"

  RMDir /r "$INSTDIR"

!insertmacro MUI_STARTMENU_GETFOLDER Application $STARTMENU_FOLDER
 
  RMDir /r "$SMPROGRAMS\$STARTMENU_FOLDER"

  DeleteRegKey HKLM "SOFTWARE\iSCSI Target"
	
	

SectionEnd
