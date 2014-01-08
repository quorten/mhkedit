/* Central windows startup stuff */
/* Windows GUI will be used for now */

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0510
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
/* #include <ErrorRep.h> */

#include <stdlib.h>
#include <string.h>
#include "resource.h"
/* #undef EM_REDO
#undef EM_CANREDO
#undef EN_SELCHANGE
#define NO_C_TEDIT */

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#include "bool.h"
#include "Panel.h"
/* #include "FileSysInterface.h" */
/** #include "TextEdit.h" */

#ifndef __GNUC__
#define __attribute__(params)
#endif

/* Note: These are specialized extended dialog template structures
   that are only sophisticated enough to handle the dialogs that
   MhkEdit needs to customize in memory.  */
struct tagDLGTEMPLATEEX
{
	WORD dlgVer;
	WORD signature;
	DWORD helpID;
	DWORD exStyle;
	DWORD style;
	WORD cDlgItems;
	short x;
	short y;
	short cx;
	short cy;
	short menu; /* 0x0000 */
	short windowClass; /* 0x0000 */
	WCHAR title[1]; /* 0x0000 */
	WORD pointsize; /* 8 */
	WORD weight;
	BYTE italic;
	BYTE charset;
	WCHAR typeface[13]; /* "MS Shell Dlg" */
} __attribute__((packed));
typedef struct tagDLGTEMPLATEEX DLGTEMPLATEEX;

struct tagDLGITEMTEMPLATEEX
{
	DWORD helpID;
	DWORD exStyle;
	DWORD style;
	short x;
	short y;
	short cx;
	short cy;
	WORD id;
	WORD windowClass[3]; /* ??? Should be 2, but isn't */
	WCHAR title[1]; /* Need custom parsing for this */
	/* WORD extraCount; /\* Should be zero *\/ */
} __attribute__((packed));
typedef struct tagDLGITEMTEMPLATEEX DLGITEMTEMPLATEEX;

HINSTANCE g_hInstance;
TCHAR stringTable[NUM_STRS][100];
bool draggingDiv = false; /* This is up here for a strange reason (see
						     "Message loop") */
static HWND paramsDlg = NULL;
int mhkGameMode = D_GM_ORLY;

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
	LPARAM lParam);
void InitPopups(HWND parentHwnd, HMENU popup);
INT_PTR CALLBACK AboutBoxProc(HWND hDlg, UINT uMsg, WPARAM wParam,
	LPARAM lParam);
INT_PTR CALLBACK ParamsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam,
	LPARAM lParam);
INT_PTR CALLBACK GameModeProc(HWND hDlg, UINT uMsg, WPARAM wParam,
	LPARAM lParam);
HWND CreateToolBar(HWND hWndParent);
void ConstructParamsDlg(HWND hwnd, LPCTSTR rcIDs[], unsigned numAppend);
void HidePanelWin(HWND hwnd);
void ShowPanelWin(HWND hwnd, HWND before1, HWND before2, HWND before3,
	BOOL horzDiv, int subProps, unsigned oldMoveTo, long divPos);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInst,
					 LPSTR lpCmdLine, int nShowCmd)
{
	HWND hwnd;			/* Window handle */
	WNDCLASSEX wcex;	/* Window class */
	MSG msg;			/* Message structure */
	HACCEL hAccel;		/* Accelerator table */
	/* Temporary variables */
	unsigned i;

	/* Don't allow Windows Error Reporting.  */
	/* AddERExcludedApplication("mhkedit.exe"); */
#ifdef _MSC_VER
	_CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF |
		_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG));
#endif

	/* Register the main window class.  */
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = MainWindowProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)MAIN_ICON);
	wcex.hCursor = NULL;
	wcex.hbrBackground = NULL;
	wcex.lpszMenuName = (LPCTSTR)MAINMENU;
	wcex.lpszClassName = "mainWindow";
	wcex.hIconSm = NULL;

	if (!RegisterClassEx(&wcex))
		return 0;

	/* Register newer text edit window class */
	/** wcex.lpfnWndProc = TextEditProc;
	wcex.lpszClassName = "CustomTextEdit";

	if (!RegisterClassEx(&wcex))
		return 0; */

	/* Load the accelerator table.  */
	hAccel = LoadAccelerators(hInstance, (LPCTSTR)ACCTABLE);

	/* Load the string table.  */
	for (i = M_NEW; i < M_NEW + NUM_STRS; i++)
		LoadString(hInstance, i, stringTable[i-M_NEW], 100 / sizeof(TCHAR));

	/* Initialize tree view and status bar functionality.  */
	InitCommonControls();

	/* Globally save the application instance.  */
	g_hInstance = hInstance;

	/* Create the main window.  */
	hwnd = CreateWindowEx(0, "mainWindow", "MhkEdit",
		WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS,
		CW_USEDEFAULT, CW_USEDEFAULT,	/* window position does not matter */
		CW_USEDEFAULT, CW_USEDEFAULT,	/* window size does not matter */
		NULL, 0, hInstance, NULL);

	if (!hwnd)
		return 0;

	/* Show and draw (update) the window.  */
	ShowWindow(hwnd, nShowCmd);
	UpdateWindow(hwnd);

	/* Message loop */
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (draggingDiv == true && ((msg.message >= WM_KEYFIRST &&
			msg.message <= WM_KEYLAST) ||
			(msg.message >= WM_MOUSEFIRST && msg.message <= WM_MOUSELAST)))
		{
			/* In other programs that have a window divider that you
			   can drag, you may be able to hit the escape key to
			   cancel dragging. Also, other keyboard input unrelated
			   to the dragging operation is ignored, but it seems that
			   the child window that previously had keyboard focus
			   still use display styles as if they had keyboard
			   focus. The best way to solve this problem seems to be
			   to disrupt the normal functioning of a program's main
			   message loop to redirect keyboard messages to the main
			   window. */
			msg.hwnd = hwnd;
			/* We don't need WM_CHAR messages (from TranslateMessage)
			   or accelerator messages (from TranslateAccelerator). */
			DispatchMessage(&msg);
		}
		else
		{
			/* Redirect scrolling keys in the parameters dialog so
			   that the user can still use this program without a
			   mouse.  */
			if (GetParent(msg.hwnd) == paramsDlg &&
				msg.message == WM_KEYDOWN &&
				(msg.wParam == VK_END || msg.wParam == VK_HOME ||
				 msg.wParam == VK_NEXT || msg.wParam == VK_PRIOR ||
				 msg.wParam == VK_DOWN || msg.wParam == VK_UP))
			{
				msg.hwnd = paramsDlg;
				DispatchMessage(&msg);
			}
			else if (!TranslateAccelerator(hwnd, hAccel, &msg) &&
				!IsDialogMessage(paramsDlg, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

#ifdef _MSC_VER
	if (_CrtDumpMemoryLeaks() == FALSE)
		OutputDebugString("No memory leaks occurred.\n");
#endif
	return (int)msg.wParam;
}

/* Window management variables */
static Panel* mainFrame = NULL;
static HWND dataWin;
static HWND treeWin;
static HWND statusWin;
static HWND toolBar;
static HWND childFocus; /* Child window with keyboard focus */
static HWND nextClipViewer; /* Clipboard view chain */
static bool toolbVis = true;
static bool statbVis = true;
static bool treeVis = true;
static bool paramsVis = true;

static bool keySplit = false;
static POINT oldCursorPos;

/*
The main window has a tool bar, a status bar, a treeview side pane,
and a "resource parameters" side pane.  All of these child windows are
managed using the Panel management code in Panel.c.  The resource
parameters dialog is located inside an additional child window to
allow the dialog to be scrolled within its pane.
*/
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
								LPARAM lParam)
{
	static HFONT hFont = NULL;
	switch (uMsg)
	{
	case WM_CREATE:
	{
		CREATESTRUCT* cs;
		HDC hDC;
		RECT rt;
		RECT winRt;
		unsigned statusHeight, toolBarHeight;
		HTREEITEM hPrev;
		TVINSERTSTRUCT tv;

		cs = (CREATESTRUCT*)lParam;

		GetClientRect(hwnd, &winRt);
		/* Add the status bar. */
		mainFrame = CreateFramePanel(&winRt, TRUE,
			SUBPAN_NODIV | SUBPAN_LOCK1);
		statusWin = CreateWindowEx(0,
			STATUSCLASSNAME, NULL, SBARS_SIZEGRIP | WS_CHILD | WS_VISIBLE,
			0, 0, 0, 0,
			hwnd, (HMENU)STATUS_BAR, cs->hInstance, NULL);
		SendMessage(statusWin, SB_SETTEXT, (WPARAM)0, (LPARAM)"Ready");
		GetWindowRect(statusWin, &rt);
		statusHeight = rt.bottom - rt.top;
		mainFrame->divPos = winRt.bottom - statusHeight;
		mainFrame->sub1->panWin = statusWin;
		UpdatePanelSizes(mainFrame);

		/* Add the data window. */
		dataWin = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", NULL,
			WS_CHILD | WS_VISIBLE | WS_VSCROLL/* | WS_HSCROLL */ |
			ES_LEFT | ES_MULTILINE | ES_WANTRETURN | ES_NOHIDESEL,
			0, 0, 0, 0,
			hwnd, (HMENU)DATA_WINDOW, cs->hInstance, NULL);
		/* Set a default font. */
		hDC = GetDC(hwnd);
		hFont = CreateFont(-MulDiv(10, GetDeviceCaps(hDC, LOGPIXELSY), 72),
			0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE,
			ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
		ReleaseDC(hwnd, hDC);
		SendMessage(dataWin, WM_SETFONT, (WPARAM)hFont, (LPARAM)FALSE);
		mainFrame->sub0->panWin = dataWin;
		/* Receive notifications. */
		/* SendMessage(dataWin, EM_SETEVENTMASK, (WPARAM)0,
			(LPARAM)(ENM_SELCHANGE | ENM_MOUSEEVENTS)); */

		/* Add the tree window. */
		InsertPanel(mainFrame->sub0, FALSE, SUBPAN_NORMAL, 1);
		AddTerminalPanel(mainFrame->sub0);
		treeWin = CreateWindowEx(WS_EX_CLIENTEDGE, WC_TREEVIEW, NULL,
			WS_CHILD | WS_VISIBLE | WS_VSCROLL | TVS_HASLINES |
			TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
			0, 0, 0, 0,
			hwnd, (HMENU)TREE_WINDOW, cs->hInstance, NULL);
		mainFrame->sub0->divPos = winRt.right / 4;
		mainFrame->sub0->sub0->panWin = treeWin;
		UpdatePanelSizes(mainFrame->sub0);

		{ /* Add the parameters dialog.  */
			LPCTSTR appendDlgs[] =
				{ (LPCTSTR)TSPR_PARAMS_DLG, (LPCTSTR)TBMP_PARAMS_DLG };
			ConstructParamsDlg(hwnd, appendDlgs, 2);
			GetWindowRect(paramsDlg, &rt);
			rt.right += GetSystemMetrics(SM_CXVSCROLL);
			InsertPanel(mainFrame->sub0, FALSE,
						/* SUBPAN_NODIV | */ SUBPAN_LOCK1, 0);
			AddTerminalPanel(mainFrame->sub0);
			mainFrame->sub0->divPos = winRt.right - (rt.right - rt.left);
			mainFrame->sub0->sub1->panWin = paramsDlg;
			UpdatePanelSizes(mainFrame->sub0);
			ShowWindow(paramsDlg, SW_SHOW);
		}

		/* Add the toolbar */
		InsertPanel(mainFrame->sub0, TRUE, SUBPAN_NODIV | SUBPAN_LOCK0, 1);
		AddTerminalPanel(mainFrame->sub0);
		toolBar = CreateToolBar(hwnd);
		GetWindowRect(toolBar, &rt);
		toolBarHeight = rt.bottom - rt.top;
		mainFrame->sub0->divPos = toolBarHeight;
		mainFrame->sub0->sub0->panWin = toolBar;
		UpdatePanelSizes(mainFrame->sub0);

		SizePanelWindows(mainFrame);

		/* Add this window to the clipboard viewer chain.  */
		nextClipViewer = SetClipboardViewer(hwnd); 

		/* Sample tree hierarchy */
		tv.hParent = NULL;
		tv.hInsertAfter = TVI_LAST;
		tv.item.mask = TVIF_CHILDREN | TVIF_PARAM | TVIF_TEXT;
		tv.item.pszText = "tBMP";
		tv.item.cchTextMax = 5;
		tv.item.cChildren = 1;
		tv.item.lParam = 1101;
		hPrev = TreeView_InsertItem(treeWin, &tv);
		tv.hParent = hPrev;
		tv.item.pszText = "README.txt";
		tv.item.cchTextMax = 11;
		tv.item.cChildren = 0;
		tv.item.lParam = 1102;
		TreeView_InsertItem(treeWin, &tv);
		tv.hParent = hPrev;
		tv.item.pszText = "Misc";
		tv.item.cchTextMax = 5;
		tv.item.cChildren = 1;
		tv.item.lParam = 1103;
		hPrev = TreeView_InsertItem(treeWin, &tv);
		tv.hParent = hPrev;
		tv.item.pszText = "Item 1";
		tv.item.cchTextMax = 7;
		tv.item.cChildren = 0;
		tv.item.lParam = 1104;
		TreeView_InsertItem(treeWin, &tv);
		tv.item.pszText = "Item 2";
		tv.item.cchTextMax = 7;
		tv.item.lParam = 1105;
		TreeView_InsertItem(treeWin, &tv);
		tv.hParent = NULL;
		tv.item.pszText = "tSPR";
		tv.item.cchTextMax = 9;
		tv.item.lParam = 1106;
		hPrev = TreeView_InsertItem(treeWin, &tv);
		tv.hParent = NULL;

		SetFocus(dataWin);
		break;
	}
	case WM_DESTROY:
		ChangeClipboardChain(hwnd, nextClipViewer);
		FreePanels(mainFrame);
		DestroyWindow(paramsDlg);
		DestroyWindow(treeWin);
		DestroyWindow(dataWin);
		DeleteObject(hFont);
		DestroyWindow(statusWin);
		PostQuitMessage(0);
		break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		BeginPaint(hwnd, &ps);
		PaintDividers(mainFrame, ps.hdc);
		EndPaint(hwnd, &ps);
		break;
	}
	case WM_SETCURSOR:
	{
		POINT mousePos;
		RECT rt;
		GetCursorPos(&mousePos);
		ScreenToClient(hwnd, &mousePos);
		GetClientRect(hwnd, &rt);
		if (!PtInRect(&rt, mousePos))
			break;
		if (FrameWinSetCursor(mainFrame, mousePos))
			return TRUE;
		break;
	}
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case K_SWITCHPANES:
		{
			childFocus = GetFocus();
			if (GetParent(childFocus) == paramsDlg)
				childFocus = paramsDlg;
			if (childFocus == dataWin && treeVis == true)
				childFocus = treeWin;
			else if (childFocus == treeWin && paramsVis == true)
				childFocus = paramsDlg;
			else if (childFocus == paramsDlg)
				childFocus = dataWin;
			else
				childFocus = dataWin;
			SetFocus(childFocus);
			break;
		}
		case K_SWITCHPANES_BACK:
		{
			childFocus = GetFocus();
			if (GetParent(childFocus) == paramsDlg)
				childFocus = paramsDlg;
			if (childFocus == dataWin && paramsVis == true)
				childFocus = paramsDlg;
			else if (childFocus == paramsDlg && treeVis == true)
				childFocus = treeWin;
			else if (childFocus == treeWin)
				childFocus = dataWin;
			else
				childFocus = dataWin;
			SetFocus(childFocus);
			break;
		}
			/* File commands */
		case M_NEW:
			MessageBox(hwnd, "HEY!", NULL, MB_OK);
			break;
		case M_OPEN:
			break;
		case M_SAVE:
			break;
		case M_SAVEAS:
			break;
		case M_GAME_MODE:
			DialogBox(g_hInstance, (LPCTSTR)GAME_MODE_DLG,
				hwnd, GameModeProc);
			break;
		case M_EXIT:
			DestroyWindow(hwnd);
			break;
			/* Edit commands */
		case M_UNDO:
			SendMessage(dataWin, WM_UNDO, (WPARAM)0, (LPARAM)0);
			break;
		case M_REDO:
			SendMessage(dataWin, EM_REDO, (WPARAM)0, (LPARAM)0);
			break;
		case M_CUT:
			SendMessage(dataWin, WM_CUT, (WPARAM)0, (LPARAM)0);
			break;
		case M_COPY:
			SendMessage(dataWin, WM_COPY, (WPARAM)0, (LPARAM)0);
			break;
		case M_PASTE:
			SendMessage(dataWin, WM_PASTE, (WPARAM)0, (LPARAM)0);
			break;
		case M_EMPTYCLIP:
			if (OpenClipboard(hwnd))
			{
				EmptyClipboard();
				CloseClipboard();
			}
			else
				MessageBeep(MB_OK);
			break;
		case M_DELETE:
			SendMessage(dataWin, WM_CLEAR, (WPARAM)0, (LPARAM)0);
			break;
		case M_SELECTALL:
			SendMessage(dataWin, EM_SETSEL, (WPARAM)0, (LPARAM)-1);
			break;
		case M_FIND:
			break;
		case M_FINDNEXT:
			MessageBox(hwnd, "HEY!", NULL, MB_OK);
			break;
		case M_REPLACE:
			break;
		/* View commands */
		case M_STATBAR:
			{
				HMENU hMen;
				HMENU hSub;
				hMen = GetMenu(hwnd);
				hSub = GetSubMenu(hMen, M_VIEW_SUBM);
				if (statbVis == true)
				{
					statbVis = false;
					CheckMenuItem(hSub, LOWORD(wParam),
						MF_BYCOMMAND | MF_UNCHECKED);
					HidePanelWin(statusWin);
				}
				else
				{
					RECT rt;
					unsigned statusHeight;
					statbVis = true;
					CheckMenuItem(hSub, LOWORD(wParam),
						MF_BYCOMMAND | MF_CHECKED);
					GetWindowRect(statusWin, &rt);
					statusHeight = rt.bottom - rt.top;
					ShowPanelWin(statusWin, NULL, NULL, NULL,
						TRUE, SUBPAN_NODIV | SUBPAN_LOCK1, 0,
						-statusHeight);
				}
				break;
			}
		case M_TOOLBAR:
			{
				HMENU hMen;
				HMENU hSub;
				hMen = GetMenu(hwnd);
				hSub = GetSubMenu(hMen, M_VIEW_SUBM);
				if (toolbVis == true)
				{
					toolbVis = false;
					CheckMenuItem(hSub, LOWORD(wParam),
						MF_BYCOMMAND | MF_UNCHECKED);
					HidePanelWin(toolBar);
				}
				else
				{
					RECT rt;
					unsigned toolBarHeight;
					toolbVis = true;
					CheckMenuItem(hSub, LOWORD(wParam),
						MF_BYCOMMAND | MF_CHECKED);
					GetWindowRect(toolBar, &rt);
					toolBarHeight = rt.bottom - rt.top;
					ShowPanelWin(toolBar, statusWin, NULL, NULL,
						TRUE, SUBPAN_NODIV | SUBPAN_LOCK0, 1, toolBarHeight);
				}
				break;
			}
		case M_RSRC_PARAMS:
			{
				HMENU hMen;
				HMENU hSub;
				hMen = GetMenu(hwnd);
				hSub = GetSubMenu(hMen, M_VIEW_SUBM);
				if (paramsVis == true)
				{
					paramsVis = false;
					if (GetParent(GetFocus()) == paramsDlg)
					{
						childFocus = dataWin;
						SetFocus(childFocus);
					}
					CheckMenuItem(hSub, LOWORD(wParam),
						MF_BYCOMMAND | MF_UNCHECKED);
					HidePanelWin(paramsDlg);
				}
				else
				{
					RECT rt;
					unsigned paramsDlgWidth;
					paramsVis = true;
					CheckMenuItem(hSub, LOWORD(wParam),
						MF_BYCOMMAND | MF_CHECKED);
					GetWindowRect(paramsDlg, &rt);
					paramsDlgWidth = rt.right - rt.left;
					ShowPanelWin(paramsDlg, toolBar, statusWin, NULL,
						FALSE, /* SUBPAN_NODIV | */ SUBPAN_LOCK1, 0,
						-paramsDlgWidth);
				}
				break;
			}
		case M_TREE:
			{
				HMENU hMen;
				HMENU hSub;
				hMen = GetMenu(hwnd);
				hSub = GetSubMenu(hMen, M_VIEW_SUBM);
				if (treeVis == true)
				{
					treeVis = false;
					if (GetFocus() == treeWin)
					{
						childFocus = dataWin;
						SetFocus(childFocus);
					}
					CheckMenuItem(hSub, LOWORD(wParam),
						MF_BYCOMMAND | MF_UNCHECKED);
					HidePanelWin(treeWin);
				}
				else
				{
					RECT rt;
					unsigned treeWidth;
					treeVis = true;
					CheckMenuItem(hSub, LOWORD(wParam),
						MF_BYCOMMAND | MF_CHECKED);
					GetWindowRect(treeWin, &rt);
					treeWidth = rt.right - rt.left;
					ShowPanelWin(treeWin, paramsDlg, toolBar, statusWin,
						FALSE, SUBPAN_NORMAL, 1, treeWidth);
				}
				break;
			}
		case M_SPLIT:
			draggingDiv = true;
			/* KeyDividerDrag(frameWin); */
			break;
		case M_FONT:
		{
			CHOOSEFONT cf;
			LOGFONT logFnt;
			GetObject(hFont, sizeof(LOGFONT), &logFnt);
			cf.lStructSize = sizeof(CHOOSEFONT);
			cf.hwndOwner = hwnd;
			cf.hDC = NULL;
			cf.lpLogFont = &logFnt;
			cf.iPointSize = 0;
			if (hFont == NULL)
				cf.Flags = CF_SCREENFONTS;
			else
				cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS;
			cf.rgbColors = (COLORREF)0x00000000;
			cf.lCustData = 0;
			cf.lpfnHook = NULL;
			cf.lpTemplateName = NULL;
			cf.hInstance = NULL;
			cf.lpszStyle = NULL;
			cf.nFontType = 0;
			cf.nSizeMin = 0;
			cf.nSizeMax = 0;
			if (ChooseFont(&cf))
			{
				DeleteObject(hFont);
				hFont = CreateFontIndirect(cf.lpLogFont);
				SendMessage(dataWin, WM_SETFONT, (WPARAM)hFont, (LPARAM)TRUE);
			}
			break;
		}
			/* Help commands */
		case M_HELP:
			break;
		case M_ABOUT:
			DialogBox(g_hInstance, (LPCTSTR)ABOUTBOX, hwnd, AboutBoxProc);
			break;
		}
		break;
	case WM_ENTERMENULOOP:
		/* SendMessage(statusWin, SB_SIMPLE, (WPARAM)TRUE, (LPARAM)0); */
		break;
	case WM_INITMENUPOPUP:
		InitPopups(hwnd, (HMENU)wParam);
		break;
	case WM_MENUSELECT:
		if (LOWORD(wParam) >= M_NEW)
		{
			SendMessage(statusWin, SB_SETTEXT, (WPARAM)0,
				(LPARAM)stringTable[LOWORD(wParam)-M_NEW]);
		}
		else
		{
			SendMessage(statusWin, SB_SETTEXT, (WPARAM)0, (LPARAM)0);
		}
		break;
	case WM_EXITMENULOOP:
		/* SendMessage(statusWin, SB_SIMPLE, (WPARAM)FALSE, (LPARAM)0); */
		SendMessage(statusWin, SB_SETTEXT, (WPARAM)0, (LPARAM)"Ready");
		break;
	case WM_NOTIFY:
	{
		NMHDR* notHead;
		notHead = (NMHDR*)lParam;
		if (notHead->code == TVN_SELCHANGED)
		{
			NMTREEVIEW* pnmtv;
			pnmtv = (NMTREEVIEW*)lParam;
			/* MessageBox(NULL, "BOO!", NULL, MB_OK); */
			/* FSSOnChangeSelection(pnmtv->itemNew.pszText, dataWin); */
		}
		if (notHead->code == TTN_GETDISPINFO)
		{
			/* Just give the address of the pre-loaded strings */
			NMTTDISPINFO* lpnmtdi;
			UINT_PTR btnId;
			unsigned strIdx;
			lpnmtdi = (NMTTDISPINFO*)lParam;
			btnId = lpnmtdi->hdr.idFrom;
			switch (btnId)
			{
			case M_NEW: strIdx = T_NEW; break;
			case M_OPEN: strIdx = T_OPEN; break;
			case M_SAVE: strIdx = T_SAVE; break;
			case M_PRINT: strIdx = T_PRINT; break;
			case M_PRINT_PREV: strIdx = T_PRINTPRE; break;
			case M_CUT: strIdx = T_CUT; break;
			case M_COPY: strIdx = T_COPY; break;
			case M_PASTE: strIdx = T_PASTE; break;
			case M_UNDO: strIdx = T_UNDO; break;
			case M_REDO: strIdx = T_REDO; break;
			}
			lpnmtdi->hinst = NULL;
			lpnmtdi->lpszText = stringTable[strIdx-M_NEW];
		}
		/* if (notHead->code == EN_SELCHANGE)
		{
			SELCHANGE* selChange;
			selChange = (SELCHANGE*)lParam;
			if (selChange->chrg.cpMin == selChange->chrg.cpMax)
			{
				SendMessage(toolBar, TB_ENABLEBUTTON, M_CUT, 0);
				SendMessage(toolBar, TB_ENABLEBUTTON, M_COPY, 0);
			}
			else
			{
				SendMessage(toolBar, TB_ENABLEBUTTON, M_CUT, 1);
				SendMessage(toolBar, TB_ENABLEBUTTON, M_COPY, 1);
			}
		} */
		/* if (notHead->code == EN_MSGFILTER)
		{
			MSGFILTER* msgFilt;
			msgFilt = (MSGFILTER*)lParam;
			if (msgFilt->msg == WM_RBUTTONUP)
			{
				HMENU hMaster;
				HMENU hMen;
				POINT pt;
				hMaster = LoadMenu(g_hInstance, (LPCTSTR)SHRTCMENUS);
				hMen = GetSubMenu(hMaster, 0);
				pt.x = (short)LOWORD(msgFilt->lParam);
				pt.y = (short)HIWORD(msgFilt->lParam);
				pt.x = mainFrame->sub0->sub1->sub0->sub1->dims.left + pt.x;
				pt.y = mainFrame->sub0->sub1->sub0->sub1->dims.top + pt.y;
				ClientToScreen(hwnd, &pt);

				/\* TODO: Get item states *\/
				/\** if (!IsClipboardFormatAvailable(CF_TEXT))
					EnableMenuItem(hMen, M_PASTE, MF_BYCOMMAND | MF_GRAYED);
				if (regionActive == false)
				{
					EnableMenuItem(hMen, M_CUT, MF_BYCOMMAND | MF_GRAYED);
					EnableMenuItem(hMen, M_COPY, MF_BYCOMMAND | MF_GRAYED);
					EnableMenuItem(hMen, M_DELETE, MF_BYCOMMAND | MF_GRAYED);
				}
				if (curUndo == 0)
					EnableMenuItem(hMen, M_UNDO, MF_BYCOMMAND | MF_GRAYED);
				if (curUndo == numUndo - 1)
					EnableMenuItem(hMen, M_REDO, MF_BYCOMMAND | MF_GRAYED);
				if (textSize == 0)
					EnableMenuItem(hMen, M_SELECTALL, MF_BYCOMMAND | MF_GRAYED); *\/
				TrackPopupMenu(hMen, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
					pt.x, pt.y, 0, hwnd, NULL);
				DestroyMenu(hMaster);
			}
		} */
		break;
	}
	case WM_SETFOCUS:
		if (GetFocus() == hwnd)
			SetFocus(childFocus);
		break;
	case WM_ACTIVATE:
		if (LOWORD(wParam) != WA_ACTIVE)
		{
			childFocus = GetFocus();
			if (childFocus == hwnd)
			{
				SetFocus(dataWin);
				childFocus = dataWin;
			}
		}
		break;
	case WM_KEYDOWN:
	{
		POINT pt;
		RECT rt;
		GetCursorPos(&pt);
		/* if (keySplit == true)
		{
			switch (wParam)
			{
			case VK_LEFT:
				ScreenToClient(hwnd, &pt);
				pt.x -= dividerWidth / 2;
				GetClientRect(hwnd, &rt);
				rt.left += divDragPt + vScrollWidth * 2;
				rt.top += divDragPt + vScrollWidth * 2;
				rt.right -= divDragPt + vScrollWidth * 2;
				rt.bottom -= divDragPt + vScrollWidth * 2;
				if (rt.left <= pt.x && pt.x < rt.right)
				{
					ClientToScreen(hwnd, &pt);
					SetCursorPos(pt.x, pt.y);
				}
				break;
			case VK_RIGHT:
				ScreenToClient(hwnd, &pt);
				pt.x += dividerWidth / 2;
				GetClientRect(hwnd, &rt);
				rt.left += divDragPt + vScrollWidth * 2;
				rt.top += divDragPt + vScrollWidth * 2;
				rt.right -= divDragPt + vScrollWidth * 2;
				rt.bottom -= divDragPt + vScrollWidth * 2;
				if (rt.left <= pt.x && pt.x < rt.right)
				{
					ClientToScreen(hwnd, &pt);
					SetCursorPos(pt.x, pt.y);
				}
				break;
			case VK_RETURN:
				keySplit = false;
				draggingDiv = false;
				EndDivDrag(hwnd);
				break;
			}
		} */
		if (wParam == VK_ESCAPE)
		{
			if (keySplit == true)
			{
				keySplit = false;
				SetCursorPos(oldCursorPos.x, oldCursorPos.y);
			}
			ReleaseCapture();
		}
		break;
	}
	case WM_LBUTTONDOWN:
	{
		if (DispatchDividerDrag(mainFrame, hwnd, MAKEPOINTS(lParam)))
			draggingDiv = true;
		if (keySplit == true)
		{
			SetCursorPos(oldCursorPos.x, oldCursorPos.y);
			keySplit = false;
			draggingDiv = false;
			EndDivDrag(hwnd, FALSE);
		}
		break;
	}
	case WM_RBUTTONDOWN:
		break;
	case WM_CAPTURECHANGED:
		if ((HWND)lParam != hwnd)
		{
			if (draggingDiv == true)
			{
				draggingDiv = false;
				if (keySplit == true)
					SetCursorPos(oldCursorPos.x, oldCursorPos.y);
				EndDivDrag(hwnd, TRUE);
				InvalidateRect(hwnd, NULL, TRUE);
			}
			if (keySplit == true)
			{
				keySplit = false;
			}
			return 0;
		}
	case WM_MOUSEMOVE:
		if (draggingDiv == true)
		{
			POINT pt;
			pt.x = (short)LOWORD(lParam);
			pt.y = (short)HIWORD(lParam);
			ProcDivDrag(hwnd, pt.x, pt.y);
		}
		break;
	case WM_LBUTTONUP:
		if (draggingDiv == true)
		{
			keySplit = false;
			draggingDiv = false;
			EndDivDrag(hwnd, FALSE);
		}
		break;
	case WM_RBUTTONUP:
		break;
	case WM_MOVE:
		/* SendMessage(dataWin, WM_MOVE, (WPARAM)0, (LPARAM)0); */
		break;
	case WM_SIZE:
		GetClientRect(hwnd, &mainFrame->dims);
		ScalePanels(mainFrame);
		SizePanelWindows(mainFrame);
		break;
	/* Messages to keep the toolbar up to date.  */
	case EN_SELCHANGE:
		SendMessage(toolBar, TB_ENABLEBUTTON, M_CUT, lParam);
		SendMessage(toolBar, TB_ENABLEBUTTON, M_COPY, lParam);
		break;
	case NM_CANUNDO:
		if (wParam == 0)
			SendMessage(toolBar, TB_ENABLEBUTTON, M_UNDO, lParam);
		else
			SendMessage(toolBar, TB_ENABLEBUTTON, M_REDO, lParam);
		break;
	case WM_CHANGECBCHAIN:
		/* If the next window is closing, repair the chain.  */
		if ((HWND)wParam == nextClipViewer) 
			nextClipViewer = (HWND)lParam; 
		/* Otherwise, pass the message to the next link.  */
		else if (nextClipViewer != NULL) 
			SendMessage(nextClipViewer, uMsg, wParam, lParam); 
		break;
	case WM_DRAWCLIPBOARD:
		if (IsClipboardFormatAvailable(CF_TEXT))
			SendMessage(toolBar, TB_ENABLEBUTTON, M_PASTE, TRUE);
		else
			SendMessage(toolBar, TB_ENABLEBUTTON, M_PASTE, FALSE);
		break;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void InitPopups(HWND parentHwnd, HMENU popup)
{
	int numMenItems;
	UINT id;
	unsigned regBegin;
	unsigned regEnd;
	bool switches[8];
	const unsigned menIDs[8] =
		{ M_UNDO, M_REDO, M_CUT, M_COPY, M_DELETE, M_PASTE,
		  M_EMPTYCLIP, M_SELECTALL };
	const unsigned numSwitches = 8;
	/* Temporary variables */
	unsigned i;

	numMenItems = GetMenuItemCount(popup);
	if (numMenItems == 0)
		return;

	/* Initialize the edit menu */
	id = GetMenuItemID(popup, 0);
	if (id != M_UNDO)
		return;

	/* Undo */
	if (SendMessage(dataWin, EM_CANUNDO, 0, 0) == FALSE)
		switches[0] = false;
	else switches[0] = true;

	/* Redo */
	if (SendMessage(dataWin, EM_CANREDO, 0, 0) == FALSE)
		switches[1] = false;
	else switches[1] = true;

	/* Cut, copy, and delete */
	SendMessage(dataWin, EM_GETSEL, (WPARAM)&regBegin, (LPARAM)&regEnd);
	if (regBegin == regEnd)
	{
		switches[2] = false;
		switches[3] = false;
		switches[4] = false;
	}
	else
	{
		switches[2] = true;
		switches[3] = true;
		switches[4] = true;
	}

	/* Paste */
	if (IsClipboardFormatAvailable(CF_TEXT))
		switches[5] = true;
	else switches[5] = false;

	/* Empty clipboard */
	if (OpenClipboard(parentHwnd))
	{
		if (EnumClipboardFormats(0) == 0)
			switches[6] = false;
		else switches[6] = true;
		CloseClipboard();
	}

	/* Select all */
	if (GetWindowTextLength(dataWin) == 0)
		switches[7] = false;
	else switches[7] = true;

	for (i = 0; i < numSwitches; i++)
	{
		UINT dispCmd;
		if (switches[i] == false)
			dispCmd = MF_GRAYED;
		else dispCmd = MF_ENABLED;
		EnableMenuItem(popup, menIDs[i], MF_BYCOMMAND | dispCmd);
	}
}

INT_PTR CALLBACK AboutBoxProc(HWND hDlg, UINT uMsg, WPARAM wParam,
	LPARAM lParam)
{
	HWND hwndOwner;
	RECT rc, rcDlg, rcOwner;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		/* Center the dialog in the parent window */
		/* Get the owner window and dialog box rectangles */
		if ((hwndOwner = GetParent(hDlg)) == NULL)
		{
			hwndOwner = GetDesktopWindow();
		}

		GetWindowRect(hwndOwner, &rcOwner);
		GetWindowRect(hDlg, &rcDlg);
		CopyRect(&rc, &rcOwner);

		/* Offset the owner and dialog box rectangles so that
		   right and bottom values represent the width and
		   height, and then offset the owner again to discard
		   space taken up by the dialog box. */
		OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top);
		OffsetRect(&rc, -rc.left, -rc.top);
		OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom);

		/* The new position is the sum of half the remaining
		   space and the owner's original position. */
		SetWindowPos(hDlg,
			HWND_TOP,
			rcOwner.left + (rc.right / 2),
			rcOwner.top + (rc.bottom / 2),
			0, 0,          /* ignores size arguments */
			SWP_NOSIZE);
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}

INT_PTR CALLBACK ParamsDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam,
	LPARAM lParam)
{
	static RECT fullRect;
	static unsigned pageSize;
	static long fntHeight;

	switch (uMsg)
	{
	case WM_INITDIALOG:
	{
		HDC hDC;
		TEXTMETRIC tm;
		GetClientRect(hDlg, &fullRect);
		hDC = GetDC(hDlg);
		GetTextMetrics(hDC, &tm);
		fntHeight = tm.tmHeight;
		ReleaseDC(hDlg, hDC);
		EnableWindow(GetDlgItem(hDlg, D_RSRC_NAME), FALSE);
		break;
	}

	/* Scrollbar handling */
	case WM_SIZE:
	{
		RECT newRect = { 0, 0, LOWORD(lParam), HIWORD(lParam) };
		SCROLLINFO si;
		pageSize = newRect.bottom;
		si.cbSize = sizeof(SCROLLINFO);
		si.fMask = SIF_DISABLENOSCROLL | SIF_PAGE | SIF_RANGE;
		si.nMin = 0;
		/* Max position never equals height.  */
		si.nMax = fullRect.bottom - 1;
		si.nPage = pageSize;
		SetScrollInfo(hDlg, SB_VERT, &si, TRUE);
		break;
	}
	case WM_VSCROLL:
	{
		SCROLLINFO si;
		int oldPos;
		unsigned thumbPos = HIWORD(wParam);
		HDC hDC;
		si.cbSize = sizeof(SCROLLINFO);
		si.fMask = SIF_POS;
		GetScrollInfo(hDlg, SB_VERT, &si);
		oldPos = si.nPos;
		si.fMask = SIF_POS;
		switch(LOWORD(wParam))
		{
		case SB_BOTTOM: si.nPos = fullRect.bottom - pageSize; break;
		case SB_TOP: si.nPos = 0; break;
		case SB_LINEDOWN: si.nPos += fntHeight; break;
		case SB_LINEUP:   si.nPos -= fntHeight; break;
		case SB_PAGEDOWN: si.nPos += pageSize; break;
		case SB_PAGEUP:   si.nPos -= pageSize; break;
		case SB_THUMBPOSITION: si.nPos = thumbPos; break;
		case SB_THUMBTRACK:    si.nPos = thumbPos; break;
		case SB_ENDSCROLL: break;
		}
		if (si.nPos < 0)
			si.nPos = 0;
		else if (si.nPos > fullRect.bottom - pageSize)
			si.nPos = fullRect.bottom - pageSize;
		SetScrollInfo(hDlg, SB_VERT, &si, TRUE);
		{
			int scrollAmount = oldPos - si.nPos;
			if (scrollAmount != 0)
				ScrollWindowEx(hDlg, 0, scrollAmount, NULL, NULL, NULL, NULL,
					SW_ERASE | SW_INVALIDATE | SW_SCROLLCHILDREN);
		}
		return TRUE;
	}
	case WM_KEYDOWN:
	{
		int scrollOp = SB_ENDSCROLL;
		switch (wParam)
		{
		case VK_END: scrollOp = SB_BOTTOM; break;
		case VK_HOME: scrollOp = SB_TOP; break;
		case VK_NEXT: scrollOp = SB_PAGEDOWN; break;
		case VK_PRIOR: scrollOp = SB_PAGEUP; break;
		case VK_DOWN: scrollOp = SB_LINEDOWN; break;
		case VK_UP: scrollOp = SB_LINEUP; break;
		}
		if (scrollOp != SB_ENDSCROLL)
			return SendMessage(hDlg, WM_VSCROLL, scrollOp, 0);
		break;
	}
	case WM_MOUSEWHEEL:
	{
		int scrollAmount =
			(-((int)(short)HIWORD(wParam)) * fntHeight * 3) / WHEEL_DELTA;
		if (scrollAmount != 0)
		{
			SCROLLINFO si;
			short scrollPos;
			si.cbSize = sizeof(SCROLLINFO);
			si.fMask = SIF_POS;
			GetScrollInfo(hDlg, SB_VERT, &si);
			scrollPos = si.nPos + scrollAmount;
			if (scrollPos < 0)
				scrollPos = 0;
			return SendMessage(hDlg, WM_VSCROLL,
				MAKEWPARAM(SB_THUMBPOSITION, scrollPos), 0);
		}
		break;
	}
	/* END scrollbar handling */

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		/* General Parameters */
		case D_HAS_RSRC_NAME:
			if (IsDlgButtonChecked(hDlg, D_HAS_RSRC_NAME))
				EnableWindow(GetDlgItem(hDlg, D_RSRC_NAME), TRUE);
			else
			{
				HWND hItem = GetDlgItem(hDlg, D_RSRC_NAME);
				EnableWindow(hItem, FALSE);
				SetWindowText(hItem, "");
			}
			break;

		/* Bitmap parameters */

		/* Sprite parameters */

		/* Palette parameters */
		}
		break;
	}
	return FALSE;
}

INT_PTR CALLBACK GameModeProc(HWND hDlg, UINT uMsg, WPARAM wParam,
	LPARAM lParam)
{
	static int newGameMode;
	switch (uMsg)
	{
	case WM_INITDIALOG:
		CheckDlgButton(hDlg, mhkGameMode, BST_CHECKED);
		newGameMode = mhkGameMode;
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case D_GM_NONE:
		case D_GM_ORLY:
			newGameMode = LOWORD(wParam);
			break;
		case IDOK:
			mhkGameMode = newGameMode;
			/* fall through */
		case IDCANCEL:
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}

/* CreateToolBar creates a toolbar and adds bitmaps to it.  The
   function returns the handle to the toolbar if successful, or NULL
   otherwise. */
HWND CreateToolBar(HWND hWndParent)
{
	TBBUTTON tbButtonsCreate[13];
	int i = 0;
	/* TBBUTTON tbButtonsAdd[4]; */
	HWND hWndToolbar;
	/* TBADDBITMAP tb;
	int index, stdidx; */

	ZeroMemory(&tbButtonsCreate, sizeof(tbButtonsCreate));
	tbButtonsCreate[i].iBitmap = STD_FILENEW;
	tbButtonsCreate[i].idCommand = M_NEW;
	tbButtonsCreate[i].fsState = TBSTATE_ENABLED;
	tbButtonsCreate[i++].fsStyle = TBSTYLE_BUTTON /* BTNS_BUTTON */;

	tbButtonsCreate[i].iBitmap = STD_FILEOPEN;
	tbButtonsCreate[i].idCommand = M_OPEN;
	tbButtonsCreate[i].fsState = TBSTATE_ENABLED;
	tbButtonsCreate[i++].fsStyle = TBSTYLE_BUTTON /* BTNS_BUTTON */;

	tbButtonsCreate[i].iBitmap = STD_FILESAVE;
	tbButtonsCreate[i].idCommand = M_SAVE;
	tbButtonsCreate[i].fsState = TBSTATE_ENABLED;
	tbButtonsCreate[i++].fsStyle = TBSTYLE_BUTTON /* BTNS_BUTTON */;

	tbButtonsCreate[i].iBitmap = 0;
	tbButtonsCreate[i].idCommand = 0;
	tbButtonsCreate[i].fsState = TBSTATE_ENABLED;
	tbButtonsCreate[i++].fsStyle = TBSTYLE_SEP /* BTNS_SEP */;

	tbButtonsCreate[i].iBitmap = STD_PRINT;
	tbButtonsCreate[i].idCommand = M_PRINT;
	tbButtonsCreate[i].fsState = TBSTATE_ENABLED;
	tbButtonsCreate[i++].fsStyle = TBSTYLE_BUTTON /* BTNS_BUTTON */;

	tbButtonsCreate[i].iBitmap = STD_PRINTPRE;
	tbButtonsCreate[i].idCommand = M_PRINT_PREV;
	tbButtonsCreate[i].fsState = TBSTATE_ENABLED;
	tbButtonsCreate[i++].fsStyle = TBSTYLE_BUTTON /* BTNS_BUTTON */;

	tbButtonsCreate[i].iBitmap = 0;
	tbButtonsCreate[i].idCommand = 0;
	tbButtonsCreate[i].fsState = TBSTATE_ENABLED;
	tbButtonsCreate[i++].fsStyle = TBSTYLE_SEP /* BTNS_SEP */;

	tbButtonsCreate[i].iBitmap = STD_CUT;
	tbButtonsCreate[i].idCommand = M_CUT;
	tbButtonsCreate[i].fsState = 0;
	tbButtonsCreate[i++].fsStyle = TBSTYLE_BUTTON /* BTNS_BUTTON */;

	tbButtonsCreate[i].iBitmap = STD_COPY;
	tbButtonsCreate[i].idCommand = M_COPY;
	tbButtonsCreate[i].fsState = 0;
	tbButtonsCreate[i++].fsStyle = TBSTYLE_BUTTON /* BTNS_BUTTON */;

	tbButtonsCreate[i].iBitmap = STD_PASTE;
	tbButtonsCreate[i].idCommand = M_PASTE;
	tbButtonsCreate[i].fsState = TBSTATE_ENABLED;
	tbButtonsCreate[i++].fsStyle = TBSTYLE_BUTTON /* BTNS_BUTTON */;

	tbButtonsCreate[i].iBitmap = 0;
	tbButtonsCreate[i].idCommand = 0;
	tbButtonsCreate[i].fsState = TBSTATE_ENABLED;
	tbButtonsCreate[i++].fsStyle = TBSTYLE_SEP /* BTNS_SEP */;

	tbButtonsCreate[i].iBitmap = STD_UNDO;
	tbButtonsCreate[i].idCommand = M_UNDO;
	tbButtonsCreate[i].fsState = 0;
	tbButtonsCreate[i++].fsStyle = TBSTYLE_BUTTON /* BTNS_BUTTON */;

	tbButtonsCreate[i].iBitmap = STD_REDOW;
	tbButtonsCreate[i].idCommand = M_REDO;
	tbButtonsCreate[i].fsState = 0;
	tbButtonsCreate[i++].fsStyle = TBSTYLE_BUTTON /* BTNS_BUTTON */;

	/* tbButtonsCreate[12].iBitmap = 0;
	tbButtonsCreate[12].idCommand = 0;
	tbButtonsCreate[12].fsState = TBSTATE_ENABLED;
	tbButtonsCreate[12].fsStyle = BTNS_SEP;

	ZeroMemory(&tbButtonsAdd, sizeof(tbButtonsAdd));
	tbButtonsAdd[0].iBitmap = VIEW_LARGEICONS;
	tbButtonsAdd[0].idCommand = IDM_LARGEICON;
	tbButtonsAdd[0].fsState = TBSTATE_ENABLED;
	tbButtonsAdd[0].fsStyle = BTNS_BUTTON;

	tbButtonsAdd[1].iBitmap = VIEW_SMALLICONS;
	tbButtonsAdd[1].idCommand = IDM_SMALLICON;
	tbButtonsAdd[1].fsState = TBSTATE_ENABLED;
	tbButtonsAdd[1].fsStyle = BTNS_BUTTON;

	tbButtonsAdd[2].iBitmap = VIEW_LIST;
	tbButtonsAdd[2].idCommand = IDM_LISTVIEW;
	tbButtonsAdd[2].fsState = TBSTATE_ENABLED;
	tbButtonsAdd[2].fsStyle = BTNS_BUTTON;

	tbButtonsAdd[3].iBitmap = VIEW_DETAILS;
	tbButtonsAdd[3].idCommand = IDM_REPORTVIEW;
	tbButtonsAdd[3].fsState = TBSTATE_ENABLED;
	tbButtonsAdd[3].fsStyle = BTNS_BUTTON; */

	//tb.hInst = HINST_COMMCTRL;
	//tb.nID = IDB_STD_SMALL_COLOR;

	hWndToolbar = CreateToolbarEx (hWndParent, 
		WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS /* | TBSTYLE_FLAT */,
		ID_TOOLBAR, 0, HINST_COMMCTRL, IDB_STD_SMALL_COLOR, 
		tbButtonsCreate, i, 0, 0, 100, 30, sizeof (TBBUTTON));
	/*hWndToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
		WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT,
		0, 0, 0, 0, hWndParent, (HMENU)ID_TOOLBAR, g_hInstance, NULL);
	SendMessage(hWndToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
	SendMessage(hWndToolbar, TB_ADDBITMAP, 0, &tbBitmaps);
	SendMessage(hWndToolbar, TB_ADDBUTTONS, 10, (LPARAM)&tbButtonsCreate);*/

	/* Add MhkEdit bitmaps */
	/*tb.hInst = HINST_COMMCTRL;
	tb.nID = IDB_VIEW_SMALL_COLOR;*/
	/* There are 12 items in IDB_VIEW_SMALL_COLOR.  However, because
	   this is a standard system-defined bitmap, wParam (nButtons) is
	   ignored. */
	/*stdidx = SendMessage (hWndToolbar, TB_ADDBITMAP, 0, (LPARAM)&tb);*/

	/* Update the indexes to the view bitmaps. */
	/*for (index = 0; index < 4; index++)
		tbButtonsAdd[index].iBitmap += stdidx;*/

	/* Add the MhkEdit buttons */
	/*SendMessage (hWndToolbar, TB_ADDBUTTONS, 4, (LPARAM) &tbButtonsAdd[0]);*/

	return hWndToolbar;
}

/* The parameters dialog bar is constructed by dynamically creating a
   dialog template in memory by vertically stacking the input dialog
   templates.  WARNING: Do NOT call this function will numAppend set
   to a value greater than 5.  */
void ConstructParamsDlg(HWND hwnd, LPCTSTR rcIDs[], unsigned numAppend)
{
	/* Start by loading the general parameters box.  */
	HRSRC hGpDlg = FindResource(NULL, (LPCTSTR)GEN_PARAMS_DLG,
								RT_DIALOG);
	DLGTEMPLATE* gpDlg =
		(DLGTEMPLATE*)LockResource(LoadResource(NULL, hGpDlg));
	DWORD gpSz = SizeofResource(NULL, hGpDlg);
	unsigned i;

	HRSRC hrcDlgs[5];
	DLGTEMPLATE* dlgTmpls[5];
	DWORD tmplSzs[5];
	DWORD resDlgSz = gpSz;

	DLGTEMPLATE* paramsDlgTmpl;
	BYTE* appendAddr;
	unsigned padding;
	DLGTEMPLATEEX* header;

	/* Load the additional dialog boxes.  */
	for (i = 0; i < numAppend; i++)
	{
		hrcDlgs[i] = FindResource(NULL, (LPCTSTR)rcIDs[i], RT_DIALOG);
		dlgTmpls[i] = (DLGTEMPLATE*)LockResource(
			LoadResource(NULL, hrcDlgs[i]));
		tmplSzs[i] = SizeofResource(NULL, hrcDlgs[i]);
		tmplSzs[i] -= sizeof(DLGTEMPLATEEX);
		resDlgSz += tmplSzs[i] + 4; /* Leave room for DWORD alignment */
	}

	/* Now generate a new dialog that is basically the input dialogs
	   stacked one on top of another.  A scrollbar is also added to
	   the resulting dialog.  */
	paramsDlgTmpl = (DLGTEMPLATE*)malloc(resDlgSz);
	header = (DLGTEMPLATEEX*)paramsDlgTmpl;
	appendAddr = (BYTE*)paramsDlgTmpl;

	/* Copy the general parameters box directly into paramsDlgTmpl.  */
	memcpy(paramsDlgTmpl, gpDlg, gpSz);
	appendAddr += gpSz;
	padding = sizeof(DWORD) - gpSz % sizeof(DWORD);
	if (padding != sizeof(DWORD))
		appendAddr += padding;
	header->style |= WS_VSCROLL;
	header->exStyle |= WS_EX_CLIENTEDGE;

	/* Append all the controls from the other dialogs into
	   paramsDlgTmpl.  */
	for (i = 0; i < numAppend; i++)
	{
		unsigned j;
		DLGITEMTEMPLATEEX* item = (DLGITEMTEMPLATEEX*)appendAddr;
		memcpy(item, (BYTE*)dlgTmpls[i] + sizeof(DLGTEMPLATEEX),
			   tmplSzs[i]);

		/* Adjust the y coordinates of the appended items.  */
		for (j = 0; j < ((DLGTEMPLATEEX*)dlgTmpls[i])->cDlgItems; j++)
		{
			WCHAR *strEnd = item->title;
			item->y += header->cy;

			while (*strEnd != L'\0')
				strEnd++;
			strEnd++;
			/* Note: We use != 1 for the padding since
			   item->windowClass before is strangely 3 words
			   rather 2 words as expected.  */
			if ((strEnd - item->title) % 2 != 1)
				strEnd++; /* DWORD padding  */
			strEnd++; /* Skip extraCount == 0 */
			item = (DLGITEMTEMPLATEEX*)strEnd;
			header->cDlgItems++;
		}

		appendAddr += tmplSzs[i];
		padding = sizeof(DWORD) - tmplSzs[i] % sizeof(DWORD);
		if (padding != sizeof(DWORD))
			appendAddr += padding;
		/* Change the height of the new dialog to be the sum of all
		   the dialogs stacked on top of each other.  */
		header->cy += ((DLGTEMPLATEEX*)dlgTmpls[i])->cy;
	}

	/* Create the actual dialog from the generated template.  */
	paramsDlg = CreateDialogIndirect(g_hInstance, paramsDlgTmpl, hwnd,
		ParamsDlgProc);
	free(paramsDlgTmpl);
}

void HidePanelWin(HWND hwnd)
{
	Panel* panel; Panel* savePanel;
	unsigned subToKeep;
	ShowWindow(hwnd, SW_HIDE);
	panel = PanelFromHWND(mainFrame, hwnd);
	/* if (panel->parent == NULL) then crash */
	if (panel->parent->sub1 == panel)
		subToKeep = 0;
	else
		subToKeep = 1;
	if (panel->parent == mainFrame)
		mainFrame = savePanel = DeletePanel(panel->parent, subToKeep);
	else
		savePanel = DeletePanel(panel->parent, subToKeep);
	SizePanelWindows(savePanel);
}

void ShowPanelWin(HWND hwnd, HWND before1, HWND before2, HWND before3,
	BOOL horzDiv, int subProps, unsigned oldMoveTo, long divPos)
{
	Panel* pOrderTest = NULL; Panel* parent;
	long cxFrame = GetSystemMetrics(SM_CXFRAME);
	long cyFrame = GetSystemMetrics(SM_CYFRAME);

	if (before1 != NULL)
		pOrderTest = PanelFromHWND(mainFrame, before1);
	if (pOrderTest == NULL && before2 != NULL)
		pOrderTest = PanelFromHWND(mainFrame, before2);
	if (pOrderTest == NULL && before3 != NULL)
		pOrderTest = PanelFromHWND(mainFrame, before3);
	if (pOrderTest == NULL)
	{
		mainFrame = InsertPanel(mainFrame, horzDiv, subProps, oldMoveTo);
		parent = mainFrame;
	}
	else
	{
		if (pOrderTest->parent->sub1 == pOrderTest)
			parent = InsertPanel(pOrderTest->parent->sub0,
								 horzDiv, subProps, oldMoveTo);
		else
			parent = InsertPanel(pOrderTest->parent->sub1,
								 horzDiv, subProps, oldMoveTo);
	}

	AddTerminalPanel(parent)->panWin = hwnd;
	parent->divPos = divPos;
	if (divPos < 0)
	{
		if (horzDiv)
			parent->divPos += parent->dims.bottom;
		else
			parent->divPos += parent->dims.right;

		if (!(subProps & SUBPAN_NODIV))
		{
			if (horzDiv)
				parent->divPos -= cyFrame - cyFrame / 2;
			else
				parent->divPos -= cxFrame - cxFrame / 2;
		}
	}
	else
	{
		if (!(subProps & SUBPAN_NODIV))
		{
			if (horzDiv)
				parent->divPos += cyFrame / 2;
			else
				parent->divPos += cxFrame / 2;
		}
	}
	UpdatePanelSizes(parent);
	SizePanelWindows(parent);
	ShowWindow(hwnd, SW_SHOW);
}
