#include "highlightData.h"
#include "preferences.h"
#include "search.h"
#include "smartIndent.h"
#include "window.h"
#include "windowTitle.h"
#include "userCmds.h"
#include "menu.h"
#include "../util/misc.h"
#include "../util/DialogF.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* Motif stuff */
#include <Xm/Xm.h>
#include <Xm/AtomMgr.h>
#include <Xm/CutPaste.h>
#include <Xm/DrawingA.h>
#include <Xm/MainW.h>
#include <Xm/PanedW.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/List.h>
#include <Xm/Frame.h>
#include <Xm/Text.h>
#include <Xm/MessageB.h>
#include <Xm/RowColumn.h>	/* XmCreateWorkArea() */
#include <Xm/SelectioB.h>
#include <Xm/DialogS.h>
#include <Xm/Form.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>
#include <Xm/PushB.h>
#include <Xm/ArrowB.h>
#include <X11/Shell.h>
#include <Xm/CascadeB.h>
#include <Xm/Scale.h>
#include <Xm/Separator.h>
#include <X11/Shell.h>
#include <X11/cursorfont.h>

#if XmVersion >= 1002
#include <Xm/Display.h>
#include <Xm/DragDrop.h>
#include <Xm/RepType.h>		/* XmRepTypeInstallTearOffModelConverter() */
#endif

#if HAVE_X11_XMU_EDITRES_H
#include <X11/Xmu/Editres.h>
#endif

typedef int bool;
#define false  0
#define true   1

/* Main types */
typedef int MMType;

#define MMPush          0 /* Create PushButton (default) */
#define MMToggle        1 /* Create ToggleButton */
#define MMMenu          2 /* Create CascadeButton with menu */
#define MMSeparator     3 /* Create Separator */
#define MMLabel         4 /* Create Label */
#define MMRadioMenu     5 /* Create CascadeButton with RadioBox menu */
#define MMOptionMenu    6 /* Create an option menu */
#define MMPanel         7 /* Create a panel */
#define MMRadioPanel    8 /* Create a panel with RadioBox menu */
#define MMButtonPanel   9 /* Like MMRadioPanel, but no radio behavior */
#define MMScale        10 /* Create a scale */
#define MMTextField    11 /* Create a text field */
#define MMEnterField   12 /* Like MMTextField, but use Enter to activate */
#define MMFlatPush     13 /* Create `flat' PushButton without shadows */
#define MMArrow        14 /* Create an arrow button */
#define MMSpinBox      15 /* Like MMTextField, but add two spin buttons */
#define MMComboBox     16 /* Create a combo box */

#define MMTypeMask     31 /* mask to find type */


/* Special attributes, to be ORed with types */

typedef int MMAttr;

#define MMHelp              32 /* Mark as help button */
#define MMInsensitive       64 /* Make item insensitive */
#define MMUnmanaged        128 /* Make item unmanaged */
#define MMUnmanagedLabel   256 /* Don't manage label (in panels) */
#define MMIgnore           512 /* Don't create item */
#define MMVertical        1024 /* Make panel vertical */
#define MMAttrMask ~MMTypeMask /* ~MMTypeMask; */

/* Conveniences */
#define MMNoCB { 0, 0 }
#define MMEnd  { 0, MMPush, MMNoCB, 0, 0, 0, 0 }
#define MMSep  { "separator", MMSeparator, MMNoCB, 0, 0, 0, 0 }

/* New resources */
#define XtNpushMenuPopupTime  "pushMenuPopupTime"
#define XtCPushMenuPopupTime  "PushMenuPopupTime"

typedef struct _PushMenuInfo {
    Widget widget;		/* The PushButton */
    Widget subMenu;		/* Submenu of this PushButton */
    bool flat;			/* Whether the PushButton is flattened */
    XtIntervalId timer;		/* Timer while waiting */
} PushMenuInfo;


typedef struct _MMDesc {
    const char *name;		 /* Widget name */
    MMType type;		 /* Widget type */
    XtCallbackRec callback;	 /* Associated callback */
    struct _MMDesc *items;  	 /* Submenus (NULL if none) */
    Widget *widgetptr;           /* Where to store the resulting widget */
    Widget widget;		 /* The resulting widget */
    Widget label;		 /* The resulting label */
    char *labelString;		 /* label string for widget */
} MMDesc;

typedef struct _ComboBoxInfo{
    Widget top;			/* The top-level window */
    Widget text;		/* The text to be updated */
    Widget button;		/* The arrow button */
    Widget list;		/* The list to select from */
    Widget shell;		/* The shell that contains the list */
    XtIntervalId timer;		/* The timer that controls popup time */
    bool popped_up;		/* True iff the combo box is popped up */
} ComboBoxInfo;

/* Procs */
typedef void (*MMItemProc)(MMDesc items[], XtPointer closure);

void MMaddCallbacks(MMDesc items[], XtPointer default_closure, int depth);
void MMaddItems(Widget shell, MMDesc items[], bool ignore_seps);
void MMonItems(MMDesc items[], MMItemProc proc, XtPointer closure, int depth);
Widget MMcreatePushMenu(Widget parent, String name, MMDesc items[],
	ArgList _args, Cardinal _arg);
Widget MMcreatePanel(Widget parent, const _XtString name, MMDesc items[],
	ArgList args, Cardinal arg);
Widget MMcreateRadioPanel(Widget parent, const _XtString name, MMDesc items[],
	ArgList _args, Cardinal _arg);
Widget MMcreateButtonPanel(Widget parent, const _XtString name, MMDesc items[],
	ArgList args, Cardinal arg);
Widget MMcreatePulldownMenu(Widget parent, String name, MMDesc items[],
	ArgList args, Cardinal arg);
Widget MMcreateRadioPulldownMenu(Widget parent, String name, MMDesc items[],
	ArgList _args, Cardinal _arg);

static void callerDestroyCB(Widget w, XtPointer client_data, XtPointer call_data);
static void modWarnDefCB(Widget w, WindowInfo *window, caddr_t callData);
static void set_sensitive(Widget w, bool state);

static Widget preferences_dialog;
static Widget apply_button;
static Widget save_button;
static Widget current_panel;
static int indentStyle, wrapStyle, matchingStyle, searchStyle;
static int windowSize;
static WindowInfo *CallerWindow;
static int PanelSelectorType = 1;

int lesstif_version = 90;   /* assume lesstif 0.90 */

/* Miscellaneous utilities */

int min(int a, int b)
{
#if defined(__GNUG__) && !defined(__STRICT_ANSI__)
    return a <? b;
#else
    return a < b ? a : b;
#endif
}

int max(int a, int b)
{
#if defined(__GNUG__) && !defined(__STRICT_ANSI__)
    return a >? b;
#else
    return a > b ? a : b;
#endif
}

void widget_creation_error()
{
    fprintf(stderr,"fatal: widget creation failed\n");
    abort();
}

Widget verify(Widget w)
{
    if (w == 0)
	widget_creation_error();
    return w;
}

static char pushMenuTranslations[] = 
	"<Expose>:          decorate-push-menu()\n";

static char lesstif_pushMenuTranslations[] = 
    	"None<Btn3Down>:	popup-push-menu()\n";

static void dummyCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    set_sensitive(apply_button, True);
}

static void prefIndentStyleCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    indentStyle = (int) client_data;
    set_sensitive(apply_button, True);
}

static void prefWrapStyleCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    wrapStyle = (int) client_data;
    set_sensitive(apply_button, True);
}

static void prefMatchingStyleCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    matchingStyle = (int) client_data;
    set_sensitive(apply_button, True);
}

static void prefSearchStyleCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    searchStyle = (int) client_data;
    set_sensitive(apply_button, True);
}

static void highlightingDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    EditHighlightPatterns(CallerWindow);
}

static void smartMacrosDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    EditSmartIndentMacros(CallerWindow);
}

static void stylesDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    EditHighlightStyles(NULL);
}

static void languageDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    EditLanguageModes();
}

#ifndef VMS
static void shellDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    EditShellMenu(CallerWindow);
}
#endif /* VMS */

static void macroDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    EditMacroMenu(CallerWindow);
}

static void bgMenuDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    EditBGMenu(CallerWindow);
}

static void customizeTitleDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    EditCustomTitleFormat(window);
}

static void fontDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    ChooseFonts(CallerWindow, False);
}

#ifdef REPLACE_SCOPE
static int replaceStyle;
void prefReplaceStyleCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    replaceStyle = (int) client_data;
    set_sensitive(apply_button, True);
}
#endif

static Widget winSizeCust_w;
void prefWindowSizeCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    int isCustom = !strcmp(XtName(w), "winCustom");

    /* enable size input box if using custom size */
    set_sensitive(winSizeCust_w, isCustom);        
    windowSize = (int) client_data;
    
    set_sensitive(apply_button, True);
}

static Widget indent_style_w[3];
static MMDesc indent_style_menu [] = 
{
    { "noIndent", MMToggle, { prefIndentStyleCB, (XtPointer)NO_AUTO_INDENT }, 
      NULL, &indent_style_w[0], 0, 0, "Off"},
    { "autoIndent", MMToggle, { prefIndentStyleCB, (XtPointer)AUTO_INDENT },
      NULL, &indent_style_w[1], 0, 0, "On" },
    { "smartIndent", MMToggle, { prefIndentStyleCB, (XtPointer)SMART_INDENT },
      NULL, &indent_style_w[2], 0, 0, "Smart" },
    MMEnd
};

static Widget WrapText, WrapTextLabel, WrapWindowToggle;
static Widget wrap_style_w[3];
static MMDesc wrap_style_menu [] = 
{
    { "noWrap", MMToggle, { prefWrapStyleCB, (XtPointer)NO_WRAP }, 
      NULL, &wrap_style_w[0], 0, 0, "None"},
    { "autoNewLine", MMToggle, { prefWrapStyleCB, (XtPointer)NEWLINE_WRAP },
      NULL, &wrap_style_w[1], 0, 0, "Auto Newline" },
    { "contWrap", MMToggle,  { prefWrapStyleCB, (XtPointer)CONTINUOUS_WRAP },
      NULL, &wrap_style_w[2], 0, 0, "Continuous" },
    MMEnd
};

static MMDesc wrap_menu [] = 
{
    { "wrapStyle",  MMRadioPanel | MMUnmanagedLabel,  MMNoCB, 
      wrap_style_menu, 0, 0, 0, "Wrap" },
    MMEnd
};

static void wrapMarginToggleCB(Widget w, XtPointer clientData, 
	XtPointer callData)
{
    int state = XmToggleButtonGetState(w);
    
    set_sensitive(WrapText, !state);
    set_sensitive(WrapTextLabel, !state);
    set_sensitive(apply_button, True);
}

static MMDesc wrap_margin_menu [] = 
{
    { "wrapWindow",  MMToggle, { (XtCallbackProc) wrapMarginToggleCB, 0 },
      NULL, &WrapWindowToggle, 0, 0, "Wrap and Fill at width of window" },
    { "wrapText", MMTextField, { (XtCallbackProc) dummyCB, 0 }, 
      NULL, &WrapText, 0, 0, " Margin For Wrap and Fill:" },
    MMEnd
};

static Widget tag_collision_w[2];
static MMDesc tag_collision_menu [] = 
{
    { "showAll", MMToggle, { (XtCallbackProc)dummyCB, 0 }, 
      NULL, &tag_collision_w[0], 0, 0, "Show All"},
    { "smart", MMToggle, { (XtCallbackProc)dummyCB, 0 },
      NULL, &tag_collision_w[1], 0, 0, "Smart" },
    MMEnd
};

static Widget backlighting_w;
static Widget backlightCharTypes_w;
static MMDesc backlighting_menu [] = 
{
    { "apply", MMToggle, { (XtCallbackProc)dummyCB, 0 }, 
      NULL, &backlighting_w, 0, 0, "Apply"},
    { "backlightCharType", MMTextField | MMUnmanagedLabel, { dummyCB, 0 }, 
      NULL, &backlightCharTypes_w, 0, 0 },
    MMEnd
};

static Widget highlighting_w[2];
static MMDesc highlighting_menu [] = 
{
    { "off", MMToggle, { (XtCallbackProc)dummyCB, 0 }, 
      NULL, &highlighting_w[0], 0, 0, "Off"},
    { "on", MMToggle, { (XtCallbackProc)dummyCB, 0 }, 
      NULL, &highlighting_w[1], 0, 0, "On"},
    MMEnd
};

static Widget matching_w[3];
static MMDesc matching_style_menu [] = 
{
    { "off", MMToggle, { prefMatchingStyleCB, (XtPointer)NO_FLASH }, 
      NULL, &matching_w[0], 0, 0, "Off"},
    { "delim", MMToggle, { prefMatchingStyleCB, (XtPointer)FLASH_DELIMIT }, 
      NULL, &matching_w[1], 0, 0, "Delimiter"},
    { "range", MMToggle, { prefMatchingStyleCB, (XtPointer)FLASH_RANGE }, 
      NULL, &matching_w[2], 0, 0, "Range"},
    MMEnd
};

static Widget syntax_matching_w;
static MMDesc matching_menu [] = 
{
    { "matching",   MMRadioPanel| MMUnmanagedLabel,  MMNoCB, 
      matching_style_menu, 0, 0, 0 },
    { "syntaxBase", MMToggle, { (XtCallbackProc)dummyCB, 0 }, 
      NULL, &syntax_matching_w, 0, 0, "Syntax Base"},
    MMEnd
};

void checkCustomWinSizeCB(Widget w, XtPointer p1, XtPointer p2)
{
    int val;
    
    GetIntTextWarn(w, &val, XtName(w), True);
    set_sensitive(apply_button, True);
}

static Widget window_cust_row;
static Widget window_cust_col;
static MMDesc window_cust_size_menu [] = 
{
    { "custRow", MMTextField | MMUnmanagedLabel, { checkCustomWinSizeCB, 0 }, 
      NULL, &window_cust_row, 0, 0 },
    { "x", MMLabel, MMNoCB, NULL, 0, 0, 0 },
    { "custCol", MMTextField | MMUnmanagedLabel, { checkCustomWinSizeCB, 0 }, 
      NULL, &window_cust_col, 0, 0 },
    MMEnd
};

static Widget window_size_w[5];
static MMDesc window_size_opt_menu [] = 
{
    { "winCustom",MMPush, { (XtCallbackProc) prefWindowSizeCB, (XtPointer)0 },
    	0, &window_size_w[0], 0,0, "Custom"},
    { "w24x80",   MMPush, { (XtCallbackProc) prefWindowSizeCB, (XtPointer)1 },
    	0, &window_size_w[1], 0,0, "24 x 80"},
    { "w40x80",   MMPush, { (XtCallbackProc) prefWindowSizeCB, (XtPointer)2 },
    	0, &window_size_w[2], 0,0, "40 x 80"},
    { "w60x80",   MMPush, { (XtCallbackProc) prefWindowSizeCB, (XtPointer)3 },
    	0, &window_size_w[3], 0,0, "60 x 80"},
    { "w80x80",   MMPush, { (XtCallbackProc) prefWindowSizeCB, (XtPointer)4 },
    	0, &window_size_w[4], 0,0, "80 x 80"},
    MMEnd
};

static MMDesc window_size_menu [] = 
{
    { "winSizeOpt",  MMOptionMenu | MMUnmanagedLabel,  MMNoCB,
      window_size_opt_menu, 0, 0, 0,},
    { "winSizeCust", MMPanel,  MMNoCB,
      window_cust_size_menu, &winSizeCust_w, 0, 0, "custom:" },
    MMEnd
};

/* Description data structure */
static Widget show_linenum_w;
static Widget show_statsline_w;
static Widget show_isearchline_w;
static Widget incremental_backup_w;
static Widget make_backup_copy_w;
static Widget append_linefeed_w;
static Widget sort_open_prev_w;
static Widget popup_under_ptr_w;
static Widget show_path_w;
static MMDesc general_preferences_menu[] = 
{
/*     { "buttonHints",         MMButtonPanel, MMNoCB, button_menu, 0, 0, 0 },*/
    { "lineNumber",    MMToggle, { (XtCallbackProc) dummyCB, 0 },
      NULL, &show_linenum_w, 0, 0, "Show Line Numbers" },
    { "statisticLine", MMToggle, { (XtCallbackProc) dummyCB, 0 },
      NULL, &show_statsline_w, 0, 0, "Show Statistic Line" },
    { "iSearchLine",   MMToggle, { (XtCallbackProc) dummyCB, 0 },
      NULL, &show_isearchline_w, 0, 0, "Show Incremental Search Line" },
    { "autoSave",      MMToggle, { (XtCallbackProc) dummyCB, 0 },
      NULL, &incremental_backup_w, 0, 0, "AutoSave (Incremental Backup)" },
    { "makeBackup",    MMToggle, { (XtCallbackProc) dummyCB, 0 },
      NULL, &make_backup_copy_w, 0, 0, "Make Backup Copy (*.bck)" },
    { "sortPrev",      MMToggle, { (XtCallbackProc) dummyCB, 0 },
      NULL, &sort_open_prev_w, 0, 0, "Sort Open Prev Menu" },
    { "appendLF",      MMToggle, { (XtCallbackProc) dummyCB, 0 },
      NULL, &append_linefeed_w, 0, 0, "Append Line Feed on Save" },
    { "popupUnder",    MMToggle, { (XtCallbackProc) dummyCB, 0 },
      NULL, &popup_under_ptr_w, 0, 0, "Popups Under Pointer" },
    { "showPath",      MMToggle, { (XtCallbackProc) dummyCB, 0 },
      NULL, &show_path_w, 0, 0, "Show Path In Windows Menu" },
    { "winSize",       MMPanel,  MMNoCB, 
      window_size_menu, 0, 0, 0, "Initial window size:" },
    MMEnd
};

static MMDesc style_menu[] = 
{
    { "matching",     MMPanel,  MMNoCB, 
      matching_menu, 0, 0, 0, "Show Matching" },
    { "wrapStyle",    MMPanel,  MMNoCB, 
      wrap_menu, 0, 0, 0, "Wrap" },
    { "wrapMargin",   MMButtonPanel | MMVertical,  MMNoCB, 
      wrap_margin_menu, 0, 0, 0, "" },
    { "tagCollision", MMRadioPanel,  MMNoCB, 
      tag_collision_menu, 0, 0, 0, "Tag Collisions" },
    { "autoIndentStyle", MMRadioPanel,  MMNoCB, 
      indent_style_menu, 0, 0, 0, "Auto Indent" },
    { "highligting",     MMRadioPanel,  MMNoCB, 
      highlighting_menu, 0, 0, 0, "Syntax Highlighting" },
    { "backligting",     MMButtonPanel,  MMNoCB, 
      backlighting_menu, 0, 0, 0, "Backlighting" },
    MMEnd
};

static Widget search_style_w[6];
static MMDesc search_style_menu [] = 
{
    { "lite", MMPush, { prefSearchStyleCB, (XtPointer)SEARCH_LITERAL }, 
      NULL, &search_style_w[SEARCH_LITERAL], 0, 0, "Literal"},
    { "liteCase", MMPush, { prefSearchStyleCB, (XtPointer)SEARCH_CASE_SENSE }, 
      NULL, &search_style_w[SEARCH_CASE_SENSE], 0, 0, "Literal, Case Sensitive"},
    { "liteWord", MMPush, { prefSearchStyleCB, (XtPointer)SEARCH_LITERAL_WORD }, 
      NULL, &search_style_w[SEARCH_LITERAL_WORD], 0, 0, "Literal, Whole Word"},
    { "liteCase", MMPush, { prefSearchStyleCB, (XtPointer)SEARCH_CASE_SENSE_WORD }, 
      NULL, &search_style_w[SEARCH_CASE_SENSE_WORD], 0, 0, "Literal, Case Sensitive, Whole Word"},
    { "regex", MMPush, { prefSearchStyleCB, (XtPointer)SEARCH_REGEX }, 
      NULL, &search_style_w[SEARCH_REGEX], 0, 0, "Regular Expression"},
    { "regexNoCase", MMPush, { prefSearchStyleCB, (XtPointer)SEARCH_REGEX_NOCASE }, 
      NULL, &search_style_w[SEARCH_REGEX_NOCASE], 0, 0, "Regular Expression, Case Insensitive"},
    MMEnd
};

#ifdef REPLACE_SCOPE
static Widget replace_style_w[3];
static MMDesc replace_style_menu [] = 
{
    { "window", MMPush, { prefReplaceStyleCB, (XtPointer)REPL_DEF_SCOPE_WINDOW }, 
      NULL, &replace_style_w[REPL_DEF_SCOPE_WINDOW], 0, 0, "In Window"},
    { "selection", MMPush, { prefReplaceStyleCB, (XtPointer)REPL_DEF_SCOPE_SELECTION }, 
      NULL, &replace_style_w[REPL_DEF_SCOPE_SELECTION], 0, 0, "In Selection"},
    { "smart", MMPush, { prefReplaceStyleCB, (XtPointer)REPL_DEF_SCOPE_SMART }, 
      NULL, &replace_style_w[REPL_DEF_SCOPE_SMART], 0, 0, "Smart"},
    MMEnd
};
#endif

static Widget search_opt_verbose_w;
static Widget search_opt_wrap_w;
static Widget search_opt_beep_w;
static Widget search_opt_keep_dialog_w;
static MMDesc search_menu [] = 
{
    { "verbose",  MMToggle, { (XtCallbackProc) dummyCB, 0 },
    	0, &search_opt_verbose_w, 0,0, "Verbose"},
    { "wrap",     MMToggle, { (XtCallbackProc) dummyCB, 0 },
    	0, &search_opt_wrap_w, 0, 0, "Wrap Around"},
    { "beep",     MMToggle, { (XtCallbackProc) dummyCB, 0 },
    	0, &search_opt_beep_w, 0, 0, "Beep On Search Wrap"},
    { "keep",     MMToggle, { (XtCallbackProc) dummyCB, 0 },
    	0, &search_opt_keep_dialog_w, 0, 0, "Keep Dialogs Up"},
    { "searchStyle",     MMOptionMenu,  MMNoCB, 
      search_style_menu, 0, 0, 0, "Default Search Style:" },
#ifdef REPLACE_SCOPE
    { "replaceStyle",     MMOptionMenu,  MMNoCB, 
      replace_style_menu, 0, 0, 0, "Default Replace Style:" },
#endif
    MMEnd
};

static Widget warn_ext_file_mod_w;
static Widget warn_check_real_file_w;
static Widget warn_on_exit_w;
static MMDesc warnings_menu [] = 
{
    { "warnExtFile",   MMToggle, { (XtCallbackProc) modWarnDefCB, 0 },
    	0, &warn_ext_file_mod_w, 0,0, "Files Modified Externally"},
    { "warnCheckFile", MMToggle, { (XtCallbackProc) dummyCB, 0 },
    	0, &warn_check_real_file_w, 0, 0, "Check Modified File Contents"},
    { "warnExit",      MMToggle, { (XtCallbackProc) dummyCB, 0 },
    	0, &warn_on_exit_w, 0, 0, "On Exit"},
    MMEnd
};

static void modWarnDefCB(Widget w, WindowInfo *window, caddr_t callData)
{
    int state = XmToggleButtonGetState(w);

    XtSetSensitive(warn_check_real_file_w, state);
    set_sensitive(apply_button, True);
}

static Widget EmTabText, EmTabToggle, UseTabsToggle, TabDistText;

static void emTabToggleCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    int state = XmToggleButtonGetState(w);
    XtSetSensitive(EmTabText, state);
    set_sensitive(apply_button, True);
}

static MMDesc tabs_menu [] = 
{
    { "tabDist", MMTextField, { dummyCB, 0 }, 
      NULL, &TabDistText, 0, 0, "Hardware Tab Size:" },
    { "emTabDist", MMTextField, { dummyCB, 0 }, 
      NULL, &EmTabText, 0, 0, "Emulated Tab Size:" },
    { "emTabToggle",      MMToggle, {  emTabToggleCB, 0 },
    	0, &EmTabToggle, 0, 0, "Emulate tabs"},
    { "useTabsToggle",    MMToggle, { dummyCB, 0 },
    	0, &UseTabsToggle, 0, 0, "Use tab characters in padding"},
    MMEnd
};

static MMDesc customize_menu [] = 
{
    { "label", MMLabel, { (XtCallbackProc)0, 0 }, 
      NULL, 0, 0, 0, "[ WARNING: changes apply within the editors ]" },
    { "edit", MMPush, { (XtCallbackProc)macroDefCB, 0 }, 
      NULL, 0, 0, 0, "Edit Macro Menu..." },
#ifndef VMS
    { "edit", MMPush, { (XtCallbackProc)shellDefCB, 0 }, 
      NULL, 0, 0, 0, "Edit Shell Menu..." },
#endif /* VMS */
    { "edit", MMPush, { (XtCallbackProc)bgMenuDefCB, 0 }, 
      NULL, 0, 0, 0, "Edit Window Background Menu..." },
    
    MMSep,
      
    { "edit", MMPush, { (XtCallbackProc)languageDefCB, 0 }, 
      NULL, 0, 0, 0, "Edit Language Modes..." },
    { "edit", MMPush, { (XtCallbackProc)highlightingDefCB, 0 }, 
      NULL, 0, 0, 0, "Edit Syntax Highlighting Patterns..." },
    { "edit", MMPush, { (XtCallbackProc)stylesDefCB, 0 }, 
      NULL, 0, 0, 0, "Edit Text Drawing Styles..." },

    MMSep,
    
    { "edit", MMPush, { (XtCallbackProc)smartMacrosDefCB, 0 }, 
      NULL, 0, 0, 0, "Program Smart Indent..." },
      
    { "edit", MMPush, { (XtCallbackProc)customizeTitleDefCB, 0 }, 
      NULL, 0, 0, 0, "Customize Window Title..." },

    { "edit", MMPush, { (XtCallbackProc)fontDefCB, 0 }, 
      NULL, 0, 0, 0, "Choose Text Font..." },
      
    MMEnd
};

/* The currently unflattened button */
static Widget active_button = 0;

static void unflatten_button(Widget w, bool switch_colors)
{
    Pixel background;
    Pixmap highlight_pixmap, label_pixmap;
    Pixmap bottom_shadow_pixmap;
    XtVaGetValues(w,
		  XmNbackground,         &background,
		  XmNlabelPixmap,        &label_pixmap,
		  XmNhighlightPixmap,    &highlight_pixmap,
		  XmNbottomShadowPixmap, &bottom_shadow_pixmap,
		  NULL);

    if (bottom_shadow_pixmap != XmUNSPECIFIED_PIXMAP)
    {
#if LOG_FLATTENING
	clog << "Unflattening " << XtName(w) << "\n";
#endif

	Arg args[10];
	Cardinal arg = 0;

	XtSetArg(args[arg], XmNbottomShadowPixmap, 
		 XmUNSPECIFIED_PIXMAP); arg++;
	XtSetArg(args[arg], XmNtopShadowPixmap,
		 XmUNSPECIFIED_PIXMAP); arg++;

	if (switch_colors)
	{
	    XtSetArg(args[arg], XmNlabelPixmap, highlight_pixmap); arg++;
	    XtSetArg(args[arg], XmNhighlightPixmap, label_pixmap); arg++;
	}

	XtSetValues(w, args, arg);
    }
}

static void flatten_button(Widget w, bool switch_colors)
{
    Pixel background;
    Pixmap highlight_pixmap, label_pixmap;
    Pixmap bottom_shadow_pixmap;
    XtVaGetValues(w,
		  XmNbackground,         &background,
		  XmNlabelPixmap,        &label_pixmap,
		  XmNhighlightPixmap,    &highlight_pixmap,
		  XmNbottomShadowPixmap, &bottom_shadow_pixmap,
		  NULL);

    if (bottom_shadow_pixmap == XmUNSPECIFIED_PIXMAP)
    {
	Arg args[10];
	Cardinal arg = 0;

	Pixmap empty = XmGetPixmap(XtScreen(w), (char *)"background", 
				   background, background);

	XtSetArg(args[arg], XmNbottomShadowPixmap, empty); arg++;
	XtSetArg(args[arg], XmNtopShadowPixmap,    empty); arg++;

	if (switch_colors)
	{
	    XtSetArg(args[arg], XmNlabelPixmap, highlight_pixmap); arg++;
	    XtSetArg(args[arg], XmNhighlightPixmap, label_pixmap); arg++;
	}

	XtSetValues(w, args, arg);
    }
}

static void set_sensitive(Widget w, bool state)
{
    if (w != 0)
    {
	XtSetSensitive(w, state);

	if (!state && w == active_button)
	{
	    /* We won't get the LeaveWindow event, since W is now */
	    /* insensitive.  Flatten button explicitly. */
	    flatten_button(w, true);
	}
    }
}

static Widget CreateSpinBox(Widget parent, String name, ArgList _args, Cardinal _arg)
{
    assert(0);
    return NULL;
}

static void CloseWhenActivatedCB(XtPointer client_data, XtIntervalId *id)
{
    ComboBoxInfo *info = (ComboBoxInfo *)client_data;
    
    assert(*id == info->timer);
    (void) id;

    info->timer = 0;
}

static void PopdownComboListCB(Widget w, XtPointer client_data, XtPointer p)
{
    ComboBoxInfo *info = (ComboBoxInfo *)client_data;

    XtVaSetValues(info->button, XmNarrowDirection, XmARROW_DOWN, NULL);
    XtPopdown(info->shell);
    info->popped_up = false;
}

static void PopupComboListCB(Widget w, XtPointer client_data, 
			     XtPointer call_data)
{
    ComboBoxInfo *info = (ComboBoxInfo *)client_data;
    Position top_x, top_y;
    Dimension top_height = 0;
    Dimension top_width  = 0;
    Dimension current_height;
    XtWidgetGeometry size;
    Position x;
    Position y;
    Dimension width;
    Dimension height;
    Widget horizontal_scrollbar;
    static Cursor cursor;
    
    if (info->popped_up)
    {
	PopdownComboListCB(w, client_data, call_data);
	return;
    }

    /* Get size and position */
    XtTranslateCoords(info->top, 0, 0, &top_x, &top_y);

    XtVaGetValues(info->top, XmNheight, &top_height,
		  XmNwidth, &top_width, NULL);

    /* Query preferred height of scroll window */
    size.request_mode = CWHeight;
    XtQueryGeometry(XtParent(info->list), (XtWidgetGeometry *)0, &size);

    XtVaGetValues(info->shell, XmNheight, &current_height, NULL);
    
    x	    = top_x;
    y	    = top_y + top_height;
    width  = top_width;
    height = max(size.height, current_height);

    XtVaSetValues(info->shell, XmNx, x, XmNy, y, 
		  XmNwidth, width, XmNheight, height, NULL);

    /* Popup shell */
    XtPopup(info->shell, XtGrabNone);
    if (XtIsRealized(info->shell))
	XRaiseWindow(XtDisplay(info->shell), XtWindow(info->shell));
    info->popped_up = true;

    /* Unmanage horizontal scrollbar */
    horizontal_scrollbar = 0;
    XtVaGetValues(XtParent(info->list), XmNhorizontalScrollBar, 
		  &horizontal_scrollbar, NULL);
    if (horizontal_scrollbar != 0)
	XtUnmanageChild(horizontal_scrollbar);

    XtVaSetValues(info->button, XmNarrowDirection, XmARROW_UP, NULL);

    cursor = XCreateFontCursor(XtDisplay(info->shell), XC_arrow);
    XDefineCursor(XtDisplay(info->shell), XtWindow(info->shell), cursor);

    /* If we release the button within the next 250ms, keep the menu open. */
    /* Otherwise, pop it down again. */
    if (info->timer != 0)
	XtRemoveTimeOut(info->timer);
    info->timer = 
	XtAppAddTimeOut(XtWidgetToApplicationContext(info->shell), 250,
			CloseWhenActivatedCB, (XtPointer)info);
}

static void ActivatePopdownComboListCB(Widget w, XtPointer client_data, 
				       XtPointer call_data)
{
    ComboBoxInfo *info = (ComboBoxInfo *)client_data;
    if (info->timer == 0)
	PopdownComboListCB(w, client_data, call_data);
}

static void RefreshComboTextCB(Widget w, XtPointer client_data,
			       XtPointer call_data)
{
    /* (void) w;			// Use it */

    XmListCallbackStruct *cbs = (XmListCallbackStruct *)call_data;
    ComboBoxInfo *info = (ComboBoxInfo *)client_data;
    XmTextPosition last_pos;
    XmString item = cbs->item;
    String item_s;
    
    /*XmStringGetLtoR(item, CHARSET_TT, &item_s); */
    XmStringGetLtoR(item, XmSTRING_DEFAULT_CHARSET, &item_s);
    XmTextFieldSetString(info->text, item_s);
    XtFree(item_s);

    last_pos = XmTextFieldGetLastPosition(info->text);
    XmTextFieldSetInsertionPosition(info->text, last_pos);
    XmTextFieldShowPosition(info->text, 0);
    XmTextFieldShowPosition(info->text, last_pos);

#if !USE_XM_COMBOBOX
    PopdownComboListCB(w, client_data, call_data);
#endif
}

/* FIXME: this function should have been a macro */
static ArgList newArgs(Cardinal _arg)
{
    return (ArgList) XtMalloc(sizeof(Arg) * _arg);
}

static Widget CreateComboBox(Widget parent, const _XtString name, ArgList _args, Cardinal _arg)
{
    ArgList args = newArgs(_arg + 10); /* = new Arg[_arg + 10]; */
    Cardinal i, arg = 0;
    Dimension shadow_thickness;
    Pixel foreground;
    Widget shell = parent, form;
    XtWidgetGeometry size;
    ComboBoxInfo *info = (ComboBoxInfo *)XtMalloc(sizeof(ComboBoxInfo));

    arg = 0;
    XtSetArg(args[arg], XmNshadowType, XmSHADOW_IN); arg++;
    XtSetArg(args[arg], XmNmarginWidth,        0); arg++;
    XtSetArg(args[arg], XmNmarginHeight,       0); arg++;
    XtSetArg(args[arg], XmNborderWidth,        0); arg++;
    XtSetArg(args[arg], XmNhighlightThickness, 0); arg++;
    info->top = verify(XmCreateFrame(parent, (char *)"frame", args, arg));
    XtManageChild(info->top);

    arg = 0;
    XtSetArg(args[arg], XmNmarginWidth,        0); arg++;
    XtSetArg(args[arg], XmNmarginHeight,       0); arg++;
    XtSetArg(args[arg], XmNborderWidth,        0); arg++;
    XtSetArg(args[arg], XmNhighlightThickness, 0); arg++;
    form = verify(XmCreateForm(info->top, (char *)"form", args, arg));
    XtManageChild(form);

#if USE_XM_COMBOBOX
    /* ComboBoxes in OSF/Motif 2.0 sometimes resize themselves without */
    /* apparent reason.  Prevent this by constraining them in a form. */
    arg = 0;
    XtSetArg(args[arg], XmNleftAttachment,     XmATTACH_FORM); arg++;
    XtSetArg(args[arg], XmNrightAttachment,    XmATTACH_FORM); arg++;
    XtSetArg(args[arg], XmNtopAttachment,      XmATTACH_FORM); arg++;
    XtSetArg(args[arg], XmNbottomAttachment,   XmATTACH_FORM); arg++;
    XtSetArg(args[arg], XmNresizable,          False);         arg++;
    for (Cardinal i = 0; i < _arg; i++)
	args[arg++] = _args[i];
    Widget combo = verify(XmCreateDropDownComboBox(form, (char *)name, 
						   args, arg));
    XtManageChild(combo);

    arg = 0;
    XtSetArg(args[arg], XmNshadowThickness, 0); arg++;
    XtSetArg(args[arg], XmNhighlightThickness, 0); arg++;
    XtSetArg(args[arg], XmNborderWidth, 0); arg++;
    XtSetArg(args[arg], XmNmarginWidth, 0); arg++;
    XtSetArg(args[arg], XmNmarginHeight, 0); arg++;
    XtSetValues(combo, args, arg);

    info->text = XtNameToWidget(combo, "*Text");
    arg = 0;
    XtSetArg(args[arg], XmNshadowThickness, 0); arg++;
    XtSetValues(info->text, args, arg);

    info->list = XtNameToWidget(combo, "*List");
    arg = 0;
    XtSetArg(args[arg], XmNshadowThickness,    2); arg++;
    XtSetArg(args[arg], XmNhighlightThickness, 0); arg++;
    XtSetValues(info->list, args, arg);

    info->shell = info->list;
    while (!XtIsShell(info->shell))
	info->shell = (XtParent)(info->shell);

    /* Set form size explicitly. */
    size.request_mode = CWHeight | CWWidth;
    XtQueryGeometry(combo, XtWidgetGeometry(0), &size);
    XtVaSetValues(form, 
		  XmNheight, size.height, 
		  XmNwidth, size.width,
		  NULL);

    /* Set frame size explicitly, too */
    XtVaGetValues(info->top,
		  XmNshadowThickness, &shadow_thickness,
		  NULL);
    XtVaSetValues(info->top, 
		  XmNheight, size.height + shadow_thickness * 2, 
		  XmNwidth, size.width + shadow_thickness * 2,
		  NULL);
#else
    /* Create text field and arrow */
    arg = 0;
    XtSetArg(args[arg], XmNborderWidth,        0);     arg++;
    XtSetArg(args[arg], XmNhighlightThickness, 0);     arg++;
    XtSetArg(args[arg], XmNshadowThickness,    0);     arg++;
    XtSetArg(args[arg], XmNresizable,          False); arg++;
    for (i = 0; i < _arg; i++)
	args[arg++] = _args[i];
    info->text = verify(XmCreateTextField(form, (char *)name, args, arg));
    XtManageChild(info->text);

    XtVaGetValues(parent, XmNbackground, &foreground, NULL);

    arg = 0;
    XtSetArg(args[arg], XmNarrowDirection,     XmARROW_DOWN);  arg++;
    XtSetArg(args[arg], XmNborderWidth,        0);             arg++;
    XtSetArg(args[arg], XmNforeground,         foreground);    arg++;
    XtSetArg(args[arg], XmNhighlightThickness, 0);             arg++;
    XtSetArg(args[arg], XmNshadowThickness,    0);             arg++;
    XtSetArg(args[arg], XmNresizable,          False);         arg++;
    XtSetArg(args[arg], XmNrightAttachment,    XmATTACH_FORM); arg++;
    XtSetArg(args[arg], XmNtopAttachment,      XmATTACH_FORM); arg++;
    XtSetArg(args[arg], XmNbottomAttachment,   XmATTACH_FORM); arg++;
    info->button = XmCreateArrowButton(form, 
				       (char *)"comboBoxArrow", args, arg);
    XtManageChild(info->button);

    XtVaSetValues(info->text,
		  XmNleftAttachment,   XmATTACH_FORM,
		  XmNrightAttachment,  XmATTACH_WIDGET,
		  XmNrightWidget,      info->button,
		  XmNtopAttachment,    XmATTACH_FORM,
		  XmNbottomAttachment, XmATTACH_FORM,
		  NULL);

    XtAddCallback(info->button, XmNarmCallback,
		  PopupComboListCB, (XtPointer)info);
    XtAddCallback(info->button, XmNactivateCallback,
		  ActivatePopdownComboListCB, (XtPointer)info);

    XtAddCallback(info->text, XmNvalueChangedCallback,
		  PopdownComboListCB, (XtPointer)info);
    XtAddCallback(info->text, XmNactivateCallback,
		  PopdownComboListCB, (XtPointer)info);

    /* Create the popup shell */
    while (!XtIsShell(shell))
	shell = XtParent(shell);

    XtAddCallback(shell, XmNpopdownCallback,
		  PopdownComboListCB, (XtPointer)info);

    arg = 0;
    XtSetArg(args[arg], XmNborderWidth, 0); arg++;
    info->shell = XtCreatePopupShell("comboBoxShell", 
				     overrideShellWidgetClass,
				     parent, args, arg);

    arg = 0;
    XtSetArg(args[arg], XmNhighlightThickness, 0); arg++;
    info->list = XmCreateScrolledList(info->shell, (char *)"list", args, arg);
    XtManageChild(info->list);

    /* Keep shell on top */
    /*_auto_raise(info->shell); */

    /* Set form size explicitly. */
    size.request_mode = CWHeight | CWWidth;
    XtQueryGeometry(info->text, (XtWidgetGeometry *)0, &size);
    XtVaSetValues(form,
		  XmNheight, size.height,
		  XmNwidth, size.width,
		  NULL);

    /* Set frame size explicitly, too */
    XtVaGetValues(info->top,
		  XmNshadowThickness, &shadow_thickness,
		  NULL);
    XtVaSetValues(info->top,
		  XmNheight, size.height + shadow_thickness * 2, 
		  XmNwidth, size.width + shadow_thickness * 2,
		  NULL);
#endif

    /* Give shell a little more border */
    XtVaSetValues(info->shell, XmNborderWidth, 1, NULL);

    /* Store ComboBox info in text field */
    XtVaSetValues(info->text, XmNuserData, (XtPointer)info, NULL);

    /* Synchronize text and list */
    XtAddCallback(info->list, XmNbrowseSelectionCallback,
		  RefreshComboTextCB, (XtPointer)info);
    XtAddCallback(info->list, XmNsingleSelectionCallback,
		  RefreshComboTextCB, (XtPointer)info);
    XtAddCallback(info->list, XmNmultipleSelectionCallback,
		  RefreshComboTextCB, (XtPointer)info);
    XtAddCallback(info->list, XmNextendedSelectionCallback,
		  RefreshComboTextCB, (XtPointer)info);

    /* delete[] args; */
    XtFree((char*)args);
    return info->text;
}

static void ArmPushMenuCB(Widget w, XtPointer client_data, XtPointer call_data)
{
}

static void RedrawPushMenuCB(Widget w, XtPointer p, XtPointer p1)
{
    XtCallActionProc(w, "decorate-push-menu", 0, 0, 0);
}

static void FlattenEH(Widget w,
		      XtPointer client_data,
		      XEvent *event, 
		      Boolean *continue_to_dispatch)
{
    if (event->xcrossing.state & (Button1Mask | Button2Mask | Button3Mask | 
				  Button4Mask | Button5Mask))
	return;			/* Button is still pressed */

    switch (event->type)
    {
    case EnterNotify:
    {
#if LOG_FLATTENING
	clog << "Entering " << XtName(w) << "\n";
#endif

	unflatten_button(w, true);
	active_button = w;
	break;
    }

    case LeaveNotify:
    {
#if LOG_FLATTENING
	clog << "Leaving " << XtName(w) << "\n";
#endif

	flatten_button(w, true);
	active_button = 0;
	break;
    }
    }
}

/* Handle Arm() and Disarm() actions */
static void FlattenCB(Widget w, XtPointer client_data, XtPointer p)
{
    bool set = (bool) client_data;
    
    if (w == active_button)
    {
	/* We have already entered it -- don't interfere */
	return;
    }

    if (set)
    {
	/* clog << "Arming " << XtName(w) << "\n"; */
	flatten_button(w, false);
    }
    else
    {
	/* clog << "Disarming " << XtName(w) << "\n"; */
	unflatten_button(w, false);
    }
}

static void ReflattenButtonCB(Widget shell, XtPointer client_data, 
			      XtPointer p)
{
    Widget w = (Widget)client_data;
    EventMask event_mask = EnterWindowMask | LeaveWindowMask;
    XtAddEventHandler(w, event_mask, False, FlattenEH, (XtPointer)0);
    XtAddCallback(w, XmNarmCallback,    FlattenCB, (XtPointer)False);
    XtAddCallback(w, XmNdisarmCallback, FlattenCB, (XtPointer)True);
    flatten_button(w, true);
}

/* Access ComboBox members */
Widget ComboBoxList(Widget text)
{
    XtPointer userData;
    ComboBoxInfo *info;
    XtVaGetValues(text, XmNuserData, &userData, NULL);
    info = (ComboBoxInfo *)userData;
    return info->list;
}

/* Add callbacks to items */
static void addCallback(MMDesc *item, XtPointer default_closure)
{
    MMType flags            = item->type;
    MMType type             = flags & MMTypeMask;
    Widget widget           = item->widget;
    XtCallbackRec callback  = item->callback;
    bool flat = false;
    
    if (callback.closure == 0)
	callback.closure = default_closure;


    switch(type) 
    {
    case MMFlatPush:
    {
	flat = true;
	/* FALL THROUGH */
    }

    case MMPush:
    {
	void *userData = 0;
	XtVaGetValues(widget, XmNuserData, &userData, NULL);

	if (userData != 0)
	{
	    PushMenuInfo *info = (PushMenuInfo *)userData;
    	    static XtTranslations translations;
	    
	    /* A 'push menu' is a menu associated with a push button. */
	    /* It pops up after pressing the button a certain time. */
	    XtAddCallback(widget, XmNarmCallback,    ArmPushMenuCB, info);
	    XtAddCallback(widget, XmNarmCallback,    RedrawPushMenuCB, 0);
	    XtAddCallback(widget, XmNdisarmCallback, RedrawPushMenuCB, 0);

	    translations =
		    XtParseTranslationTable(pushMenuTranslations);
	    XtAugmentTranslations(widget, translations);

	    if (lesstif_version <= 81)
	    {
		/* In LessTif 0.81 and earlier, one must use button 3 */
		/* to pop up push menus */
		/* static XtTranslations lesstif_translations = */
		XtTranslations lesstif_translations =
		    XtParseTranslationTable(lesstif_pushMenuTranslations);
		XtAugmentTranslations(widget, lesstif_translations);
	    }
	}

	if (flat)
	{
	    ReflattenButtonCB(widget, (XtPointer)widget, 0);
	}

	/* FALL THROUGH */
    }

    case MMArrow:
    {
	if (callback.callback != 0)
	    XtAddCallback(widget, 
			  XmNactivateCallback,
			  callback.callback, 
			  callback.closure);
	else
	    set_sensitive(widget, False);
	break;
    }

    case MMToggle:
    case MMScale:
    {
	if (callback.callback != 0)
	    XtAddCallback(widget,
			  XmNvalueChangedCallback,
			  callback.callback, 
			  callback.closure);
	else
	    set_sensitive(widget, False);
	break;
    }

    case MMComboBox:
    {
	if (callback.callback != 0)
	{
	    Widget list = ComboBoxList(widget);

	    XtAddCallback(list, XmNbrowseSelectionCallback,
			  callback.callback, callback.closure);
	    XtAddCallback(list, XmNsingleSelectionCallback,
			  callback.callback, callback.closure);
	    XtAddCallback(list, XmNmultipleSelectionCallback,
			  callback.callback, callback.closure);
	    XtAddCallback(list, XmNextendedSelectionCallback,
			  callback.callback, callback.closure);
	}

	/* FALL THROUGH */
    }

    case MMSpinBox:
    case MMTextField:
    {
	if (callback.callback != 0)
	    XtAddCallback(widget,
			  XmNvalueChangedCallback,
			  callback.callback, 
			  callback.closure);

	if (type == MMTextField)
	    break;
	/* FALL THROUGH */
    }

    case MMEnterField:
    {
	if (callback.callback != 0)
	    XtAddCallback(widget,
			  XmNactivateCallback,
			  callback.callback, 
			  callback.closure);
	break;
    }

    case MMMenu:
    case MMRadioMenu:
    case MMOptionMenu:
    {
	Widget subMenu = 0;
	XtVaGetValues(widget, XmNsubMenuId, &subMenu, NULL);

	if (subMenu != 0 && callback.callback != 0)
	{
	    XtAddCallback(subMenu,
			  XmNmapCallback,
			  callback.callback, 
			  callback.closure);
	    XtAddCallback(subMenu,
			  XmNunmapCallback,
			  callback.callback, 
			  callback.closure);
#if XmVersion >= 1002
	    XtAddCallback(subMenu,
			  XmNtearOffMenuActivateCallback,
			  callback.callback, 
			  callback.closure);
	    XtAddCallback(subMenu,
			  XmNtearOffMenuDeactivateCallback,
			  callback.callback, 
			  callback.closure);
#endif
	}
	break;
    }

    case MMLabel:
    case MMSeparator:
    case MMPanel:
    case MMRadioPanel:
    case MMButtonPanel:
	assert(callback.callback == 0);
	break;

    default:
	/* invalid type */
	assert(0);
	abort();
    }
}

/* Add help callback */
static void addHelpCallback(MMDesc *item, XtPointer closure)
{
    Widget widget       = item->widget;
    XtCallbackProc proc = (XtCallbackProc)(closure);

    XtAddCallback(widget, XmNhelpCallback, proc, (XtPointer)(0));
}

void MMaddHelpCallback(MMDesc items[], XtCallbackProc proc, int depth)
{
    MMonItems(items, addHelpCallback, (XtPointer)(proc), depth);
}

/* Create radio pulldown menu from items */
Widget MMcreateRadioPulldownMenu(Widget parent, String name, MMDesc items[],
				 ArgList _args, Cardinal _arg)
{
    ArgList args = newArgs(_arg + 10); /* = new Arg[_arg + 10]; */
    Cardinal i, arg = 0;
    Widget w;
    
    XtSetArg(args[arg], XmNisHomogeneous, True); arg++;
    XtSetArg(args[arg], XmNentryClass, xmToggleButtonWidgetClass); arg++;
    XtSetArg(args[arg], XmNradioBehavior, True); arg++;

    for (i = 0; i < _arg; i++)
	args[arg++] = _args[i];

    w = MMcreatePulldownMenu(parent, name, items, args, arg);

    /*delete[] args; */
    XtFree((char*)args);
    return w;
}

/* Create button panel from items */
Widget MMcreateButtonPanel(Widget parent, const _XtString name, MMDesc items[],
			   ArgList args, Cardinal arg)
{
    Widget panel = verify(XmCreateRowColumn(parent, (char *)name, args, arg));
    MMaddItems(panel, items, false);
    XtManageChild(panel);

    return panel;
}

/* Perform proc on items */
void MMonItems(MMDesc items[], MMItemProc proc, XtPointer closure, int depth)
{
    MMDesc *item;
    
    if (depth == 0)
	return;

    for (item = items; item != 0 && item->name != 0; item++)
    {
	if (item->type & MMIgnore)
	    continue;

	proc(item, closure);

	if (item->items)
	    MMonItems(item->items, proc, closure, depth - 1);
    }
}


/* Create radio panel from items */
Widget MMcreateRadioPanel(Widget parent, const _XtString name, MMDesc items[],
			  ArgList _args, Cardinal _arg)
{
    ArgList args = newArgs(_arg + 10); /* = new Arg[_arg + 10]; */
    Cardinal i, arg = 0;
    Widget panel;

    XtSetArg(args[arg], XmNisHomogeneous, True);                      arg++;
    XtSetArg(args[arg], XmNentryClass,    xmToggleButtonWidgetClass); arg++;
    XtSetArg(args[arg], XmNradioBehavior, True);                      arg++;

    for (i = 0; i < _arg; i++)
	args[arg++] = _args[i];

    panel = verify(XmCreateRowColumn(parent, (char *)name, args, arg));
    MMaddItems(panel, items, false);
    XtManageChild(panel);

    /* delete[] args; */
    XtFree((char*)args);
    return panel;
}

/* Create pulldown menu from items */
Widget MMcreatePulldownMenu(Widget parent, String name, MMDesc items[],
			    ArgList args, Cardinal arg)
{
    Widget menu = verify(XmCreatePulldownMenu(parent, name, args, arg));
    MMaddItems(menu, items, false);
    /*auto_raise(XtParent(menu)); */

    return menu;
}


void MMadjustPanel(MMDesc items[], Dimension space)
{
    /* Align panel labels */
    Dimension max_label_width = 0;
    MMDesc *item;
    for (item = items; item != 0 && item->name != 0; item++)
    {
	XtWidgetGeometry size;
	if (item->label == 0)
	    continue;

	size.request_mode = CWWidth;
	XtQueryGeometry(item->label, NULL, &size);
	max_label_width = max(max_label_width, size.width);
    }

    /* Leave some extra space */
    max_label_width += space;

    for (item = items; item != 0 && item->name != 0; item++)
    {
	if (item->label == 0)
	    continue;

	XtVaSetValues(item->label,
		      XmNrecomputeSize, False,
		      XmNwidth, max_label_width,
		      NULL);
    }
}

/* Actions */
static XtActionsRec actions [] = {
    {NULL, NULL},
};

/*----------------------------------------------------------------------- */
/* Add items */
/*----------------------------------------------------------------------- */

/* Add items to shell.  If IGNORE_SEPS is set, all separators are ignored. */
void MMaddItems(Widget shell, MMDesc items[], bool ignore_seps)
{
    static bool actions_added = false;
    Arg args[10];
    int arg;
    XmString s1;
    MMDesc *item;
    
    if (!actions_added)
    {
	XtAppAddActions(XtWidgetToApplicationContext(shell), 
			actions, XtNumber(actions));
	actions_added = true;
    }

    /* Create lots of buttons... */
    for (item = items; item != 0 && item->name != 0; item++)
    {
	char *name              = (char *)item->name;
	MMType flags            = item->type;
	MMType type             = flags & MMTypeMask;
	Widget *widgetptr       = item->widgetptr;
	MMDesc *subitems        = item->items;

	char subMenuName[80]; /* = name + "Menu"; */
	char panelName[80];   /* = name + "Panel"; */
	static String textName  = "text";
	static String labelName = "label";
	Widget subMenu = 0;
	Widget panel   = 0;
	bool flat = false;
	item->label = 0;
	item->widget = 0;

    	sprintf(subMenuName, "%sMenu", name);
    	sprintf(panelName, "%sPanel", name);
	
	if (flags & MMIgnore)
	    continue;		/* Don't create */

	switch(type) 
	{
	case MMFlatPush:
	{
	    flat = true;
	    /* FALL THROUGH */
	}

	case MMPush:
	{
	    /* Create a PushButton */
	    PushMenuInfo *info = 0;
	    arg = 0;

	    if (flat)
	    {
	    	Pixmap empty;
		Pixel background;
		XtVaGetValues(shell, XmNbackground, &background, NULL);
		empty = XmGetPixmap(XtScreen(shell), 
					   (char *)"background", 
					   background, background);

		XtSetArg(args[arg], XmNbottomShadowPixmap, empty); arg++;
		XtSetArg(args[arg], XmNtopShadowPixmap,    empty); arg++;
		XtSetArg(args[arg], XmNhighlightThickness, 0);     arg++;
		XtSetArg(args[arg], XmNshadowThickness,    2);     arg++;
	    }
	    else
	    {
		XtSetArg(args[arg], 
			 XmNhighlightPixmap, XmUNSPECIFIED_PIXMAP); arg++;
	    }

	    if (lesstif_version <= 84)
	    {
		/* LessTif 0.84 and earlier wants the PushButton as */
		/* parent of the menu */
		item->widget = verify(XmCreatePushButton(shell, name, args, arg));

		if (subitems != 0)
		{
		    subMenu = MMcreatePushMenu(item->widget, subMenuName, subitems, 0, 0);
		    info = (PushMenuInfo *)XtMalloc(sizeof(PushMenuInfo));
		    info->widget = item->widget;
		    info->subMenu = subMenu;
		    info->flat = flat;
		    info->timer = 0;
		    XtVaSetValues(item->widget, XmNuserData, (XtPointer)info, NULL);
		}
	    }
	    else
	    {
		/* Motif wants the shell as parent of the menu */
		if (subitems != 0)
		{
		    subMenu = MMcreatePushMenu(shell, subMenuName, subitems, 0, 0);
		    info = (PushMenuInfo *)XtMalloc(sizeof(PushMenuInfo));
		    info->widget = 0;
		    info->subMenu = subMenu;
		    info->flat = flat;
		    info->timer = 0;
		    XtSetArg(args[arg], XmNuserData, (XtPointer)info); arg++;
		}

		item->widget = verify(XmCreatePushButton(shell, name, args, arg));

		if (info != 0)
		    info->widget = item->widget;
	    }
	    break;
	}

	case MMToggle:
	{
	    /* Create a ToggleButton */
	    assert(subitems == 0);

	    arg = 0;
	    item->widget = verify(XmCreateToggleButton(shell, name, args, arg));
	    break;
	}

	case MMLabel:
	{
	    /* Create a Label */
	    assert(subitems == 0);

	    arg = 0;
	    item->widget = verify(XmCreateLabel(shell, name, args, arg));
	    break;
	}

	case MMArrow:
	{
	    /* Create an arrow */
	    assert(subitems == 0);

	    arg = 0;
	    item->widget = verify(XmCreateArrowButton(shell, name, args, arg));
	    break;
	}

	case MMMenu:
	{
	    /* Create a CascadeButton and a new PulldownMenu */
	    assert(subitems != 0);

	    subMenu = MMcreatePulldownMenu(shell, subMenuName, subitems, 0, 0);

	    arg = 0;
	    XtSetArg(args[arg], XmNsubMenuId, subMenu); arg++;
	    item->widget = verify(XmCreateCascadeButton(shell, name, args, arg));

#if 0
            /* FIXME: I assume we are not going to use Lesstif < 0.90 */
            if (lesstif_version <= 79)
	    {
		/* LessTif 0.79 and earlier has a very tight packing */
		/* of menu items; place a few spaces around the labels */
		/* to increase item distance. */
		XmString old_label, new_label;
		XmString old_acc, new_acc;
		XtVaGetValues(item->widget, XmNlabelString, &old_label, NULL);
		new_label = XmStringCopy(old_label);
		XmStringFree(old_label);

		if (!new_label.isNull())
		{
		    new_label = MString("  ") + new_label + MString("  ");
		    XtVaSetValues(item->widget, 
				  XmNlabelString, new_label.xmstring(), 
				  NULL);
		}

		/* Same applies to accelerator texts. */
		XtVaGetValues(item->widget, XmNacceleratorText, &old_acc, NULL);
		new_acc = XmStringCopy(old_acc);
		XmStringFree(old_acc);

		if (!new_acc.isNull())
		{
		    new_acc = MString("  ") + new_acc;
		    XtVaSetValues(item->widget, 
				  XmNacceleratorText, new_acc.xmstring(), 
				  NULL);
		}
	    }
#endif
	    break;
	}

	case MMRadioMenu:
	{
	    /* Create a CascadeButton and a new PulldownMenu */
	    assert(subitems != 0);

	    subMenu = MMcreateRadioPulldownMenu(shell, subMenuName, subitems, 0, 0);

	    arg = 0;
	    XtSetArg(args[arg], XmNsubMenuId, subMenu); arg++;
	    item->widget = verify(XmCreateCascadeButton(shell, name, args, arg));
	    break;
	}

	case MMOptionMenu:
	{
	    /* Create an option menu */
	    assert(subitems != 0);

	    subMenu = MMcreatePulldownMenu(shell, subMenuName, subitems, 0, 0);

	    arg = 0;
	    XtSetArg(args[arg], XmNsubMenuId, subMenu); arg++;
	    item->widget = verify(XmCreateOptionMenu(shell, name, args, arg));
	    break;
	}

	case MMPanel:
	case MMRadioPanel:
	case MMButtonPanel:
	{
	    /* Create a label with an associated panel */
	    Widget (*create_panel)(Widget, const _XtString, MMDesc[], 
				   ArgList, Cardinal) = 0;
	    bool have_label;
	    assert(subitems != 0);

	    have_label = 
		(name[0] != '\0' && (flags & MMUnmanagedLabel) == 0);

	    arg = 0;
	    XtSetArg(args[arg], XmNorientation, XmHORIZONTAL); arg++;
	    XtSetArg(args[arg], XmNborderWidth,     0); arg++;
	    XtSetArg(args[arg], XmNentryBorder,     0); arg++;
	    XtSetArg(args[arg], XmNspacing,         0); arg++;
	    XtSetArg(args[arg], XmNmarginWidth,     0); arg++;
	    XtSetArg(args[arg], XmNmarginHeight,    0); arg++;
	    XtSetArg(args[arg], XmNshadowThickness, 0); arg++;

	    item->widget = verify(XmCreateRowColumn(shell, panelName, args, arg));

	    arg = 0;
	    item->label = verify(XmCreateLabel(item->widget, name, args, arg));
	    if (have_label)
		XtManageChild(item->label);


	    switch (type)
	    {
	    case MMPanel:
		create_panel = MMcreatePanel;
		break;

	    case MMRadioPanel:
		create_panel = MMcreateRadioPanel;
		break;

	    case MMButtonPanel:
		create_panel = MMcreateButtonPanel;
		break;

	    default:
		assert(0);
		abort();
	    }

	    arg = 0;
	    XtSetArg(args[arg], XmNorientation, 
		     (flags & MMVertical) ? XmVERTICAL : XmHORIZONTAL); arg++;

	    if (!have_label)
	    {
		XtSetArg(args[arg], XmNborderWidth,     0); arg++;
		XtSetArg(args[arg], XmNentryBorder,     0); arg++;
		XtSetArg(args[arg], XmNspacing,         0); arg++;
		XtSetArg(args[arg], XmNmarginWidth,     0); arg++;
		XtSetArg(args[arg], XmNmarginHeight,    0); arg++;
		XtSetArg(args[arg], XmNshadowThickness, 0); arg++;
	    }

	    subMenu = create_panel(item->widget, subMenuName, subitems, args, arg);

	    XtManageChild(subMenu);
	    break;
	}

	case MMScale:
	{
	    /* Create a scale */
	    assert(subitems == 0);

	    arg = 0;
	    item->widget = verify(XmCreateScale(shell, name, args, arg));
	    break;
	}

	case MMSpinBox:
	case MMComboBox:
	case MMTextField:
	case MMEnterField:
	{
	    /* Create a label with an associated text field */
	    assert(subitems == 0);

	    arg = 0;
	    XtSetArg(args[arg], XmNorientation,     XmHORIZONTAL); arg++;
	    XtSetArg(args[arg], XmNborderWidth,     0); arg++;
	    XtSetArg(args[arg], XmNentryBorder,     0); arg++;
	    XtSetArg(args[arg], XmNspacing,         0); arg++;
	    XtSetArg(args[arg], XmNmarginWidth,     0); arg++;
	    XtSetArg(args[arg], XmNmarginHeight,    0); arg++;
	    XtSetArg(args[arg], XmNshadowThickness, 0); arg++;

	    panel = verify(XmCreateRowColumn(shell, name, args, arg));

	    arg = 0;
	    item->label = verify(XmCreateLabel(panel, labelName, args, arg));
	    if (name[0] != '\0' && (flags & MMUnmanagedLabel) == 0)
		XtManageChild(item->label);

	    switch (type)
	    {
	    case MMSpinBox:
		arg = 0;
		item->widget = CreateSpinBox(panel, textName, args, arg);
		break;

	    case MMComboBox:
		arg = 0;
		item->widget = CreateComboBox(panel, textName, args, arg);
		break;

	    case MMTextField:
	    case MMEnterField:
		arg = 0;
		item->widget = verify(XmCreateTextField(panel, textName, args, arg));
		XtManageChild(item->widget);
		break;
	    }
	    break;
	}

	case MMSeparator:
	{
	    /* Create a separator */
	    assert(subitems == 0);

	    if (ignore_seps)
		continue;
	    arg = 0;
	    item->widget = verify(XmCreateSeparator(shell, name, args, arg));
	    break;
	}

	default:
	    /* Invalid type */
	    assert(0);
	    abort();
	}

	if (flags & MMHelp)
	{
	    arg = 0;
	    XtSetArg(args[arg], XmNmenuHelpWidget, item->widget); arg++;
	    XtSetValues(shell, args, arg);
	}

    	if (item->labelString) {
	    Widget wgt = item->label? item->label : item->widget;
    	    XtVaSetValues(wgt, XmNlabelString, 
	    	    s1=XmStringCreateSimple(item->labelString), NULL);
	    XmStringFree(s1);
    	}
	
	if (panel == 0)
	    panel = item->widget;

	if (flags & MMInsensitive)
	    set_sensitive(panel, False);

	if (!(flags & MMUnmanaged))
	    XtManageChild(panel);

	if (widgetptr != 0)
	    *widgetptr = item->widget;
    }
}

/* Create panel from items */
Widget MMcreatePanel(Widget parent, const _XtString name, MMDesc items[],
		     ArgList args, Cardinal arg)
{
    Widget panel = verify(XmCreateWorkArea(parent, (char *)name, args, arg));
    MMaddItems(panel, items, false);
    XtManageChild(panel);

    return panel;
}

/*----------------------------------------------------------------------- */
/* PushMenus */
/*----------------------------------------------------------------------- */

/* Create pushmenu from items */
Widget MMcreatePushMenu(Widget parent, String name, MMDesc items[],
			ArgList _args, Cardinal _arg)
{
    ArgList args = newArgs(_arg + 10); /* = new Arg[_arg + 10]; */
    Cardinal arg = 0, i;
    Widget menu;
    
    /* By default, PushButton menus are activated using Button 1. */
    if (XmVersion < 1002 || lesstif_version <= 84)
    {
	/* Setting the menuPost resource is required by Motif 1.1 and */
	/* LessTif 0.84 and earlier.  However, OSF/Motif 2.0 (and */
	/* OSF/Motif 1.2, according to Roy Dragseth */
	/* <royd@math.uit.no>) choke on this line - buttons become */
	/* entirely insensitive. */
	XtSetArg(args[arg], XmNmenuPost, "<Btn1Down>"); arg++;
    }

#if XmVersion >= 1002
    /* Tear-off push menus don't work well - in LessTif, they cause */
    /* frequent X errors, and in Motif, they disable the old menus */
    /* once torn off.  So, we explicitly disable them. */
    XtSetArg(args[arg], XmNtearOffModel, XmTEAR_OFF_DISABLED); arg++;
#endif

    for (i = 0; i < _arg; i++)
	args[arg++] = _args[i];
    
    menu = verify(XmCreatePopupMenu(parent, name, args, arg));
    MMaddItems(menu, items, false);
    /*auto_raise(XtParent(menu)); */

    /* LessTif places a passive grab on the parent, such that the */
    /* pointer is grabbed as soon as the menuPost event occurs.  This */
    /* grab breaks PushMenus, so we cancel it.  Motif places a passive */
    /* grab on button 3, such that the pointer is grabbed as soon as */
    /* button 3 is pressed.  In Motif 1.1, it even remains grabbed! */
    /* This breaks any X session, so we cancel it. */
    XtUngrabButton(parent, AnyButton, AnyModifier);

    /*delete[] args; */
    XtFree((char*)args);
    return menu;
}

void MMaddCallbacks(MMDesc items[], XtPointer default_closure, int depth)
{
    MMonItems(items, addCallback, default_closure, depth);
}

static void register_menu_shell(MMDesc *items)
{
}

static void HelpOnThisCB(Widget widget, XtPointer client_data, XtPointer call_data)
{
    DialogF(DF_INF, (Widget)client_data, 1,
    	    "This feature is under construction", "OK");
}

static void savePreferencesCB(Widget w, XtPointer dialogParent, 
	XtPointer call_data)
{
    SaveNEditPrefs((Widget)dialogParent, False);
    set_sensitive(save_button, CheckPrefsChanged());
}

static void update_reset_preferences()
{
}

static void listChangePanelCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    XmListCallbackStruct *cbs = (XmListCallbackStruct *) call_data;
    Widget panel_box = (Widget)client_data;
    Widget panel;
    WidgetList children;
    Cardinal i, num_children;

    update_reset_preferences();

    /* Unmanage all other children */
    XtVaGetValues(panel_box, 
		  XmNchildren, &children,
		  XmNnumChildren, &num_children,
		  NULL);

    /* Manage this child */
    panel = children[cbs->item_position-1];
    XtManageChild(panel);
    current_panel = panel;

    for (i = 0; i < num_children; i++)
    {
	Widget child = children[i];

	XtRemoveCallback(preferences_dialog, XmNhelpCallback,
			 HelpOnThisCB, (XtPointer)child);

	if (child != panel)
	{
	    XtUnmanageChild(child);
	}
    }

    XtAddCallback(preferences_dialog, XmNhelpCallback,
		  HelpOnThisCB, (XtPointer)(panel));
}

static void ChangePanelCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    Widget panel = (Widget)client_data;
    XmToggleButtonCallbackStruct *cbs = 
	(XmToggleButtonCallbackStruct *)call_data;

    if (cbs->set)
    {
	WidgetList children;
	Cardinal i, num_children;
	
	/* Manage this child */
	XtManageChild(panel);
	current_panel = panel;

	update_reset_preferences();

	/* Unmanage all other children */
	XtVaGetValues(XtParent(panel), 
		      XmNchildren, &children,
		      XmNnumChildren, &num_children,
		      NULL);

	for (i = 0; i < num_children; i++)
	{
	    Widget child = children[i];

	    XtRemoveCallback(preferences_dialog, XmNhelpCallback,
			     HelpOnThisCB, (XtPointer)child);

	    if (child != panel)
	    {
		XtUnmanageChild(child);
	    }
	}

	XtAddCallback(preferences_dialog, XmNhelpCallback,
		      HelpOnThisCB, (XtPointer)(panel));
    }
}

static void ImmediateHelpCB(Widget w, XtPointer client_data,
	XtPointer call_data)
{
}

static void OfferRestartCB(Widget dialog, XtPointer p, XtPointer p1)
{
}

static Widget add_panel(Widget parent, Widget buttons, 
			const _XtString name, MMDesc items[],
			Dimension *max_width, Dimension *max_height,
                        char *panel_label, bool set)
{
    Arg args[10];
    int arg;
    Widget form, panel, button;
    XtWidgetGeometry size;

    /* Add two rows */
    arg = 0;
    XtSetArg(args[arg], XmNmarginWidth,  0); arg++;
    XtSetArg(args[arg], XmNmarginHeight, 0); arg++;
    XtSetArg(args[arg], XmNborderWidth,  0); arg++;
    form = verify(XmCreateRowColumn(parent, (char *)name, args, arg));
    XtManageChild(form);

    /* Add panel */
    panel = MMcreatePanel(form, "panel", items, 0, 0);
    MMadjustPanel(items, 15);
    MMaddCallbacks(items, 0, -1);
    MMaddHelpCallback(items, ImmediateHelpCB, -1);
    XtManageChild(panel);
    register_menu_shell(items);

    /* Fetch panel geometry */
    size.request_mode = CWHeight | CWWidth;
    XtQueryGeometry(form, NULL, &size);

    size.width  += 10;		/* Compensate for small rounding errors */
    size.height += 10;

    *max_width  = max(*max_width,  size.width);
    *max_height = max(*max_height, size.height);

    /* Add button */
    if (PanelSelectorType == 0) {
        arg = 0;
        button = verify(XmCreateToggleButton(buttons, (char *)name, args, arg));
        XtManageChild(button);

        XtAddCallback(button, XmNvalueChangedCallback, ChangePanelCB, 
		      (XtPointer)form);

        XmToggleButtonSetState(button, set, False);

        if (set)
        {
	    XmToggleButtonCallbackStruct cbs;
	    cbs.set = set;
	    ChangePanelCB(button, (XtPointer)(form), &cbs);
        }
    }
    else {
        XmString s1 = MKSTRING(panel_label? panel_label : (char *)name);
        XmListAddItem(buttons, s1, 0);
        XmStringFree(s1); 
    }
    
    return button;
}

/*----------------------------------------------------------------------------- */
/* Option handling */
/*----------------------------------------------------------------------------- */

static void set_toggle(Widget w, unsigned char new_state, bool notify)
{
#if XmVersion < 2000
    Boolean old_state;
#else
    unsigned char old_state;
#endif

    if (w == 0)
	return;

    assert(XmIsToggleButton(w));

    XtVaGetValues(w, XmNset, &old_state, NULL);

    if (old_state != new_state)
    {
	if (notify)
	    XmToggleButtonSetState(w, new_state, True);
	else
	    XtVaSetValues(w, XmNset, new_state, NULL);
    }
}

/* update states of options in preference dialog */
static void update_options()
{
    Widget winSizeWidget;
    char winSizeString[20];
    int emulate, emTabDist, useTabs, tabDist;
    int margin = GetPrefWrapMargin();
    
    XmToggleButtonSetState(WrapWindowToggle, margin==0, True);
    if (margin != 0)
    	SetIntText(WrapText, margin);
    XtSetSensitive(WrapText, margin!=0);
    XtSetSensitive(WrapTextLabel, margin!=0);

    /* general preferences */
    set_toggle(show_linenum_w, GetPrefLineNums(), false);
    set_toggle(show_statsline_w, GetPrefStatsLine(), false);
    set_toggle(show_isearchline_w, GetPrefISearchLine(), false);
    set_toggle(incremental_backup_w, GetPrefAutoSave(), false);
    set_toggle(make_backup_copy_w, GetPrefSaveOldVersion(), false);
    set_toggle(append_linefeed_w, GetPrefAppendLF(), false);
    set_toggle(sort_open_prev_w, GetPrefSortOpenPrevMenu(), false);
    set_toggle(popup_under_ptr_w, GetPrefRepositionDialogs(), false);
    set_toggle(show_path_w, GetPrefShowPathInWindowsMenu(), false);

    /* backlighting */
    set_toggle(backlighting_w, GetPrefBacklightChars(), false);
    XmTextSetString(backlightCharTypes_w, GetPrefBacklightCharTypes());
    
    /* syntax hilite */
    set_toggle(highlighting_w[0], !GetPrefHighlightSyntax(), false);
    set_toggle(highlighting_w[1], GetPrefHighlightSyntax(), false);
    
    /* wrap style */
    wrapStyle = GetPrefWrap(PLAIN_LANGUAGE_MODE);
    set_toggle(wrap_style_w[0], wrapStyle == NO_WRAP, false);
    set_toggle(wrap_style_w[1], wrapStyle == NEWLINE_WRAP, false);
    set_toggle(wrap_style_w[2], wrapStyle == CONTINUOUS_WRAP, false);

    /* indent style */
    indentStyle = GetPrefAutoIndent(PLAIN_LANGUAGE_MODE);
    set_toggle(indent_style_w[0], indentStyle == NO_AUTO_INDENT, false);
    set_toggle(indent_style_w[1], indentStyle == AUTO_INDENT, false);
    set_toggle(indent_style_w[2], indentStyle == SMART_INDENT, false);
    
    /* show matching */
    matchingStyle = GetPrefShowMatching();
    set_toggle(matching_w[0], GetPrefShowMatching() == NO_FLASH, false);
    set_toggle(matching_w[1], GetPrefShowMatching() == FLASH_DELIMIT, false);
    set_toggle(matching_w[2], GetPrefShowMatching() == FLASH_RANGE, false);
    set_toggle(syntax_matching_w, GetPrefMatchSyntaxBased(), false);
	    
    /* tag collision */
    set_toggle(tag_collision_w[0], !GetPrefSmartTags(), false);
    set_toggle(tag_collision_w[1], GetPrefSmartTags(), false);
    
    /* search options */
    set_toggle(search_opt_verbose_w, GetPrefSearchDlogs(), false);
    set_toggle(search_opt_wrap_w, GetPrefSearchWraps(), false);
    set_toggle(search_opt_beep_w, GetPrefBeepOnSearchWrap(), false);
    set_toggle(search_opt_keep_dialog_w, GetPrefKeepSearchDlogs(), false);
    searchStyle = GetPrefSearch();
    XtCallActionProc(search_style_w[searchStyle], "ArmAndActivate", 
    	    (XEvent *)0, (String *)0, 0);

#ifdef REPLACE_SCOPE
    replaceStyle = GetPrefReplaceDefScope();
    XtCallActionProc(replace_style_w[replaceStyle], "ArmAndActivate", 
    	    (XEvent *)0, (String *)0, 0);
#endif

    /* initial window size */
    sprintf(winSizeString, "*.w%dx%d",GetPrefRows(), GetPrefCols());
    winSizeWidget =  XtNameToWidget(preferences_dialog, winSizeString);
    if (!winSizeWidget) {
    	winSizeWidget =  XtNameToWidget(preferences_dialog, "*.winCustom");
	SetIntText(window_cust_row, GetPrefRows());
	SetIntText(window_cust_col, GetPrefCols());
    }
    
    XtCallActionProc(winSizeWidget, "ArmAndActivate", 
    	    (XEvent *)0, (String *)0, 0);
	    
    /* tabs settings */
    emTabDist = GetPrefEmTabDist(PLAIN_LANGUAGE_MODE);
    useTabs = GetPrefInsertTabs();
    tabDist = GetPrefTabDist(PLAIN_LANGUAGE_MODE);
    emulate = emTabDist != 0;
    SetIntText(TabDistText, tabDist);
    XmToggleButtonSetState(EmTabToggle, emulate, True);
    if (emulate)
    	SetIntText(EmTabText, emTabDist);
    XmToggleButtonSetState(UseTabsToggle, useTabs, False);
    XtSetSensitive(EmTabText, emulate);

    /* warnings */
    set_toggle(warn_ext_file_mod_w, GetPrefWarnFileMods(), false);
    XtSetSensitive(warn_check_real_file_w, GetPrefWarnFileMods());
    set_toggle(warn_check_real_file_w, GetPrefWarnRealFileMods(), false);
    set_toggle(warn_on_exit_w, GetPrefWarnExit(), false);
}


static int applyCustWindowSize()
{
    int rowValue, colValue, stat;
    
    switch (windowSize) {
      case 0:
	/* get the values that the user entered and make sure they're ok */
	stat = GetIntTextWarn(window_cust_row, &rowValue, "number of rows", True);
	if (stat != TEXT_READ_OK)
    	    return False;
	stat = GetIntTextWarn(window_cust_col, &colValue, "number of columns", True);
	if (stat != TEXT_READ_OK)
    	    return False;
	break;
      case 1:
      	rowValue = 24;
	colValue = 80;
	break;
      case 2:
      	rowValue = 40;
	colValue = 80;
	break;
      case 3:
      	rowValue = 60;
	colValue = 80;
	break;
      case 4:
      	rowValue = 80;
	colValue = 80;
	break;
      default:
      	assert(0);
    }
	
    /* set the corresponding preferences and dismiss the dialog */
    SetPrefRows(rowValue);
    SetPrefCols(colValue);
    return True;
}

static int applyWrapMargin()
{
    int wrapAtWindow, margin, stat;
    
    /* get the values that the user entered and make sure they're ok */
    wrapAtWindow = XmToggleButtonGetState(WrapWindowToggle);
    if (wrapAtWindow)
    	margin = 0;
    else {
	stat = GetIntTextWarn(WrapText, &margin, "wrap Margin", True);
	if (stat != TEXT_READ_OK)
    	    return False;
	if (margin <= 0 || margin >= 1000) {
    	    DialogF(DF_WARN, WrapText, 1, "Wrap margin out of range", "Dismiss");
    	    return False;
	}
    }

    SetPrefWrapMargin(margin);
    return True;
}

static int applyTabSettings()
{
    int emulate, useTabs, stat, tabDist, emTabDist;
    
    emulate = XmToggleButtonGetState(EmTabToggle);
    useTabs = XmToggleButtonGetState(UseTabsToggle);
    stat = GetIntTextWarn(TabDistText, &tabDist, "tab spacing", True);
    if (stat != TEXT_READ_OK)
    	return False;
    if (tabDist <= 0 || tabDist > MAX_EXP_CHAR_LEN) {
    	DialogF(DF_WARN, TabDistText, 1, "Tab spacing out of range", "Dismiss");
    	return False;
    }
    if (emulate) {
	stat = GetIntTextWarn(EmTabText, &emTabDist, "emulated tab spacing",True);
	if (stat != TEXT_READ_OK)
	    return False;
	if (emTabDist <= 0 || tabDist >= 1000) {
	    DialogF(DF_WARN, EmTabText, 1, "Emulated tab spacing out of range",
	    	    "Dismiss");
	    return False;
	}
    } else
    	emTabDist = 0;
    
    SetPrefTabDist(tabDist);
    SetPrefEmTabDist(emTabDist);
    SetPrefInsertTabs(useTabs);
    
    return True;
}

static void removeWindowHookCB(Widget w, XtPointer dialogParent, 
	XtPointer call_data)
{
    /* FIXME: possible recursive behavior? */
    XtRemoveCallback(CallerWindow->shell, XmNdestroyCallback,
	callerDestroyCB, (XtPointer)preferences_dialog);
}

static void dismissPreferencesCB(Widget w, XtPointer dialogParent, 
	XtPointer call_data)
{
}

static void applyPreferencesCB(Widget w, XtPointer dialogParent, 
	XtPointer call_data)
{
    WindowInfo *win;
    int state;
    char *str;
    
    if (applyCustWindowSize() == False)
    	return;
	
    if (applyTabSettings() == False)
    	return;
	
    if (applyWrapMargin() == False)
    	return;
	
    SetPrefStatsLine(XmToggleButtonGetState(show_statsline_w));
    SetPrefISearchLine(XmToggleButtonGetState(show_isearchline_w));
    SetPrefLineNums(XmToggleButtonGetState(show_linenum_w));
    SetPrefMatchSyntaxBased(XmToggleButtonGetState(syntax_matching_w));
    SetPrefHighlightSyntax(XmToggleButtonGetState(highlighting_w[1]));
    SetPrefAutoSave(XmToggleButtonGetState(incremental_backup_w));
    SetPrefSaveOldVersion(XmToggleButtonGetState(make_backup_copy_w));
    SetPrefAppendLF(XmToggleButtonGetState(append_linefeed_w));
    SetPrefRepositionDialogs(XmToggleButtonGetState(popup_under_ptr_w));
    SetPointerCenteredDialogs(XmToggleButtonGetState(popup_under_ptr_w));


    /* backlighting */    
    SetPrefBacklightChars(XmToggleButtonGetState(backlighting_w));
    SetPrefBacklightCharTypes(str = XmTextGetString(backlightCharTypes_w));
    XtFree(str);
    
    /* path in Windows menu */
    state = XmToggleButtonGetState(show_path_w);
    if (state != GetPrefShowPathInWindowsMenu()) {
	SetPrefShowPathInWindowsMenu((state));
	for (win=WindowList; win!=NULL; win=win->next) {
	    win->windowMenuValid = False;
    	    XmToggleButtonSetState(win->pathInWindowsMenuDefItem, state, False);
	}
    }
    
    /* sort prev open menu */
    state = XmToggleButtonGetState(sort_open_prev_w);
    if (state != GetPrefSortOpenPrevMenu()) {
	SetPrefSortOpenPrevMenu(state);
	InvalidatePrevOpenMenus();
    }
    
    SetPrefSmartTags(XmToggleButtonGetState(tag_collision_w[1]));
    SetPrefAutoIndent(indentStyle);
    SetPrefWrap(wrapStyle);
    SetPrefShowMatching(matchingStyle);

    SetPrefSearchDlogs(XmToggleButtonGetState(search_opt_verbose_w));
    SetPrefBeepOnSearchWrap(XmToggleButtonGetState(search_opt_beep_w));
    SetPrefKeepSearchDlogs(XmToggleButtonGetState(search_opt_keep_dialog_w));
    SetPrefSearchWraps(XmToggleButtonGetState(search_opt_wrap_w));
    SetPrefSearch(searchStyle);
#ifdef REPLACE_SCOPE
    SetPrefReplaceDefScope(replaceStyle);
#endif

    SetPrefWarnFileMods(XmToggleButtonGetState(warn_ext_file_mod_w));
    SetPrefWarnRealFileMods(XmToggleButtonGetState(warn_check_real_file_w));
    SetPrefWarnExit(XmToggleButtonGetState(warn_on_exit_w));

    /* disable Apply button to indicate changes applied */
    set_sensitive(apply_button, False);
    set_sensitive(save_button, CheckPrefsChanged());
}

void make_preferences(Widget parent)
{
    Arg args[10];
    int arg = 0;
    Dimension max_width  = 0;
    Dimension max_height = 0;
    Widget box, buttons, frame, change, general_button;
    Widget dummyShell;
    XmString s1;
    
    /* this is a quick hack around the original DDD code:
       we want the preference dialog to be global for all windows,
       so we create a dummy shell as parent to the dialog */
    dummyShell = CreateShellWithBestVis(APP_NAME, APP_CLASS,
    	    applicationShellWidgetClass, TheDisplay, args, arg);
    
    /* Realize the widget in unmapped state */
    XtVaSetValues(dummyShell, XmNmappedWhenManaged, False, NULL);
    XtRealizeWidget(dummyShell);

    arg = 0;
    XtSetArg(args[arg], XmNdialogTitle, s1=MKSTRING("Default Preferences")); arg++;
    preferences_dialog = verify(XmCreateSelectionDialog(dummyShell, 
	    "prefDialog", args, arg));
    XmStringFree(s1);
	    
    /* Delay::register_shell(preferences_dialog); */
    XtVaSetValues(preferences_dialog, XmNdefaultButton, (Widget)(0), NULL);
    XtAddCallback(preferences_dialog, XmNunmapCallback, OfferRestartCB, NULL);
    XtAddCallback(preferences_dialog, XmNunmapCallback, removeWindowHookCB, 
    	    preferences_dialog);

    /* use OK button to save preferences (after applying changes) */
    save_button = 
        XmSelectionBoxGetChild(preferences_dialog, XmDIALOG_OK_BUTTON);
    XtRemoveAllCallbacks(save_button, XmNactivateCallback);
    XtAddCallback(save_button, XmNactivateCallback, applyPreferencesCB, 
    	    preferences_dialog);
    XtAddCallback(save_button, XmNactivateCallback, savePreferencesCB, 
    	    preferences_dialog);

    XtAddCallback(preferences_dialog, XmNapplyCallback, applyPreferencesCB, 
    	    preferences_dialog);
	    
    XtVaSetValues(preferences_dialog, XmNokLabelString,
    	    s1=MKSTRING("Save"), NULL);
    XmStringFree(s1);
    
    XtVaSetValues(preferences_dialog, XmNcancelLabelString,
    	    s1=MKSTRING("Dismiss"), NULL);
    XmStringFree(s1);
    XtAddCallback(preferences_dialog, XmNcancelCallback, dismissPreferencesCB, 
    	    preferences_dialog);
    
    /* remove the extra stuff on the dialog */
    XtUnmanageChild(XmSelectionBoxGetChild(preferences_dialog,
    	    XmDIALOG_TEXT));
    XtUnmanageChild(XmSelectionBoxGetChild(preferences_dialog, 
    	    XmDIALOG_SELECTION_LABEL));
    XtUnmanageChild(XmSelectionBoxGetChild(preferences_dialog, 
    	    XmDIALOG_LIST_LABEL));
    XtUnmanageChild(XmSelectionBoxGetChild(preferences_dialog,
    	    XmDIALOG_LIST));

    /* On Motif 1.2 @ Solaris 2.5, we also need to take out parent
       of the List widget */
    XtUnmanageChild(XtParent(XmSelectionBoxGetChild(preferences_dialog,
    	    XmDIALOG_LIST)));
	    
    arg = 0;
    XtSetArg(args[arg], XmNmarginWidth,  0); arg++;
    XtSetArg(args[arg], XmNmarginHeight, 0); arg++;
    XtSetArg(args[arg], XmNborderWidth,  0); arg++;
    XtSetArg(args[arg], XmNorientation, XmHORIZONTAL); arg++;
    box = verify(XmCreateRowColumn(preferences_dialog, (char *)"box", 
	    args, arg));
    XtManageChild(box);

    /* create container for panel selection buttons */
    if (PanelSelectorType == 0) {
        arg = 0;
        XtSetArg(args[arg], XmNentryAlignment, XmALIGNMENT_BEGINNING); arg++;
        XtSetArg(args[arg], XmNpacking, XmPACK_COLUMN); arg++;
        XtSetArg(args[arg], XmNmarginWidth,  0); arg++;
        XtSetArg(args[arg], XmNmarginHeight, 0); arg++;
        XtSetArg(args[arg], XmNborderWidth,  0); arg++;
        buttons = verify(XmCreateRadioBox(box, (char *)"buttons", args, arg));
    }
    else {
        XmFontList fontList;
        XtVaGetValues(save_button, XmNfontList, &fontList, NULL);

        arg = 0;
        XtSetArg(args[arg], XmNmarginWidth,  1); arg++;
        XtSetArg(args[arg], XmNmarginHeight, 1); arg++;
        XtSetArg(args[arg], XmNborderWidth,  0); arg++;
        XtSetArg(args[arg], XmNvisibleItemCount,  10); arg++;
        XtSetArg(args[arg], XmNfontList,  fontList); arg++;
        buttons = verify(XmCreateScrolledList(box, (char *)"buttons", 
	        args, arg));    
    }
    XtManageChild(buttons);

    arg = 0;
    frame = verify(XmCreateFrame(box, (char *)"frame", args, arg));
    XtManageChild(frame);

    /* create container for panels */
    arg = 0;
    XtSetArg(args[arg], XmNmarginWidth,  0); arg++;
    XtSetArg(args[arg], XmNmarginHeight, 0); arg++;
    XtSetArg(args[arg], XmNborderWidth,  0); arg++;
    change = verify(XmCreateRowColumn(frame, (char *)"change", args, arg));
    XtManageChild(change);

    /* add panels to dialog */
    general_button = add_panel(change, buttons, "General", 
    	    general_preferences_menu, &max_width, &max_height, NULL, false);
    add_panel(change, buttons, "Styles", style_menu, 
    	    &max_width, &max_height, NULL, false);
    add_panel(change, buttons, "Searching", search_menu, 
    	    &max_width, &max_height, "Search options", false);
    add_panel(change, buttons, "Tabs", tabs_menu, 
    	    &max_width, &max_height, "Tabs settings", false);
    add_panel(change, buttons, "Warnings", warnings_menu, 
    	    &max_width, &max_height, NULL, false);
    add_panel(change, buttons, "Customize", customize_menu, 
    	    &max_width, &max_height, NULL, false);

    XtVaSetValues(change,
	XmNwidth, max_width,
	XmNheight, max_height,
	XmNresizeWidth, False,
	XmNresizeHeight, False,
	NULL);

    if (PanelSelectorType == 0) {
        XmToggleButtonSetState(general_button, True, True);
    }
    else {
        XtAddCallback(buttons, XmNbrowseSelectionCallback, listChangePanelCB, 
		(XtPointer)change);
        XmListSelectPos(buttons, 1, True);
    }
    
    /* some other widgets we need to access directly */
    WrapTextLabel = XtNameToWidget(XtParent(WrapText),"label");
    apply_button = XmSelectionBoxGetChild(preferences_dialog,
            XmDIALOG_APPLY_BUTTON);
}

void hide_preferences()
{
    if (!preferences_dialog || !XtIsManaged(preferences_dialog))
    	return;
    
    XtUnmanageChild(preferences_dialog);
    CallerWindow = NULL;
}

static void callerDestroyCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    hide_preferences();
}

void show_preferences(Widget parent)
{
    if (!preferences_dialog)
    	make_preferences(parent);
    
    if (XtIsManaged(preferences_dialog)) {
    	RaiseDialogWindow(XtParent(preferences_dialog));
    } else {
        /* setup the hook to tie dialog to caller window */
    	CallerWindow = WidgetToWindow(parent);
	XtAddCallback(CallerWindow->shell, XmNdestroyCallback,
	    callerDestroyCB, preferences_dialog);

        /* refresh dialog states */
        update_options();
        set_sensitive(apply_button, False);
        set_sensitive(save_button, CheckPrefsChanged());
    
    	ManageDialogCenteredOnPointer(preferences_dialog);
    }
}

void RefreshPrefDialogStates(void)
{
    if (!preferences_dialog || !XtIsManaged(preferences_dialog))
        return;
        
    /* refresh dialog states */
    update_options();
    set_sensitive(apply_button, False);
    set_sensitive(save_button, CheckPrefsChanged());    
}
