/* Core Windows text editor code base - Foundation module for custom
   coded text editor.  This module is meant to kind of like the code
   base that is behind the functioning of the standard multiline edit
   control available in Windows. However, some minor flaws have been
   discovered in this simple but convenient window control:
	 1. Word wrapping does not always stay up to date.
	 2. In order draw custom multiple highlights behind the text, it
	    seems that you would process the WM_CTRLCOLOREDIT message and
	    draw to the DC based off of what text is on what line.
   It is really only the last flaw that makes things kind of
   unreliable. You could experiment and read on more about Windows edit
   controls, but I chose to do a more bottom-up implementation of the
   text editor, thinking that it would be better in the long run.

   NOTE: Mouse vanish is not supported on Windows 95/98
   NOTE: Mouse wheel requires special support on Windows 95
   General code revision:
   Revise the wrapping code (OK)
   Check the border code (OK)
   Check mouse position code (OK)
   Check various mouse 16/32 bit parts (OK, only 16-bit mouse coordinates)
   Check HIWORDs and LOWORDs (OK)
   Check for numeric overflows
   Check unsigned vs. signed integer areas (OK)
   Wrap up all variables in a context structure
   Perform slight organizational improvements
*/

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0501 /* or _WIN32_WINDOWS 0x410 */
#include <windows.h>
#include <commctrl.h>
/* #include <zmouse.h> */
#include "resource.h"

#include <stdlib.h>
#include <string.h>
#include <crtdbg.h>

#include "bool.h"

extern HINSTANCE g_hInstance;

const unsigned truncLen = 8192; /* Maximum length of lines before line
								   breaking is forced */
#define TRUNC_LEN 8192

enum UndoEntryType
{
	UE_INSERT,
	UE_DELETE,
	UE_REPLACE
};

struct UndoEntry_t
{
	unsigned type; /* Was the operation a insert, delete, or replace? */
	bool caretFirst; /* Did the caret come before the region beginning? */
	unsigned opPos; /* Position to change text */
	char* data; /* If replace, this is the new data */
	char* oldData; /* Only set on replace operations */
};

typedef struct UndoEntry_t UndoEntry;

/* Text data and caret variables */
static char* buffer;
static unsigned buffSize;
static bool ownBuffer;
static unsigned textSize;
static unsigned textPos; /* Byte-wise caret position */
static unsigned caretLine;
static int caretLnOffst; /* Difference from text line to displayed
						    line (used to emulate weird caret snapping
						    behavior) */
static unsigned caretX; /* X position of caret (without borders or margins) */
static unsigned caretY; /* Y position of caret (without borders) */
static bool hasFocus; /* Used to determine if the caret should be rendered */
static bool caretVisible; /* Cleared if the caret is scrolled out of
						     view */
static bool overwrite; /* Is overwrite mode enabled? */
static bool wasInsert; /* Was the last operation an insert or a delete? */
static bool sameUndoOp; /* Should the next operation be added as part
						   of the current undo entry? */
static unsigned regionBegin;
static bool regionActive;
static UndoEntry* undoHistory;
static unsigned numUndo;
static unsigned curUndo;

/* Text metric and display varaibles */
static HFONT hFont;
static unsigned fontHeight;
static bool fixedPitch;
static unsigned avgCharWidth;
static unsigned maxCharWidth;
static unsigned cWidths[256];
static int numkPairs;
static KERNINGPAIR* kernPairs;

static unsigned leftMargin, rightMargin;
static unsigned textAreaWidth;
static unsigned textAreaHeight;
static unsigned numLines;
static unsigned* lineStarts; /* This array always maintains an extra
							    entry that is equal to the text
							    size. */
static POLYTEXT* norTextRI;
static POLYTEXT* regTextRI;
static unsigned numTabStops;
static unsigned* tabStops;
static bool wrapWords;

/* Mouse and mouse wheel scroll variables */
static UINT w95WheelMsg = 0; /* Message ID for MSH_MOUSEWHEEL */
static HWND w95WHwnd = NULL; /* Handle to hidden mouse wheel window */
static BOOL w95Wheel = FALSE; /* Has Windows 95 mouse wheel support
							     been initialized? */
static bool cursorVis; /* Is the cursor visible now? */
static bool drawDrag; /* Should the mouse pan mark be drawn? */
static POINT dragP; /* Point to draw the mouse pan mark at */
static bool lBtnTimeUp; /* Track if there is still time for a double
						   or triple click */
static unsigned lBtnMult; /* Manually track mouse double and triple clicks */
static bool mBtnTimeUp; /* Will releasing the middle mouse button
						   deactivate the mouse pan mark? */
static HICON panMark;
static HCURSOR panCurs[11];
/* This is used to reduce the size of the cursor setting code */
/* (see cursMap.txt for picture) */
static unsigned panCursMap[] = {10, 1, 0, 8,
								3, 7, 5, 0,
								2, 6, 4, 0,
								9, 0, 0, 0};

/* Render parameters */
static bool riInitialized; /* Set to true upon first call to
							  UpdateRenderInfo() */
static unsigned beginUpdLine, endUpdLine; /* Line range for
										     recalculating render info
										     (display relative) */
/* End update line is one line beyond the lines getting updated */
static unsigned norUpLine, numNorUpLines; /* Offset from and number in
										     visible text lines to
										     redraw */
static unsigned lastWrappedLine; /* Which line was last corrected in
								    rewrapping or retruncating
								    lines */
static unsigned lastTextPos; /* Used to find which parts of the region
							    need to be drawn */
static unsigned lastCaretLine; /* Same as lastTextPos */
static RECT updRect; /* Used to verify that the update region has not
					    changed since InvalidateRect() */

/* Variables that reduce function calling */
static unsigned wheelLines; /* This needs updating at WM_SETTINGCHANGE */
static unsigned dblClickTime; /* Same with this */
static BOOL mouseVanish; /* And this */
static unsigned bordWidthX, bordWidthY; /* Thin-line border widths */
static unsigned numVisLines; /* Number of visible lines (including partials) */
static unsigned totVisLines; /* Maximum partial visible lines that
							    will fit in the window */
static unsigned regBeginLine; /* Which line the region begins on */
static unsigned regVisBegLn; /* The first visible region line (display
							    relative) */
static unsigned numRegVisLines;
static HRGN regionMask; /* Used to mark where the region is rendered in */
static HRGN ctRegMask; /* Used for fast rendering of the caret line */
static unsigned visLine; /* First partial visible line */
static unsigned visLnOffset; /* Offset from top of first partial
							    visible line to window top */
static unsigned xScrollPos; /* Position of the horizontal scroll bar */
static unsigned xMaxScroll; /* Length of longest line in the text */
static unsigned* numVisChars; /* Number of visible characters on each line */
static unsigned* firstVisChars; /* First characters on each line to be
								   partially visible */
static unsigned* firstVisOffset; /* Offset from beginning of partially
								    visible characters to left window
								    edge */
static POINT lastMousePos;
static POINT scrdiff; /* Difference from client area origin to screen
					     origin */
static bool lBtnDown; /* The mouse was clicked in our window */
static unsigned winWidth, winHeight;

/* Variables that reduce parameter passing */
static HWND lastHwnd;

/* Debugging variables */
static unsigned hitCount = 0;

/* Private function declarations */
static void UpdateFont(HFONT newFont);
static void CalcTextAreaDims();
static void CalcNumVisLines();
static void GenTabStops();
static void WrapWords(bool rewrap);
static void ProcessTabs(unsigned lineNum, int* charWidths, unsigned endIndex);
static void TruncateLines();
static void LongestLineLen();
static void RetruncateLines(bool setUpdLines/** = false*/);
static void UpdateRenderInfo(bool onlyRegion, bool reindex);
static void UpdateScrollInfo();
static void ScrollContents(bool offset, unsigned otherType, int amount,
						   int sBar);
static bool ScrollRenderStructs(bool vert, int offset);
static void MButtonScroll(bool setCursor); /* Function called for processing
				mouse movements after pressing middle mouse button */
static void MScrollEnd();
static void MWheelScroll(short amount);
static void InsertText(char c, const char* str, bool silent);
static void DeleteText(unsigned p1, unsigned p2, bool silent);
static void InvalidateLines();
static void ProcessMiscKey(WPARAM wParam);
static void AddUndo(); /* Add an operation to the undo queue */
static void InsertLineStart(unsigned line);
static void DeleteLineStarts(unsigned begin, unsigned end);
static void CopyRegion();
static void SkipWord(bool forward);
static void CalcCaretLine();
static void CalcCaretPos();
static void UpdateCaretPos();
static void TextPosFromPoint();
static void ScrollToCaret();
static void UpdateRegion(bool setRegActive);
static void TryCursorUnhide();
static void SortAscending(unsigned* p1, unsigned* p2);
static void FreeUndoHist();
static void FreeRenderInfo(bool freeNorRI/** = true*/);
static void FreeLineInfo();

/* Window procedure called by system for CustomTextEdit class windows.

   Message interception occurs in this function, and processing may be
   peformed here too if the case in very specific.  However, if a case
   is related to other cases, it is better to use a function to
   increase shared code.

   Throughout the function, there are comment labels that categorize
   the messages. */
LRESULT CALLBACK TextEditProc(HWND hwnd, UINT uMsg, WPARAM wParam,
							  LPARAM lParam)
{
	static HCURSOR hIBeam;

	lastHwnd = hwnd;
	switch (uMsg)
	{
	/* Window management messages */
	case WM_CREATE:
		/* Initialize caret. */
		textPos = 0;
		caretLine = 0;
		caretX = 0;
		caretY = 0;
		caretLnOffst = 0;
		hasFocus = false;
		caretVisible = false;

		/* Nullify font display pointers. */
		kernPairs = NULL;
		lineStarts = NULL;
		norTextRI = NULL;
		regTextRI = NULL;
		regionMask = NULL;
		ctRegMask = NULL;
		tabStops = NULL;
		/* Set default word wrapping state. */
		wrapWords = false;
		/* Set client to screen coordinate corrector. */
		scrdiff.x = 0; scrdiff.y = 0;
		ClientToScreen(hwnd, &scrdiff);
		/* Initialize icon handle. */
		hIBeam = LoadCursor(NULL, IDC_IBEAM);

		/* Initialize font. */
		hFont = NULL;
		UpdateFont(NULL);

		/* Initialize buffer. */
		buffer = (char*)malloc(1000);
		buffer[0] = '\0';
		/* strcpy(buffer, "This is sample text. It remains here only to give you an example of how real text \
may look. Really, you should get to doing the right thing, because this is getting \
kind of boring. aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa                        aaaa\n\tNow you should make sure new line handling is O.K."); */
		ownBuffer = true;
		buffSize = 1000;
		textSize = 0;

		/* Initialize basic text edit. */
		overwrite = false;
		wasInsert = false;
		sameUndoOp = false;
		regionActive = false;
		undoHistory = NULL;
		numUndo = 0;
		curUndo = 0;
		regionBegin = 0;
		if (ownBuffer == true)
			AddUndo(); /* We want to undo back to an empty buffer */

		/* Initialize mouse. */
		lBtnDown = false;
		cursorVis = true;

		/* Initialize Windows 95 mouse wheel. */
		/* if (w95Wheel == FALSE)
		{
			UINT w95GetWScrlLines;
			w95WheelMsg = RegisterWindowMessage(MSH_MOUSEWHEEL);
			/** w95WActive = RegisterWindowMessage(MSH_WHEELSUPPORT); *
			if (w95wHwnd == NULL)
				w95WHwnd = FindWindow(MSH_WHEELMODULE_CLASS, MSH_WHEELMODULE_TITLE);
			/** if (w95WheelMsg && w95WHwnd)
				w95Wheel = (BOOL)SendMessage(hdlMSHWheel, msgMSHWheelSupportActive, 0, 0); *
			w95GetWScrlLines = RegisterWindowMessage(MSH_SCROLL_LINES);
			if (w95GetWScrlLines && w95WHwnd)
				wheelLines = (unsigned)SendMessage(w95WHwnd, w95GetWScrlLines, 0, 0);
		} */
		drawDrag = false;
		lBtnMult = 0;
		lBtnTimeUp = true;
		mBtnTimeUp = false;
		if (wrapWords == true)
			panMark = LoadIcon(g_hInstance, (LPCTSTR)VERT_MARK);
		else
			panMark = LoadIcon(g_hInstance, (LPCTSTR)CENT_MARK);
		{
			unsigned i;
			for (i = 0; i < 11; i++)
				panCurs[i] = LoadCursor(g_hInstance, (LPCTSTR)(U_ARROW+i));
		}

		/* Initialize text scroll cache. */
		numVisLines = 0;
		totVisLines = 0;
		regVisBegLn = 0;
		numRegVisLines = 0;
		visLine = 0;
		visLnOffset = 0;
		regBeginLine = 0;
		xScrollPos = 0;
		xMaxScroll = 0;
		numVisChars = NULL;
		firstVisChars = NULL;
		firstVisOffset = NULL;

		/* Initialize redraw cache. */
		beginUpdLine = 0;
		endUpdLine = 0;
		ZeroMemory(&updRect, sizeof(RECT));
		norUpLine = 0;
		numNorUpLines = 0;
		lastWrappedLine = 0;
		lastTextPos = 0;
		lastCaretLine = 0;
		visLine = 0;

		/* Initialize the system variables too. */
	case WM_SETTINGCHANGE:
		SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &wheelLines, 0);
		SystemParametersInfo(SPI_GETMOUSEVANISH, 0, &mouseVanish, 0);
		dblClickTime = GetDoubleClickTime();
		bordWidthX = GetSystemMetrics(SM_CXBORDER);
		bordWidthY = GetSystemMetrics(SM_CYBORDER);
		break;
	case WM_DESTROY:
		/* Is WM_KILLFOCUS called before WM_DESTROY automatically? */
		TryCursorUnhide();
		if (ownBuffer == true)
		{
			free(buffer);
			buffer = NULL;
		}
		FreeUndoHist();
		free(kernPairs);
		kernPairs = NULL;
		free(lineStarts);
		lineStarts = NULL;
		FreeRenderInfo(true);
		free(tabStops);
		tabStops = NULL;
		FreeLineInfo();
		break;
	case WM_PAINT:
	{
		/* We have to re-get these because Windows 95 does not send a
		   proper notification. */
		COLORREF regBkCol;
		COLORREF regTxtCol;
		COLORREF norTxtCol;
		PAINTSTRUCT ps;
		HDC hDC;
		/* Temporary variables */
		unsigned i;
		regBkCol = GetSysColor(COLOR_HIGHLIGHT);
		regTxtCol = GetSysColor(COLOR_HIGHLIGHTTEXT);
		norTxtCol = GetSysColor(COLOR_WINDOWTEXT);

		if (riInitialized == false)
			break;

		/* Verify that the beginUpdLine and endUpdLine are valid. */
		{
			RECT rt;

			GetUpdateRect(hwnd, &rt, FALSE);
			/* If the actual update RECT is not in or equal to the
			   saved update RECT, then don't use the cached redraw
			   lines. */
			if (endUpdLine == 0 ||
				(rt.left < updRect.left ||  rt.top < updRect.top ||
				rt.right > updRect.right || rt.bottom > updRect.bottom))
			{
				/* Assume all lines need to be drawn. */
				beginUpdLine = 0;
				endUpdLine = numVisLines;
			}
			else /* Debugging */
				hitCount++;
		}

		/* The caret line has special rendering priority, so
		   render it separately. */
		if (caretLine >= visLine + beginUpdLine &&
			caretLine < visLine + endUpdLine)
		{
			RECT rt;
			RECT rt2;
			RECT valRect;
			HDC hMemDC;
			HBITMAP hBmCaret;
			unsigned cvl;

			HideCaret(hwnd);
			rt.left = 0;
			rt.top = (int)bordWidthY + (caretLine - visLine) *
				fontHeight - visLnOffset;
			rt.right = winWidth;
			rt.bottom = (int)bordWidthY + (caretLine + 1 - visLine) *
				fontHeight - visLnOffset;
			hDC = GetDC(hwnd);
			hMemDC = CreateCompatibleDC(hDC);
			hBmCaret = CreateCompatibleBitmap(hDC, rt.right,
											  rt.bottom - rt.top);
			SelectObject(hMemDC, hFont);
			SelectObject(hMemDC, hBmCaret);
			rt2.left = 0; rt2.top = 0;
			rt2.right = winWidth; rt2.bottom = fontHeight;
			cvl = caretLine - visLine;
			/* Draw the normal text line. */
			ExtTextOut(hMemDC, norTextRI[cvl].x, 0, ETO_OPAQUE, &rt2,
				norTextRI[cvl].lpstr, norTextRI[cvl].n, norTextRI[cvl].pdx);
			/* Draw the region text line (if visible). */
			SetTextColor(hMemDC, regTxtCol);
			SetBkColor(hMemDC, regBkCol);
			SelectClipRgn(hMemDC, ctRegMask);
			if (regionActive == true &&
				regVisBegLn + numRegVisLines > caretLine - visLine)
			{
				ExtTextOut(hMemDC, norTextRI[cvl].x, 0, ETO_OPAQUE, &rt2,
					norTextRI[cvl].lpstr, norTextRI[cvl].n, norTextRI[cvl].pdx);
			}
			/* Copy the bitmap. */
			IntersectClipRect(hDC, bordWidthX, bordWidthY,
				winWidth - bordWidthX, winHeight - bordWidthY);
			BitBlt(hDC, 0, rt.top, rt.right, rt.bottom - rt.top,
				   hMemDC, 0, 0, SRCCOPY);
			DeleteObject(hBmCaret);
			DeleteDC(hMemDC);
			ReleaseDC(hwnd, hDC);
			/* Validate the drawn area. */
			rt2.left = bordWidthX;
			rt2.top = bordWidthY;
			rt2.right = winWidth - bordWidthX;
			rt2.bottom = winHeight - bordWidthY;
			IntersectRect(&valRect, &rt, &rt2);
			ValidateRect(hwnd, &valRect);
			ShowCaret(hwnd);
		}

		hDC = BeginPaint(hwnd, &ps);
		SelectObject(hDC, hFont);
		IntersectClipRect(hDC, bordWidthX, bordWidthY,
			winWidth - bordWidthX, winHeight - bordWidthY);
		/** PolyTextOut(hDC, &norTextRI[beginUpdLine], endUpdLine - beginUpdLine); */
		for (i = beginUpdLine; i < endUpdLine; i++)
		{
			if (i == caretLine - visLine)
				continue;
			ExtTextOut(hDC, norTextRI[i].x, norTextRI[i].y, 0, NULL,
				norTextRI[i].lpstr, norTextRI[i].n, norTextRI[i].pdx);
		}
		if (regionActive == true && numRegVisLines != 0)
		{
			HRGN origRgn;
			SetBkMode(hDC, OPAQUE);
			SetTextColor(hDC, regTxtCol);
			SetBkColor(hDC, regBkCol);
			/* Configure clip rectangles. */
			origRgn = CreateRectRgn(0, 0, 2, 2);
			GetClipRgn(hDC, origRgn);
			ExtSelectClipRgn(hDC, regionMask, RGN_AND);
			/** PolyTextOut(hDC, &norTextRI[regVisBegLn], numRegVisLines); */
			for (i = regVisBegLn; i < regVisBegLn + numRegVisLines; i++)
			{
				if (i == caretLine - visLine)
					continue;
				ExtTextOut(hDC, norTextRI[i].x, norTextRI[i].y, 0, NULL,
					norTextRI[i].lpstr, norTextRI[i].n, norTextRI[i].pdx);
			}
			SetTextColor(hDC, norTxtCol);
			/** SetBkMode(hDC, TRANSPARENT); */
			SelectClipRgn(hDC, origRgn);
			DeleteObject(origRgn);
		}
		if (drawDrag == true)
			DrawIcon(hDC, dragP.x - 16, dragP.y - 16, panMark);
		EndPaint(hwnd, &ps);
		endUpdLine = 0;
		break;
	}
	case WM_SIZE:
		winWidth = LOWORD(lParam);
		winHeight = HIWORD(lParam);
		FreeRenderInfo(true);
		CalcTextAreaDims();
		GenTabStops();
		if (wrapWords == true)
			WrapWords(false);
		else
			TruncateLines();
		UpdateScrollInfo();
		UpdateRenderInfo(false, false);
		CalcCaretPos();
		UpdateCaretPos();
		break;
	case WM_MOVE:
		scrdiff.x = 0; scrdiff.y = 0;
		ClientToScreen(hwnd, &scrdiff);
		break;
	case WM_CAPTURECHANGED:
		/** if ((HWND)lParam != hwnd) */
		lBtnDown = false;
		KillTimer(hwnd, 3);
		if (lBtnTimeUp == true)
			lBtnMult = 0;
		/* Sorry, no double and triple clicks today. */
		lBtnMult = 0;
		break;
	/* Special input messages */
	case WM_HSCROLL:
		ScrollContents(false, LOWORD(wParam), 0, SB_HORZ);
		break;
	case WM_VSCROLL:
		ScrollContents(false, LOWORD(wParam), 0, SB_VERT);
		break;
	case WM_MOUSEACTIVATE:
		SetFocus(hwnd);
		return MA_ACTIVATE;
	case WM_CONTEXTMENU:
	{
		HMENU hMaster;
		HMENU hMen;
		POINT pt;
		hMaster = LoadMenu(g_hInstance, (LPCTSTR)SHRTCMENUS);
		hMen = GetSubMenu(hMaster, 0);
		pt.x = (short)LOWORD(lParam);
		pt.y = (short)HIWORD(lParam);

		if (pt.x == -1)
		{
			pt.x = (long)caretX; pt.y = (long)caretY;
			ClientToScreen(hwnd, &pt);
		}
		if (!IsClipboardFormatAvailable(CF_TEXT))
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
			EnableMenuItem(hMen, M_SELECTALL, MF_BYCOMMAND | MF_GRAYED);
		TrackPopupMenu(hMen, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
			pt.x, pt.y, 0, GetParent(hwnd), NULL);
		DestroyMenu(hMaster);
		break;
	}
	case WM_TIMER:
		if (wParam == 1) /* Middle button scrolling */
			MButtonScroll(false);
		if (wParam == 2) /* Holding middle button down rather than toggling */
		{
			mBtnTimeUp = true;
			KillTimer(hwnd, 2);
		}
		/* The mouse is clicked and dragged outside the window. */
		if (wParam == 3 && (lastMousePos.x < 0 ||
			lastMousePos.x >= (int)winWidth || lastMousePos.y < 0 ||
			lastMousePos.y >= (int)winHeight))
		{
			unsigned line1, line2;
			line1 = caretLine;
			TextPosFromPoint();
			line2 = caretLine;
			SortAscending(&line1, &line2);
			if (line1 >= visLine + numVisLines || line2 < visLine)
				line2 = 0;
			if (line1 < visLine)
				line1 = visLine;
			if (line2 >= visLine + numVisLines)
				line2 = visLine + numVisLines - 1;
			beginUpdLine = line1 - visLine;
			endUpdLine = line2 + 1 - visLine;
			UpdateRegion(true);
		}
		/* Timer for double and triple clicks */
		if (wParam == 4)
		{
			lBtnTimeUp = true;
			KillTimer(hwnd, 4);
			/* Sorry, no double and triple clicks today. */
			lBtnMult = 0;
		}
		break;
	case EM_GETHANDLE:
		return (LRESULT)buffer;
	case EM_SETHANDLE:
		if (ownBuffer == true)
		{
			ownBuffer = false;
			free(buffer);
		}
		buffer = (char*)wParam;
		buffSize = (unsigned)lParam;
		textSize = strlen(buffer);
		textPos = 0;
		if (regionActive == true)
		{
			UpdateRegion(false);
		} /* more? */
		FreeRenderInfo(true);
		WrapWords(false);
		UpdateScrollInfo();
		UpdateRenderInfo(false, false);
		CalcCaretPos();
		UpdateCaretPos();
		break;
	/* Edit commands */
	case WM_CUT:
		if (regionActive == true)
		{
			CopyRegion();
			DeleteText(regionBegin, textPos, false);
			UpdateRegion(false);
		}
		break;
	case WM_COPY:
		if (regionActive == true)
			CopyRegion();
		break;
	case WM_PASTE:
		if (OpenClipboard(hwnd))
		{
			HANDLE clipMem;
			char* clipCont;
			clipMem = GetClipboardData(CF_TEXT);
			clipCont = (char*)GlobalLock(clipMem);

			if (regionActive == true)
			{
				DeleteText(regionBegin, textPos, false);
				UpdateRegion(false);
			}
			if (clipCont != NULL)
			{
				/* Convert \r\n pairs to \n */
				SIZE_T clipSize;
				unsigned dataSize;
				char* newData;
				unsigned newDataSize;
				/* Temporary variables */
				unsigned i;
				/* dataSize = strlen(clipCont); We're being secure! */
				clipSize = GlobalSize(clipMem);
				dataSize = (unsigned)clipSize - 1;
				newData = (char*)malloc(dataSize + 1);
				newDataSize = 0;

				for (i = 0; i < dataSize; i++)
				{
					if (clipCont[i] != '\r')
						newData[newDataSize++] = clipCont[i];
					else
					{
						if (clipCont[i+1] == '\n') /* CR+LF */
							i++;
						newData[newDataSize++] = '\n';
					}
				}
				newData[newDataSize++] = '\0';
				sameUndoOp = false;
				InsertText(0, newData, false);
				free(newData);
			}
			GlobalUnlock(clipMem);
			CloseClipboard();
		}
		break;
	case WM_CLEAR:
		if (regionActive == true)
		{
			DeleteText(regionBegin, textPos, false);
			UpdateRegion(false);
		}
		break;
	case EM_GETSEL:
		if (regionActive == false)
		{
			*((unsigned*)wParam) = textPos;
			*((unsigned*)lParam) = textPos;
		}
		else
		{
			*((unsigned*)wParam) = regionBegin;
			*((unsigned*)lParam) = textPos;
		}
		return MAKELPARAM(*((short*)wParam), *((short*)lParam));
	case EM_SETSEL:
		regionBegin = (unsigned)wParam;
		if (lParam == -1)
			textPos = textSize;
		else
			textPos = (unsigned)lParam;
		CalcCaretLine();
		ScrollToCaret();
		UpdateRegion(true);
		CalcCaretPos();
		UpdateCaretPos();
		break;
	case WM_UNDO:
		if (curUndo > 0)
		{
			UndoEntry* cuPtr;
			cuPtr = &undoHistory[curUndo];
			sameUndoOp = false;
			if (cuPtr->type == UE_INSERT)
			{
				regionBegin = cuPtr->opPos;
				textPos = regionBegin;
				CalcCaretLine();
				ScrollToCaret();
				InsertText(0, cuPtr->data, true);
				if (cuPtr->caretFirst == true)
				{
					unsigned t;
					t = regionBegin;
					regionBegin = textPos;
					textPos = t;
				}
				UpdateRegion(true);
			}
			else if (cuPtr->type == UE_DELETE)
			{
				textPos = cuPtr->opPos;
				CalcCaretLine();
				ScrollToCaret();
				DeleteText(cuPtr->opPos,
					cuPtr->opPos + strlen(cuPtr->data), true);
				UpdateRegion(false);
			}
			else
			{
				/* Replace text. */
				DeleteText(cuPtr->opPos,
					cuPtr->opPos + strlen(cuPtr->data), true);
				textPos = cuPtr->opPos;
				InsertText(0, cuPtr->oldData, true);
			}
			curUndo--;
		}
		if (curUndo == 0)
			SendMessage(GetParent(hwnd), NM_CANUNDO, 0, FALSE);
		if (curUndo > 0)
			SendMessage(GetParent(hwnd), NM_CANUNDO, 0, TRUE);
		if (curUndo < numUndo - 1)
			SendMessage(GetParent(hwnd), NM_CANUNDO, 1, TRUE);
		if (curUndo == numUndo - 1)
			SendMessage(GetParent(hwnd), NM_CANUNDO, 1, FALSE);
		break;
	case EM_REDO:
		if (curUndo < numUndo - 1)
		{
			UndoEntry* cuPtr;
			sameUndoOp = false;
			curUndo++;
			cuPtr = &undoHistory[curUndo];
			if (cuPtr->type == UE_DELETE)
			{
				textPos = cuPtr->opPos;
				CalcCaretLine();
				ScrollToCaret();
				InsertText(0, cuPtr->data, true);
				UpdateRegion(false);
			}
			else if (cuPtr->type == UE_INSERT)
			{
				textPos = cuPtr->opPos;
				CalcCaretLine();
				ScrollToCaret();
				DeleteText(cuPtr->opPos,
					cuPtr->opPos + strlen(cuPtr->data), true);
				UpdateRegion(false);
			}
			else
			{
				/* Replace text. */
				DeleteText(cuPtr->opPos,
					cuPtr->opPos + strlen(cuPtr->oldData), true);
				textPos = cuPtr->opPos;
				InsertText(0, cuPtr->data, true);
			}
		}
		if (curUndo == 0)
			SendMessage(GetParent(hwnd), NM_CANUNDO, 0, FALSE);
		if (curUndo > 0)
			SendMessage(GetParent(hwnd), NM_CANUNDO, 0, TRUE);
		if (curUndo < numUndo - 1)
			SendMessage(GetParent(hwnd), NM_CANUNDO, 1, TRUE);
		if (curUndo == numUndo - 1)
			SendMessage(GetParent(hwnd), NM_CANUNDO, 1, FALSE);
		break;
	case EM_CANUNDO:
		if (curUndo == 0) return FALSE;
		else return TRUE;
	case EM_CANREDO:
		if (curUndo == numUndo - 1) return FALSE;
		else return TRUE;
	case WM_SETFONT:
		FreeRenderInfo(true);
		UpdateFont((HFONT)wParam);
		if (hasFocus == true && caretVisible == true)
		{
			DestroyCaret();
			CreateCaret(hwnd, NULL, fontHeight / 16, fontHeight);
			ShowCaret(hwnd);
		}
		CalcTextAreaDims();
		GenTabStops();
		if (wrapWords == true)
			WrapWords(false);
		else
			TruncateLines();
		UpdateScrollInfo();
		UpdateRenderInfo(false, false);
		CalcCaretPos();
		UpdateCaretPos();
		if (lParam == TRUE)
		{
			InvalidateRect(hwnd, NULL, TRUE);
			ZeroMemory(&updRect, sizeof(RECT));
		}
		break;
	/* Mouse input messages */
	case WM_LBUTTONDOWN:
		sameUndoOp = false;
		TryCursorUnhide();
		if (lBtnTimeUp == false)
		{
			KillTimer(hwnd, 4);
			SetTimer(hwnd, 4, dblClickTime, NULL);
			lBtnMult++;
		}
		if (lBtnTimeUp == true)
		{
			SetTimer(hwnd, 4, dblClickTime, NULL);
			lBtnTimeUp = false;
			lBtnMult = 1;
		}
		if (drawDrag == true)
		{
			MScrollEnd();
			return 0;
		}
		lastMousePos.x = (short)LOWORD(lParam);
		lastMousePos.y = (short)HIWORD(lParam);
		beginUpdLine = caretLine - visLine;
		TextPosFromPoint();
		/* Process double and triple clicks. */
		if (lBtnMult == 2)
		{
			SkipWord(false);
		}
		if (lBtnMult == 3)
		{
			ProcessMiscKey(VK_HOME);
		}
		if (GetKeyState(VK_SHIFT) & 0x8000)
		{
			unsigned line1, line2;
			line1 = beginUpdLine;
			line2 = caretLine;
			SortAscending(&line1, &line2);
			if (line1 >= visLine + numVisLines || line2 < visLine)
				line2 = 0;
			if (line1 < visLine)
				line1 = visLine;
			if (line2 >= visLine + numVisLines)
				line2 = visLine + numVisLines - 1;
			beginUpdLine = line1 - visLine;
			endUpdLine = line2 + 1 - visLine;
			UpdateRegion(true);
		}
		else
		{
			regionBegin = textPos;
			if (regionActive == true)
				UpdateRegion(false);
		}
		lBtnDown = true;
		SetTimer(hwnd, 3, 50, NULL);
		SetCapture(hwnd);
		break;
	case WM_LBUTTONUP:
		lBtnDown = false;
		KillTimer(hwnd, 3);
		ReleaseCapture();
		if (lBtnTimeUp == true)
			lBtnMult = 0;
		/* Sorry, no double and triple clicks today. */
		lBtnMult = 0;
		break;
	case WM_RBUTTONDOWN:
		TryCursorUnhide();
		if (drawDrag == true)
		{
			MScrollEnd();
			return 0;
		}
		break;
	case WM_RBUTTONUP:
		break;
	case WM_MBUTTONDOWN:
		TryCursorUnhide();
		if (drawDrag == false)
		{
			RECT rt;
			dragP.x = (short)LOWORD(lParam);
			dragP.y = (short)HIWORD(lParam);
			drawDrag = true;
			SetTimer(hwnd, 1, 10, NULL);
			SetTimer(hwnd, 2, dblClickTime, NULL);
			rt.left = dragP.x - 16;
			rt.top = dragP.y - 16;
			rt.right = dragP.x + 16;
			rt.bottom = dragP.y + 16;
			InvalidateRect(hwnd, &rt, TRUE);
			ZeroMemory(&updRect, sizeof(RECT));
			SendMessage(lastHwnd, WM_SETCURSOR, 0, 0);
		}
		else
			MScrollEnd();
		break;
	case WM_MBUTTONUP:
		if (mBtnTimeUp == true)
			MScrollEnd();
		break;
	case WM_MOUSEMOVE:
		TryCursorUnhide();
		lastMousePos.x = (short)LOWORD(lParam);
		lastMousePos.y = (short)HIWORD(lParam);
		if (lBtnDown == true)
		{
			unsigned line1, line2;
			line1 = caretLine;
			TextPosFromPoint();
			/* Process double and triple click drags. */
			if (lBtnMult == 2 && regionBegin < textPos)
				SkipWord(true);
			if (lBtnMult == 2 && regionBegin > textPos)
				SkipWord(false);
			if (lBtnMult == 3 && regionBegin < textPos)
				ProcessMiscKey(VK_END);
			if (lBtnMult == 3 && regionBegin > textPos)
				ProcessMiscKey(VK_HOME);
			if (lBtnMult == 2 || lBtnMult == 3)
			{
				CalcCaretPos();
				UpdateCaretPos();
			}
			line2 = caretLine;
			SortAscending(&line1, &line2);
			if (line1 >= visLine + numVisLines || line2 < visLine)
				line2 = 0;
			if (line1 < visLine)
				line1 = visLine;
			if (line2 >= visLine + numVisLines)
				line2 = visLine + numVisLines - 1;
			beginUpdLine = line1 - visLine;
			endUpdLine = line2 + 1 - visLine;
			UpdateRegion(true);
		}
		break;
	case WM_SETCURSOR:
	{
		POINT pt;
		GetCursorPos(&pt);
		pt.x -= scrdiff.x;
		pt.y -= scrdiff.y;
		if (drawDrag == true)
		{
			lastMousePos.x = pt.x;
			lastMousePos.y = pt.y;
			MButtonScroll(true);
			return TRUE;
		}
		if (0 <= pt.x && pt.x < (int)winWidth &&
			0 <= pt.y && pt.y < (int)winHeight)
		{
			SetCursor(hIBeam);
			return TRUE;
		}
		break;
	}
	case WM_MOUSEWHEEL:
	/** case MSH_MOUSEWHEEL: */
		MWheelScroll((short)HIWORD(wParam));
		break;
	/* Keyboard input messages */
	case WM_SETFOCUS:
		hasFocus = true;
		CreateCaret(hwnd, NULL, fontHeight / 16, fontHeight);
		ShowCaret(hwnd);
		UpdateCaretPos();
		break;
	case WM_KILLFOCUS:
		hasFocus = false;
		TryCursorUnhide();
		DestroyCaret();
		MScrollEnd();
		break;
	case WM_KEYDOWN:
		ProcessMiscKey(wParam);
		break;
	case WM_CHAR:
		if (wParam < 0x20 && wParam != 0x08 && wParam != 0x09 &&
			wParam != 0x0D)
			break; /* We don't insert control characters... */
		{
			POINT pt;
			GetCursorPos(&pt);
			pt.x -= scrdiff.x;
			pt.y -= scrdiff.y;
			if (0 <= pt.x && pt.x < (int)winWidth &&
				0 <= pt.y && pt.y < (int)winHeight &&
				cursorVis == true && mouseVanish == TRUE)
			{
				cursorVis = false;
				ShowCursor(FALSE);
			}
		}
		if (regionActive == true)
		{
			DeleteText(regionBegin, textPos, false);
			UpdateRegion(false);
			if (wParam == 0x08 || wParam == 0x7F)
				break;
		}
		if (wParam == 0x08) /* backspace */
		{
			if (textPos != 0)
				DeleteText(textPos - 1, textPos, false);
			break;
		}
		if (wParam == 0x7F) /* CTRL+backspace */
		{
			regionBegin = textPos;
			SkipWord(false);
			DeleteText(textPos, regionBegin, false);
			break;
		}
		if (wParam == 0x0D) /* carriage-return */
			InsertText('\n', NULL, false);
		else
			InsertText((char)wParam, NULL, false);
		break;
	/* Custom messages */
	default:
		break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

/* Saves the new font handle and caches various font metrics.

   The font should only be changed upon request with the WM_SETFONT
   message.  The window that sent the message is responsible for font
   resource cleanup.

   Whenever the font is changed, other cached data, such as the scroll
   position, is also invalidated.  It is the caller's responsibility
   to call these other functions. */
static void UpdateFont(HFONT newFont)
{
	LOGFONT fontInfo;
	TEXTMETRIC tm;
	unsigned oldFontHeight;
	HDC hDC;

	if (hFont == NULL)
		oldFontHeight = 0;
	else
		oldFontHeight = fontHeight;

	hFont = newFont;

	hDC = GetDC(lastHwnd);
	SelectObject(hDC, hFont);
	GetTextMetrics(hDC, &tm);
	fontHeight = tm.tmHeight;
	avgCharWidth = tm.tmAveCharWidth;
	maxCharWidth = tm.tmMaxCharWidth;

	/* If fontHeight is zero, that could CRASH the program, but it
	   seems that the font chooser dialog will not allow that. */

	GetObject(hFont, sizeof(LOGFONT), &fontInfo);
	if (fontInfo.lfPitchAndFamily & FIXED_PITCH)
		fixedPitch = true;
	else
	{
		float tempcWidths[256];
		ABCFLOAT abcWidths[256];
		/* Temporary variables */
		unsigned i;
		fixedPitch = false;
		/* Get the character widths (optional). */
		GetCharWidthFloat(hDC, 0x00, 0xFF, tempcWidths);
		for (i = 0; i < 256; i++)
			cWidths[i] = (unsigned)(tempcWidths[i] * 16);

		numkPairs = GetKerningPairs(hDC, -1, NULL);
		if (numkPairs <= 0)
		{
			if (kernPairs != NULL)
			{
				free(kernPairs);
				kernPairs = NULL;
			}
			numkPairs = 0;
		}
		else
		{
			if (kernPairs != NULL)
			{
				free(kernPairs);
				kernPairs = NULL;
			}
			kernPairs = (KERNINGPAIR*)malloc(sizeof(KERNINGPAIR) * numkPairs);
			GetKerningPairs(hDC, numkPairs, kernPairs);
		}

		leftMargin = 0;
		rightMargin = 0;
		GetCharABCWidthsFloat(hDC, 0x00, 0xFF, abcWidths);
		for (i = 0; i < 256; i++)
		{
			if (abcWidths[i].abcfA < -(float)leftMargin)
				leftMargin = (unsigned)(-abcWidths[i].abcfA);
			if (abcWidths[i].abcfC < -(float)rightMargin)
				rightMargin = (unsigned)(-abcWidths[i].abcfC);
		}
	}
	ReleaseDC(lastHwnd, hDC);

	/* Reset visLine and visLnOffset. */
	{
		unsigned oldYPos;
		oldYPos = oldFontHeight * visLine + visLnOffset;
		visLine = oldYPos / fontHeight;
		visLnOffset = oldYPos % fontHeight;
	}
}

/* Calculates the text area dimensions.

   The text area dimensions specify the width and height subset out of
   the client area that text drawing and wrapping occurs in. */
static void CalcTextAreaDims()
{
	if (winWidth > leftMargin + rightMargin + bordWidthX * 2)
		textAreaWidth = winWidth -
			(leftMargin + rightMargin + bordWidthX * 2);
	else
		textAreaWidth = 0;

	if (textAreaWidth <= maxCharWidth)
		textAreaWidth = maxCharWidth + 1;

	if (winHeight > bordWidthY * 2)
		textAreaHeight = winHeight - bordWidthY * 2;
	else
		textAreaHeight = 0;

	/* Calculate the total number of visible lines. */
	totVisLines = textAreaHeight / fontHeight;
	if (textAreaHeight % fontHeight > 0)
		totVisLines++;
	totVisLines++;

	/* Perform line information allocations. */
	if (numVisChars == NULL)
	{
		numVisChars = (unsigned*)malloc(sizeof(unsigned) * totVisLines);
		firstVisChars = (unsigned*)malloc(sizeof(unsigned) * totVisLines);
		firstVisOffset = (unsigned*)malloc(sizeof(unsigned) * totVisLines);
		ZeroMemory(numVisChars, sizeof(unsigned) * totVisLines);
	}
	else
	{
		numVisChars = (unsigned*)realloc(numVisChars,
			sizeof(unsigned) * totVisLines);
		firstVisChars = (unsigned*)realloc(firstVisChars,
			sizeof(unsigned) * totVisLines);
		firstVisOffset = (unsigned*)realloc(firstVisOffset,
			sizeof(unsigned) * totVisLines);
		ZeroMemory(numVisChars, sizeof(unsigned) * totVisLines);
	}
}

/* Calculates the number of visible lines.

   This function normally only be called from UpdateRenderInfo(), but
   it is provided as a separate function for exceptional cases. */
static void CalcNumVisLines()
{
	numVisLines = (textAreaHeight + visLnOffset) / fontHeight;
	if ((textAreaHeight + visLnOffset) % fontHeight > 0)
		numVisLines++;
	if (numVisLines > numLines)
		numVisLines = numLines;
}

/* Generates standard tab stops.

   This function should be used whenever the window size has changed.
   If custom tab stops are used instead, then those will need to be
   updated by the proper mechanism. */
static void GenTabStops()
{
	HDC hDC;
	unsigned tabAreaWidth;
	unsigned tsWidth;
	unsigned dist;
	hDC = GetDC(lastHwnd);
	SelectObject(hDC, hFont);
	tsWidth = LOWORD(GetTabbedTextExtent(hDC, "\t", 1, 0, NULL));
	ReleaseDC(lastHwnd, hDC);
	tabAreaWidth = textAreaWidth;
	if (wrapWords == false && xMaxScroll > textAreaWidth)
		tabAreaWidth = xMaxScroll;

	if (tabStops != NULL)
	{
		free(tabStops);
		tabStops = NULL;
	}
	numTabStops = 0;
	tabStops = (unsigned*)malloc(sizeof(unsigned) * 20);
	for (dist = 0; ; )
	{
		dist += (unsigned)tsWidth;
		if (dist >= tabAreaWidth)
		{
			if (tabAreaWidth != 0)
				tabStops[numTabStops] = tabAreaWidth;
			else
				tabStops[numTabStops] = tabAreaWidth;
			numTabStops++;
			if (numTabStops % 20 == 0)
			{
				tabStops = (unsigned*)realloc(tabStops,
					sizeof(unsigned) * (numTabStops + 20));
			}
			break;
		}
		tabStops[numTabStops] = dist;
		numTabStops++;
		if (numTabStops % 20 == 0)
		{
			tabStops = (unsigned*)realloc(tabStops,
				sizeof(unsigned) * (numTabStops + 20));
		}
	}
	tabStops = (unsigned*)realloc(tabStops, sizeof(unsigned) * (numTabStops));
}

/* Wraps or rewraps words in the buffer.

   WrapWords() updates the line start information according to a
   standard word wrapping algorithm (Kinsoku rules).  This function
   does not affect the text buffer.

   If "rewrap" is equal to true, then the function will attempt to
   perform the minimum number of computations required to rewrap the
   buffer.  When rewrapping, "caretLine" should be equal to the first
   line which text was modified on.  The rewrapping function will
   check if rewrapping needs to be performed on the line just before
   "caretLine". */
static void WrapWords(bool rewrap)
{
	unsigned curLine;
	unsigned wrapPos;
	unsigned wrapEnd;
	bool foundNewline;
	bool insertedLines;
	bool deletedLines;
	HDC hDC;
	/* Temporary variables */
	unsigned i;
	foundNewline = false;
	insertedLines = false;
	deletedLines = false;

	if (textAreaWidth <= maxCharWidth)
		return;

	if (rewrap == true)
	{
		unsigned lastTextPos;
		unsigned testLine;
		curLine = caretLine;
		wrapPos = lineStarts[curLine];
		lastTextPos = textPos;
		SkipWord(false);
		if (textPos <= lineStarts[caretLine] && textPos != 0 &&
			(lastTextPos < textSize || wasInsert == false) &&
			caretLine > 0 && buffer[lineStarts[caretLine]-1] != '\n')
		{
			/* Start wrapping words one before the caret line. */
			curLine--;
			wrapPos = lineStarts[curLine];
			beginUpdLine = curLine;
		}
		textPos = lastTextPos;
		/* Search forward for the rewrapping end position. */
		wrapEnd = 0;
		for (testLine = curLine + 1; testLine < numLines; testLine++)
		{
			if (buffer[lineStarts[testLine]-1] == '\n')
			{
				wrapEnd = lineStarts[testLine];
				break;
			}
		}
		if (wrapEnd == 0)
			wrapEnd = textSize;
	}
	else
	{
		/* Reset the necessary variables. */
		if (lineStarts != NULL)
		{
			free(lineStarts);
			lineStarts = NULL;
		}
		numLines = 0;
		lineStarts = (unsigned*)malloc(sizeof(unsigned) * 20);
		wrapPos = 0;
		lineStarts[0] = 0;
		numLines++;
		curLine = numLines - 1;
		wrapEnd = textSize;
	}

	hDC = GetDC(lastHwnd);
	SelectObject(hDC, hFont);
	while (wrapPos < wrapEnd)
	{
		GCP_RESULTS wrapRes;
		unsigned wrapStride;
		int charWidths[TRUNC_LEN];
		unsigned newLength;
		unsigned lastSpace;
		/* Temporary variables */
		unsigned j;
		wrapStride = truncLen;
		newLength = 0;
		lastSpace = 0;

		ZeroMemory(&wrapRes, sizeof(GCP_RESULTS));
		wrapRes.lStructSize = sizeof(GCP_RESULTS);
		wrapRes.lpDx = charWidths;
		if (wrapPos + wrapStride > wrapEnd)
			wrapStride = wrapEnd - wrapPos;
		wrapRes.nGlyphs = wrapStride;
		GetCharacterPlacement(hDC, &(buffer[wrapPos]), wrapStride,
			textAreaWidth, &wrapRes, GCP_MAXEXTENT);
		wrapPos += wrapRes.nMaxFit;
		/* Process for tabs. */
		ProcessTabs(curLine, charWidths, wrapPos);
		/* Process for newlines and make sure that the line ends with
		   whitespace, if possible. */
		for (i = lineStarts[curLine], j = 0;
			newLength < textAreaWidth && i < wrapPos; i++)
		{
			if (buffer[i] == '\t' ||
				buffer[i] == ' ')
				lastSpace = i;
			if (buffer[i] == '\n')
			{
				lastSpace = i;
				break;
			}
			newLength += charWidths[j];
			j++;
		}

		if (lastSpace != 0 && (buffer[lastSpace] == ' ' ||
			buffer[lastSpace] == '\t' || buffer[lastSpace] == '\n'))
			wrapPos = lastSpace + 1;
		else if (curLine == 0 && (buffer[lastSpace] == ' ' ||
			buffer[lastSpace] == '\t' || buffer[lastSpace] == '\n'))
			wrapPos = lastSpace + 1;

		/* If the line ends with a space, then keep skipping any
		   subsequent space characters untill a non-space character
		   is reached. */
		if (buffer[lastSpace] == ' ')
		{
			while (buffer[wrapPos] == ' ')
				wrapPos++;
			if (wrapPos > 0 && buffer[wrapPos] == '\0')
				break;
		}

		/* See if this line really needs to be broken. */
		/* If we are at the end of the text data, there is not
		   a newline at the end, and there is spare length
		   at the end of the window, then we are done wrapping. */
		if (wrapRes.nMaxFit == wrapStride && buffer[wrapPos-1] != '\n' &&
			newLength < textAreaWidth && curLine == numLines - 1)
			break;

		if (rewrap == true)
		{
			if (wrapPos > wrapEnd)
			{
				/* This should never happen. */
				OutputDebugString("Word wrapping exceeded limits.");
				break;
			}
			if (wrapPos != wrapEnd && lineStarts[curLine+1] == wrapEnd)
			{
				/* We need more space to finish wrapping. */
				InsertLineStart(curLine + 1);
				insertedLines = true;
			}
			if (wrapPos == wrapEnd)
			{
				/* Delete any excess line start markers between the
				   current line and the word wrapping end line. */
				unsigned wrapEndLine;
				for (wrapEndLine = curLine + 1;
					 lineStarts[wrapEndLine] < wrapEnd;
					 wrapEndLine++);
				DeleteLineStarts(curLine + 1, wrapEndLine);
				if (wrapEndLine - (curLine + 1) > 0)
					deletedLines = true;
			}
			if (wrapPos == wrapEnd && wrapEnd == textSize &&
				lineStarts[numLines-1] != textSize)
			{
				/* If we did not break out of the loop earlier at the
				   end of the data, then we must add another line
				   start reference for a newline at the end of
				   the data. */
				/* Note that there is a small "gotcha" in this
				   implementation: if there is already a blank
				   newline at the end of the data, the line start
				   reference for that line will be the same as the
				   text size. */
				InsertLineStart(curLine + 1);
				insertedLines = true;
			}
			lineStarts[curLine+1] = wrapPos;
			curLine++;
		}
		else
		{
			lineStarts[numLines] = wrapPos;
			numLines++;
			curLine = numLines - 1;
			if (numLines % 20 == 0)
			{
				lineStarts = (unsigned*)realloc(lineStarts,
					sizeof(unsigned) * (numLines + 20));
			}
		}
	}
	lineStarts[numLines] = textSize;
	if ((numLines + 1) % 20 == 0)
	{
		lineStarts = (unsigned*)realloc(lineStarts,
			sizeof(unsigned) * (numLines + 21));
	}
	ReleaseDC(lastHwnd, hDC);
	if (insertedLines == false && deletedLines == false)
		lastWrappedLine = curLine + 1;
	else
		lastWrappedLine = numLines;
}

/* Computes the width of tab characters in a line.

   The cached tabStops variable is used to perform this computation
   and must be up-to-date to work as expected.  "endIndex" refers to
   the position just beyond the last character. */
static void ProcessTabs(unsigned lineNum, int* charWidths, unsigned endIndex)
{
	unsigned newLength;
	/* Temporary variables */
	unsigned i;
	unsigned j;
	newLength = 0;

	for (i = lineStarts[lineNum], j = 0; i < endIndex; i++)
	{
		if (buffer[i] == '\t')
		{
			/* Change the proper lpDx. */
			unsigned curTabStop;
			for (curTabStop = 0; curTabStop < numTabStops &&
				 newLength >= tabStops[curTabStop]; curTabStop++);
			charWidths[j] = tabStops[curTabStop] - newLength;
		}
		newLength += charWidths[j];
		j++;
	}
}

/* Breaks oversized lines in non-word wrapped mode.

   On Windows 95/98, the text rendering functions will not accept
   lines longer than 8192 characters.  Besides, lines longer than 8192
   characters are very inconvenient to have to deal with in a text
   editor. */
static void TruncateLines()
{
	unsigned wrapPos;
	unsigned i;

	/* Reset the necessary variables. */
	if (lineStarts != NULL)
	{
		free(lineStarts);
		lineStarts = NULL;
	}
	numLines = 0;
	lineStarts = (unsigned*)malloc(sizeof(unsigned) * 20);

	lineStarts[0] = 0;
	numLines++;

	/* Count through string, looking for newlines and lines in need of
	   truncating. */
	for (i = 0; i < textSize; i++)
	{
		if (buffer[i] == '\n' ||
			i - lineStarts[numLines-1] == truncLen)
		{
			/* Start a new line. */
			lineStarts[numLines] = i;
			numLines++;
			if (numLines % 20 == 0)
			{
				lineStarts = (unsigned*)realloc(lineStarts,
					sizeof(unsigned) * (numLines + 20));
			}
		}
	}
	lineStarts[numLines] = textSize;
	if ((numLines + 1) % 20 == 0)
	{
		lineStarts = (unsigned*)realloc(lineStarts,
			sizeof(unsigned) * (numLines + 20));
	}
}

/* Incomplete */
static void RetruncateLines(bool setUpdLines)
{
	/* This function is simple, mostly because it usually doesn't happen. */
	unsigned curLine;
	unsigned curPos;
	unsigned scanEnd;
	curLine = caretLine;
	curPos = lineStarts[caretLine];
	scanEnd = lineStarts[caretLine+1];

	while (curPos < scanEnd)
	{
		if (buffer[curPos] == '\n')
		{
			InsertLineStart(curLine + 1);
			lineStarts[curLine+1] = curPos;
			curLine++;
		}
		if (curPos - lineStarts[curLine] >= truncLen)
		{
			InsertLineStart(curLine + 1);
			lineStarts[curLine+1] = curPos;
			curLine++;
		}
		curPos++;
	}
	/** if (setUpdLines == true) */
		lastWrappedLine = curLine + 1;
}

/* Find the length of the longest line. */
static void LongestLineLen()
{
	HDC hDC;
	unsigned wrapPos;
	unsigned i;

	hDC = GetDC(lastHwnd);
	SelectObject(hDC, hFont);
	wrapPos = 0;
	xMaxScroll = 0;

	for (i = 0; i < numLines; i++)
	{
		GCP_RESULTS wrapRes;
		unsigned wrapStride;
		int charWidths[TRUNC_LEN];
		unsigned newLength;
		unsigned j;
		wrapStride = truncLen;
		newLength = 0;

		ZeroMemory(&wrapRes, sizeof(GCP_RESULTS));
		wrapRes.lStructSize = sizeof(GCP_RESULTS);
		wrapRes.lpDx = charWidths;
		if (wrapPos + wrapStride > textSize)
			wrapStride = textSize - wrapPos;
		wrapRes.nGlyphs = wrapStride;
		GetCharacterPlacement(hDC, &(buffer[wrapPos]), wrapStride, 0,
			&wrapRes, 0);

		for (j = 0; j < wrapStride; j++)
			newLength += wrapRes.lpDx[j];
		if (newLength > xMaxScroll)
			xMaxScroll = newLength;
	}

	ReleaseDC(lastHwnd, hDC);
}

/* Updates the cached render info.

   The render info is used to perform faster text rendering by saving
   information that is likely to get reused when redrawing the window.
   This way, the system does not have to recompute information such as
   the character widths.  Saving the render info also facilitates the
   caret positioning code. */
static void UpdateRenderInfo(bool onlyRegion, bool reindex)
{
	char* curLinePtr;
	unsigned lineSize;
	GCP_RESULTS charPlac;
	int* charWidths;
	HDC hDC;
	unsigned p1, p2;
	/* Variables used at RegRIUpdate: */
	unsigned beginLine;
	unsigned endLine;
	/* Temporary variables */
	unsigned i;

	CalcNumVisLines();
	if (riInitialized == false)
	{
		totVisLines = textAreaHeight / fontHeight;
		if (textAreaHeight % fontHeight > 0)
			totVisLines++;
		totVisLines++;
		beginUpdLine = 0;
		endUpdLine = numVisLines;
		norTextRI = (POLYTEXT*)malloc(sizeof(POLYTEXT) * totVisLines);
		for (i = 0; i < totVisLines; i++)
			norTextRI[i].pdx = NULL;
		riInitialized = true;
	}

	if (reindex == true)
		beginUpdLine = 0;

	if (endUpdLine > numVisLines)
		endUpdLine = numVisLines;

	if (onlyRegion == true)
		goto regRIUpdate;

	hDC = GetDC(lastHwnd);
	SelectObject(hDC, hFont);

	if (wrapWords == false)
	{
		unsigned i;
		for (i = 0; i < totVisLines; i++)
		{
			/* First enter crude values. */
			firstVisOffset[i] = xScrollPos;
			firstVisChars[i] = 0;
			numVisChars[i] = lineStarts[visLine+i+1] - lineStarts[visLine+i];
		}
		for (i = 0; i < numVisLines; i++)
		{
			unsigned j;
			unsigned curLen;
			curLinePtr = &(buffer[lineStarts[visLine+i]]);
			/* Then compute the fine values. */
			lineSize = numVisChars[i];
			charWidths = (int*)malloc(sizeof(int) * lineSize);
			ZeroMemory(&charPlac, sizeof(GCP_RESULTS));
			ZeroMemory(charWidths, sizeof(int) * lineSize);
			charPlac.lStructSize = sizeof(GCP_RESULTS);
			charPlac.lpDx = charWidths;
			charPlac.nGlyphs = lineSize;
			GetCharacterPlacement(hDC, curLinePtr, lineSize, 0, &charPlac, 0);
			ProcessTabs(visLine + i, charWidths, lineStarts[visLine+i] +
						lineSize);

			curLen = 0;
			for (j = 0; j < numVisChars[i] && curLen < xScrollPos; j++)
				curLen += charWidths[j];
			if (j > 0)
			{
				j--;
				curLen -= charWidths[j];
			}
			firstVisChars[i] = j;
			firstVisOffset[i] = xScrollPos - curLen;
			for (; j < numVisChars[i] &&
				 curLen < xScrollPos + textAreaWidth; j++)
				curLen += charWidths[j];
			numVisChars[i] = j - firstVisChars[i];
			free(charWidths);
		}
	}

	/* Fill in normal text render information. */
	for (i = beginUpdLine; i < numVisLines; i++)
	{
		curLinePtr = &(buffer[lineStarts[visLine+i]]);
		if (wrapWords == true)
		{
			if (lineStarts[visLine+i+1] < lineStarts[visLine+i])
			{
				/* A rewrapping discrepancy happened. */
				WrapWords(false);
				beginUpdLine = 0;
				endUpdLine = numVisLines;
				UpdateRenderInfo(onlyRegion, reindex);
				return;
			}
			lineSize = lineStarts[visLine+i+1] - lineStarts[visLine+i];
			if (lineSize > 0 && buffer[lineStarts[visLine+i+1]-1] == '\n')
				lineSize--;
		}
		else
		{
			lineSize = numVisChars[i];
			curLinePtr += firstVisChars[i];
		}

		if (wrapWords == true && i >= endUpdLine || reindex == true)
		{
			norTextRI[i].n = lineSize;
			norTextRI[i].lpstr = curLinePtr;
			continue;
		}

		charWidths = (int*)malloc(sizeof(int) * lineSize);
		ZeroMemory(&charPlac, sizeof(GCP_RESULTS));
		ZeroMemory(charWidths, sizeof(int) * lineSize);
		charPlac.lStructSize = sizeof(GCP_RESULTS);
		charPlac.lpDx = charWidths;
		charPlac.nGlyphs = lineSize;
		GetCharacterPlacement(hDC, curLinePtr, lineSize, 0, &charPlac, 0);
		ProcessTabs(visLine + i, charWidths, lineStarts[visLine+i] + lineSize);

		norTextRI[i].x = bordWidthX + leftMargin;
		if (wrapWords == false)
			norTextRI[i].x -= firstVisOffset[i];
		norTextRI[i].y = (int)bordWidthY - visLnOffset + fontHeight * i;
		norTextRI[i].n = lineSize;
		norTextRI[i].lpstr = curLinePtr;
		norTextRI[i].uiFlags = 0;
			norTextRI[i].rcl.left = 0;
			norTextRI[i].rcl.top = 0;
			norTextRI[i].rcl.right = winWidth;
			norTextRI[i].rcl.bottom = fontHeight;
		if (norTextRI[i].pdx != NULL)
		{
			free(norTextRI[i].pdx);
			norTextRI[i].pdx = NULL;
		}
		norTextRI[i].pdx = charWidths;
	}
	ReleaseDC(lastHwnd, hDC);

	if (regionActive == false)
		return;

regRIUpdate:

	/* Fill in region render information. */

	/* Sort the beginning and the end. */
	p1 = regionBegin;
	p2 = textPos;
	SortAscending(&p1, &p2);

	/* Find the number of visible region lines. */
	for (i = 0; i < numLines; i++)
	{
		if (p1 >= lineStarts[i])
			beginLine = i;
		if (p2 > lineStarts[i])
			endLine = i + 1;
		else if (p2 == lineStarts[i])
			endLine = i;
	}
	if (endLine <= visLine || beginLine >= visLine + numVisLines ||
		endLine - beginLine == 0)
	{
		/* There is no region to render. */
		numRegVisLines = 0;
		return;
	}
	if (beginLine < visLine)
	{
		beginLine = visLine;
		p1 = lineStarts[beginLine];
	}
	if (endLine > visLine + numVisLines)
	{
		endLine = visLine + numVisLines;
		p2 = lineStarts[endLine];
	}
	beginLine -= visLine;
	endLine -= visLine;
	regVisBegLn = beginLine;
	numRegVisLines = endLine - beginLine;

	/* if (regTextRI != NULL)
	{
		free(regTextRI);
		regTextRI = NULL;
	}
	regTextRI = (POLYTEXT*)malloc(sizeof(POLYTEXT) * numRegVisLines); */

	if (regionMask != NULL)
	{
		DeleteObject(regionMask);
		regionMask = NULL;
	}
	regionMask = CreateRectRgn(0, 0, 2, 2);
	if (ctRegMask != NULL)
	{
		DeleteObject(ctRegMask);
		ctRegMask = NULL;
	}

	for (i = beginLine; i < endLine; i++)
	{
		unsigned curLineStart, curLineEnd;
		unsigned numLeadChars;
		unsigned leadCharWidth;
		unsigned curLineWidth;
		/* Temporary variables */
		unsigned j;

		curLineStart = lineStarts[visLine+i];
		curLineEnd = lineStarts[visLine+i+1];
		if (wrapWords == false)
		{
			curLineStart += firstVisChars[i];
			curLineEnd = curLineStart + numVisChars[i];
		}
		if (i == beginLine)
			curLineStart = p1;
		if (i == endLine - 1)
			curLineEnd = p2;
		if (curLineEnd > 0 && buffer[curLineEnd-1] == '\n')
			curLineEnd--;
		curLinePtr = &(buffer[curLineStart]);

		if (wrapWords == true)
			lineSize = curLineEnd - curLineStart;
		else
			lineSize = curLineEnd - curLineStart;

		numLeadChars = curLineStart - lineStarts[visLine+i];
		if (wrapWords == false)
		{
			if (curLineStart > firstVisChars[i])
				numLeadChars = curLineStart -= firstVisChars[i];
			else
				numLeadChars = 0;
		}
		leadCharWidth = 0;
		for (j = 0; j < numLeadChars; j++)
			leadCharWidth += norTextRI[i].pdx[j];
		if (wrapWords == false)
			leadCharWidth -= firstVisOffset[i];

		curLineWidth = 0;
		for (j = numLeadChars; j < numLeadChars + lineSize; j++)
			curLineWidth += norTextRI[i].pdx[j];

		if (i == caretLine - visLine)
		{
			RECT rt;
			rt.left = bordWidthX + leftMargin + leadCharWidth;
			rt.top = 0;
			rt.right = rt.left + curLineWidth;
			rt.bottom = fontHeight;
			ctRegMask = CreateRectRgnIndirect(&rt);
		}

		{
			HRGN lineRgn;
			RECT rt;
			rt.left = bordWidthX + leftMargin + leadCharWidth;
			if (wrapWords == false)
				rt.left -= firstVisOffset[i];
			rt.top = (int)bordWidthY - visLnOffset + fontHeight * i;
			rt.right = rt.left + curLineWidth;
			rt.bottom = rt.top + fontHeight;
			lineRgn = CreateRectRgnIndirect(&rt);
			CombineRgn(regionMask, regionMask, lineRgn, RGN_OR);
			DeleteObject(lineRgn);
		}

		/* regTextRI[i-beginLine].x = bordWidthX + leftMargin + leadCharWidth;
		if (wrapWords == false)
			regTextRI[i-beginLine].x -= firstVisOffset[i];
		regTextRI[i-beginLine].y = (int)bordWidthY - visLnOffset +
			fontHeight * i;
		regTextRI[i-beginLine].n = lineSize;
		regTextRI[i-beginLine].lpstr = curLinePtr;
		regTextRI[i-beginLine].uiFlags = ETO_OPAQUE;
			regTextRI[i-beginLine].rcl.left = regTextRI[i-beginLine].x;
			regTextRI[i-beginLine].rcl.top = regTextRI[i-beginLine].y;
			regTextRI[i-beginLine].rcl.right = regTextRI[i-beginLine].x +
				curLineWidth;
			regTextRI[i-beginLine].rcl.bottom = regTextRI[i-beginLine].y +
				fontHeight;
		regTextRI[i-beginLine].pdx = &norTextRI[i].pdx[numLeadChars]; */
	}
}

/* Updates the scroll bar info.

   The scroll bar info needs to be updated whenever the window size,
   font face, or text buffer has changed.  This function also ensures
   that the cached scroll information is valid. */
static void UpdateScrollInfo()
{
	SCROLLINFO si;

	/* Set common parameters. */
	si.cbSize = sizeof(SCROLLINFO);
	si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE | SIF_DISABLENOSCROLL;
	si.nMin = 0;

	/* Set the vertical scroll bar. */
	/* nMax refers to the maximum valid value, not the total number of
	   elements. */
	if (numLines * fontHeight > 0)
		si.nMax = numLines * fontHeight - 1;
	else
		si.nMax = 0;
	if (textAreaHeight == 0)
		si.nPage = 1;
	else
		si.nPage = textAreaHeight;
	si.nPos = visLine * fontHeight + visLnOffset;
	SetScrollInfo(lastHwnd, SB_VERT, &si, TRUE);

	if (wrapWords == false)
	{
		/* Set the horizontal scroll bar. */
		LongestLineLen();
		si.nMax = xMaxScroll;
		si.nPage = textAreaWidth;
		si.nPos = xScrollPos;
		SetScrollInfo(lastHwnd, SB_HORZ, &si, TRUE);
	}
	else
	{
		si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
		si.nMax = 0;
		si.nPage = 0;
		si.nPos = 0;
		SetScrollInfo(lastHwnd, SB_HORZ, &si, TRUE);
	}

	/* Make sure that the visible line information is correct. */
	GetScrollInfo(lastHwnd, SB_VERT, &si);
	visLine = si.nPos / fontHeight;
	visLnOffset = si.nPos % fontHeight;
}

/* Scrolls the contents of the text editor.

   This function has two methods of specifying how to scroll: either
   by a pixel offset or by a scroll bar command.  If "offset" is true,
   then the pixel offset is specified in "amount", positive meaning
   scroll down toward the end of the text buffer, and negative
   scrolling in the opposite direction.  "otherType" specifies a
   scroll bar command.  "sBar" is either "SB_VERT" or "SB_HORZ". */
static void ScrollContents(bool offset, unsigned otherType, int amount,
						   int sBar)
{
	/* These variables are used for scrolling both horizontally and
	   vertically at once. */
	static int lastScrollAmnt;
	static bool secondRound = false;

	SCROLLINFO si;
	int sbPos;

	/* Get all the scroll bar information. */
	si.cbSize = sizeof(SCROLLINFO);
	si.fMask = SIF_ALL;
	GetScrollInfo(lastHwnd, sBar, &si);
	/* Save the position for comparison later on. */
	sbPos = si.nPos;

	if (offset == false)
	{
		switch (otherType)
		{
		/* user pressed the HOME keyboard key */
		case SB_TOP:
			si.nPos = si.nMin;
			break;
		/* user pressed the END keyboard key */
		case SB_BOTTOM:
			si.nPos = si.nMax;
			break;
		/* user pressed the top arrow */
		case SB_LINEUP:
			si.nPos -= fontHeight;
			break;
		/* user pressed the bottom arrow */
		case SB_LINEDOWN:
			si.nPos += fontHeight;
			break;
		/* user clicked the scroll bar shaft above the scroll box */
		case SB_PAGEUP:
			si.nPos -= si.nPage;
			break;
		/* user clicked the scroll bar shaft below the scroll box */
		case SB_PAGEDOWN:
			si.nPos += si.nPage;
			break;
		/* user dragged the scroll box */
		case SB_THUMBTRACK:
			si.nPos = si.nTrackPos;
			break;
		}
	}
	else
		si.nPos += amount;

	/* Set the position and then retrieve it. Due to adjustments by
	   Windows it may not be the same as the value set. */
	si.fMask = SIF_POS;
	SetScrollInfo(lastHwnd, sBar, &si, TRUE);
	GetScrollInfo(lastHwnd, sBar, &si);
	/* If the position has changed, scroll window and update it. */
	if (si.nPos != sbPos)
	{
		RECT rt;
		/* Scroll the render info structures (if necessary). */
		if (sBar == SB_VERT || secondRound == true)
		{
			visLine = si.nPos / fontHeight;
			visLnOffset = si.nPos % fontHeight;
			/** if (ScrollRenderStructs(true, si.nPos - sbPos)) */
			beginUpdLine = 0;
			endUpdLine = totVisLines;
			UpdateRenderInfo(false, false);
		}
		if (sBar == SB_HORZ || sBar == SB_BOTH && secondRound == false)
		{
			xScrollPos = si.nPos;
			/** if (ScrollRenderStructs(false, si.nPos - sbPos)) */
				UpdateRenderInfo(false, false);
		}
		/** endUpdLine = 0; */

		/* Scroll the window bits. */
		rt.left = bordWidthX + leftMargin;
		rt.top = bordWidthY;
		rt.right = winWidth - bordWidthX - rightMargin;
		rt.bottom = winHeight - bordWidthY;
		if (sBar == SB_VERT)
		{
			ScrollWindowEx(lastHwnd, 0, sbPos - si.nPos, &rt, &rt,
				NULL, NULL, SW_INVALIDATE | SW_ERASE);
		}
		if (sBar == SB_HORZ)
		{
			ScrollWindowEx(lastHwnd, sbPos - si.nPos, 0, &rt, &rt,
				NULL, NULL, SW_INVALIDATE | SW_ERASE);
		}
		if (sBar == SB_BOTH)
		{
			if (secondRound == false)
			{
				secondRound = true;
				lastScrollAmnt = sbPos - si.nPos;
			}
			else
			{
				secondRound = false;
				ScrollWindowEx(lastHwnd, lastScrollAmnt, sbPos - si.nPos,
					&rt, &rt, NULL, NULL, SW_INVALIDATE | SW_ERASE);
			}
		}
		UpdateCaretPos();
	}
}

/* Scrolls the render structures to avoid recomputations.

   This function should only be called by ScrollContents(), since it
   does not update the scroll bar information. */
/* Incomplete */
static bool ScrollRenderStructs(bool vert, int offset)
{
	if (vert == true)
	{
		int offSize;
		int offLines;
		unsigned oldNumVisLines;
		offSize = visLnOffset + offset;

		if (offSize > 0)
		{
			offLines = offSize / fontHeight;
			visLine += offLines;
			visLnOffset = offSize % fontHeight;
		}
		else if (offSize < 0)
		{
			offLines = offSize / fontHeight;
			visLnOffset = fontHeight + offSize % fontHeight;
			if (visLnOffset != fontHeight)
				offLines--;
			else
				visLnOffset = 0;
			visLine += offLines;
		}
		else
			return false;

		/* Calculate the number of visible lines. */
		oldNumVisLines = numVisLines;
		CalcNumVisLines();

		/* Scroll render info structures and mark update regions. */
		if (offLines >= 0)
		{
			unsigned i;
			for (i = oldNumVisLines - offLines; i < oldNumVisLines; i++)
				free(norTextRI[i].pdx);
			if (numVisLines > oldNumVisLines)
			{
				norTextRI = (POLYTEXT*)realloc(norTextRI,
					sizeof(POLYTEXT) * numVisLines);
			}
			memmove(norTextRI, &norTextRI[offLines],
				sizeof(POLYTEXT) * (oldNumVisLines - offLines));
			beginUpdLine = oldNumVisLines - offLines;
			endUpdLine = numVisLines;
		}
		else
		{
			unsigned i;
			for (i = 0; i < -offLines; i++)
				free(norTextRI[i].pdx);
			if (numVisLines > oldNumVisLines)
			{
				norTextRI = (POLYTEXT*)realloc(norTextRI,
					sizeof(POLYTEXT) * numVisLines);
				memmove(&norTextRI[-offLines+1], norTextRI,
					sizeof(POLYTEXT) * (oldNumVisLines + offLines));
				endUpdLine = -offLines + 1;
			}
			else
			{
				memmove(&norTextRI[-offLines], norTextRI,
					sizeof(POLYTEXT) * (oldNumVisLines + offLines));
				endUpdLine = -offLines;
			}
			beginUpdLine = 0;
		}

		return true;
	}
	else
	{
		/* We only scroll vertical render information. */
		return true;
		/* bool retVal = false;
		xScrollPos += offset;

		for (unsigned i = 0; i < numVisLines; i++)
		{
			/* Count up character widths until xScrollPos is exceeded. *
			/* Go back one. *
			/* Find the difference between xScrollPos and the current length. *
		}
		return retVal; */
	}
	return false;
}

/* Processes middle mouse button scrolling.

   When the middle mouse button is pressed, a pan icon is displayed in
   the window at the pressed position and the user can move the mouse
   cursor away from it to control scroll speed. */
static void MButtonScroll(bool setCursor)
{
	if (setCursor == true)
	{
		bool down, up, right, left;
		unsigned pick;
		down = false; up = false; right = false; left = false;
		if (lastMousePos.y > dragP.y + 16)
			down = true;
		else if (lastMousePos.y < dragP.y - 16)
			up = true;
		if (lastMousePos.x > dragP.x + 16)
			right = true;
		else if (lastMousePos.x < dragP.x - 16)
			left = true;
		if (wrapWords == true)
		{
			if ((up || down) == false)
			{
				up = true;
				down = true;
			}
			right = false;
			left = false;
		}
		pick = (int)down + ((int)up << 1) + ((int)right << 2) +
			((int)left << 3);
		SetCursor(panCurs[panCursMap[pick]]);
	}
	else
	{
		static int modX = 0, modY = 0; /* Holds accumulated division
										  remainders */
		const int speedFactor = 4; /* Controls acceleration */

		int dragDistX;
		int dragDistY;
		dragDistX = lastMousePos.x - dragP.x;
		dragDistY = lastMousePos.y - dragP.y;
		if (dragDistX < -16 || 16 < dragDistX)
		{
			modX += dragDistX % speedFactor;
			dragDistX /= speedFactor;
		}
		else
			dragDistX = 0;
		if (dragDistY < -16 || 16 < dragDistY)
		{
			if (dragDistY < -16)
				dragDistY += 16;
			if (16 < dragDistY)
				dragDistY -= 16;
			modY += dragDistY % speedFactor;
			dragDistY /= speedFactor;
			if (modY >= speedFactor)
			{
				modY -= speedFactor;
				dragDistY++;
			}
			if (modY <= -speedFactor)
			{
				modY += speedFactor;
				dragDistY--;
			}
		}
		else
			dragDistY = 0;
		if (wrapWords == true)
		{
			RECT rt;
			rt.left = -16 + dragP.x;
			rt.right = 16 + dragP.x;
			rt.top = -16 + dragP.y;
			rt.bottom = 16 + dragP.y;
			if (dragDistY != 0)
			{
				InvalidateRect(lastHwnd, &rt, TRUE);
				rt.top -= dragDistY;
				rt.bottom -= dragDistY;
				InvalidateRect(lastHwnd, &rt, TRUE);
				ZeroMemory(&updRect, sizeof(RECT));
			}
			ScrollContents(true, 0, dragDistY, SB_VERT);
		}
		else
		{
			if (modX >= speedFactor)
			{
				modX -= speedFactor;
				dragDistX++;
			}
			if (modX <= -speedFactor)
			{
				modX += speedFactor;
				dragDistX--;
			}
			ScrollContents(true, 0, dragDistX, SB_BOTH);
			ScrollContents(true, 0, dragDistY, SB_BOTH);
		}
	}
}

/* Ends middle mouse button scrolling. */
static void MScrollEnd()
{
	RECT rt;

	if (drawDrag == false)
		return;

	mBtnTimeUp = false;
	drawDrag = false;
	/* dragDist = 0; */
	KillTimer(lastHwnd, 1);
	KillTimer(lastHwnd, 2);
	rt.left = dragP.x - 16;
	rt.top = dragP.y - 16;
	rt.right = dragP.x + 16;
	rt.bottom = dragP.y + 16;
	InvalidateRect(lastHwnd, &rt, TRUE);
	ZeroMemory(&updRect, sizeof(RECT));
	SendMessage(lastHwnd, WM_SETCURSOR, 0, 0);
}

/* Processes mouse wheel scroll messages.

   Although this code has support for smooth mouse wheels, that
   portion of the code was not yet tested, neither indirectly nor
   directly. */
static void MWheelScroll(short amount)
{
	static int pixelMod = 0; /* Accumulated partial pixel scrolls */
	int wheelScroll;
	int offset;
	wheelScroll = -amount;

	if (wheelLines == WHEEL_PAGESCROLL)
		offset = textAreaHeight;
	else
		offset = fontHeight * wheelLines;
	offset *= wheelScroll;
	offset += pixelMod;
	/* Store the fractional scroll pixels not accounted for. */
	pixelMod = offset % WHEEL_DELTA;
	/* Finalize offset calculation. */
	offset /= WHEEL_DELTA;

	/* According to the Platform SDK documentation, if the number of
	   mouse wheel scroll lines is greater than the window area, it
	   should be interpreted as a page up or page down. */
	if (offset > (int)textAreaHeight)
		offset = textAreaHeight;
	if (offset < -(int)textAreaHeight)
		offset = -(int)textAreaHeight;

	ScrollContents(true, 0, offset, SB_VERT);
}

/* General purpose text insert function.

   This function can insert either a single character or a string of
   characters.  Only one of the insertion methods should be used.

   The parameter "silent" suppresses updating the undo queue if set to
   true.  This parameter should probably only be used for the undo and
   redo functions. */
static void InsertText(char c, const char* str, bool silent)
{
	if (c != 0)
	{
		/* Temporary variables */
		unsigned i;
		/* Add the character. */
		if (overwrite == false || buffer[textPos] == '\n' ||
			buffer[textPos] == '\0')
		{
			textSize++;
			if (textSize >= buffSize)
			{
				buffSize += 1000;
				buffer = (char*)realloc(buffer, buffSize);
				UpdateRenderInfo(false, true);
			}
			if (textPos != (textSize - 1))
			{
				memmove(&(buffer[textPos+1]), &(buffer[textPos]),
					textSize - textPos);
			}
			else
				buffer[textPos+1] = '\0';

			/* Correct all line beginning indices. */
			for (i = caretLine + 1; i < numLines + 1; i++)
				lineStarts[i]++;
		}
		buffer[textPos] = c;
		textPos++;

		/* Check for undo addition. */
		if (silent == false && wasInsert == true && sameUndoOp == true)
		{
			char* curUndoData;
			unsigned dataLen;
			curUndoData = undoHistory[curUndo].data;
			dataLen = strlen(curUndoData) + 2;
			curUndoData = (char*)realloc(curUndoData, dataLen);
			curUndoData[dataLen-2] = c;
			curUndoData[dataLen-1] = '\0';
			/* Make sure the reallocation is captured. */
			undoHistory[curUndo].data = curUndoData;
		}
		else if (silent == false)
		{
			AddUndo();
			undoHistory[curUndo].type = UE_DELETE;
			undoHistory[curUndo].data = (char*)malloc(2);
			undoHistory[curUndo].data[0] = c;
			undoHistory[curUndo].data[1] = '\0';
			undoHistory[curUndo].opPos = textPos - 1;
			sameUndoOp = true;
		}
	}
	else
	{
		unsigned insertLen;
		/* Temporary variables */
		unsigned i;
		/* Allocate proper space. */
		insertLen = strlen(str);
		if (textSize + insertLen >= buffSize)
		{
			buffSize = textSize + insertLen;
			buffSize += 1000 - buffSize % 1000;
			buffer = (char*)realloc(buffer, buffSize);
			UpdateRenderInfo(false, true);
		}
		if (textPos < textSize)
			memmove(&(buffer[textPos+insertLen]), &(buffer[textPos]),
				textSize - textPos);
		else
			buffer[textPos+insertLen] = '\0';

		/* Paste the string in. */
		strncpy(&(buffer[textPos]), str, insertLen);
		textSize += insertLen;
		textPos += insertLen;

		/* Correct all line beginning indices. */
		for (i = caretLine + 1; i < numLines + 1; i++)
			lineStarts[i] += insertLen;

		/* Add an undo entry. */
		if (silent == false)
		{
			AddUndo();
			undoHistory[curUndo].type = UE_DELETE;
			undoHistory[curUndo].data = (char*)malloc(insertLen + 1);
			strcpy(undoHistory[curUndo].data, str);
			undoHistory[curUndo].opPos = textPos - insertLen;
			sameUndoOp = false;
		}
	}

	/* Configure miscellaneous flags. */
	wasInsert = true;

	beginUpdLine = caretLine;

	if (wrapWords == true)
		WrapWords(true);
	else
		RetruncateLines(false);

	/* Update the window. */
	CalcCaretLine();
	UpdateScrollInfo();
	ScrollToCaret();
	if (beginUpdLine < visLine)
		beginUpdLine = visLine;
	beginUpdLine -= visLine;
	endUpdLine = lastWrappedLine - visLine;
	InvalidateLines();
	UpdateRenderInfo(false, false);

	/* Update the caret. */
	CalcCaretPos();
	UpdateCaretPos();
}

/* General purpose text delete function.

   "p1" and "p2" need not be presorted in ascending order, as this
   function will perform the sorting as necessary.

   The parameter "silent" suppresses updating the undo queue if set to
   true.  This parameter should probably only be used for the undo and
   redo functions. */
static void DeleteText(unsigned p1, unsigned p2, bool silent)
{
	bool deletedLines;
	unsigned lastNumLines;
	unsigned p1Line, p2Line;
	unsigned lastCaretLine;
	deletedLines = false;
	SortAscending(&p1, &p2);

	/* Check for undo addition. */
	if (silent == false)
	{
		if (p2 - p1 == 1)
		{
			if (wasInsert == false && sameUndoOp == true)
			{
				char* curUndoData;
				unsigned dataLen;
				curUndoData = undoHistory[curUndo].data;
				dataLen = strlen(curUndoData) + 2;
				curUndoData = (char*)realloc(curUndoData, dataLen);
				if (p1 < undoHistory[curUndo].opPos)
				{
					/* Insert at the beginning of the string (move the
					   null character too). */
					memmove(&curUndoData[1], curUndoData, dataLen - 1);
					curUndoData[0] = buffer[p1];
					undoHistory[curUndo].opPos--;
				}
				else
				{
					curUndoData[dataLen-2] = buffer[p1];
					curUndoData[dataLen-1] = '\0';
				}
				/* Make sure the reallocation is captured. */
				undoHistory[curUndo].data = curUndoData;
			}
			else
			{
				AddUndo();
				undoHistory[curUndo].type = UE_INSERT;
				undoHistory[curUndo].data = (char*)malloc(2);
				undoHistory[curUndo].data[0] = buffer[p1];
				undoHistory[curUndo].data[1] = '\0';
				undoHistory[curUndo].opPos = p1;
				sameUndoOp = true;
			}
		}
		else
		{
			unsigned delLen;
			delLen = p2 - p1;
			AddUndo();
			undoHistory[curUndo].type = UE_INSERT;
			undoHistory[curUndo].data = (char*)malloc(delLen + 1);
			strncpy(undoHistory[curUndo].data, &(buffer[p1]), delLen);
			undoHistory[curUndo].data[delLen] = '\0';
			undoHistory[curUndo].opPos = p1;
			sameUndoOp = false;
		}
	}

	/* Delete the range of lines that definitely will become invalid. */
	lastNumLines = numLines;
	for (p1Line = 0; p1Line < numLines; p1Line++)
	{
		if (p1 < lineStarts[p1Line])
			break;
	}
	p1Line--;
	for (p2Line = p1Line; p2Line < numLines; p2Line++)
	{
		if (p2 < lineStarts[p2Line])
			break;
	}
	p2Line--;
	DeleteLineStarts(p1Line + 1, p2Line + 1);
	if (p2Line > p1Line)
		deletedLines = true;

	/* Erase the region from p1 to p2 (move null character too). */
	memmove(&(buffer[p1]), &(buffer[p2]), textSize + 1 - p2);
	textSize -= (p2 - p1);
	if (textPos >= p2)
		textPos -= (p2 - p1);
	else if (textPos > p1)
		textPos = p1;

	/* Correct line beginning indices after the deleted text. */
	{
		unsigned i;
		for (i = p1Line + 1; i < numLines + 1; i++)
			lineStarts[i] -= (p2 - p1);
	}

	/** if (p2 - p1 != 1)
		CalcCaretLine(); */
	caretLine = p1Line;

	/* Configure miscellaneous flags. */
	wasInsert = false;

	beginUpdLine = caretLine;

	lastCaretLine = caretLine;

	if (wrapWords == true)
		WrapWords(true);
	else
		RetruncateLines(false);

	if (deletedLines == true)
		lastWrappedLine = numLines; /* Since we deleted line start
									   indices ourselves */

	/* Update the window. */
	if (lastNumLines + 2 > totVisLines && lastNumLines > numLines &&
		visLine + numVisLines > numLines)
	{
		ScrollContents(true, 0, -(int)(((visLine + numVisLines) - numLines) *
			fontHeight), SB_VERT);
		UpdateScrollInfo();
		beginUpdLine = 0;
		endUpdLine = (visLine + numVisLines) - numLines;
		if (visLnOffset > 0)
			endUpdLine++;
		if (deletedLines == true)
		{
			/** if (beginUpdLine < visLine)
				beginUpdLine = visLine;
			beginUpdLine -= visLine; */
			endUpdLine = lastWrappedLine - visLine;
			if (lastWrappedLine == numLines &&
				numVisLines + 1 < totVisLines)
				InvalidateRect(lastHwnd, NULL, TRUE);
			InvalidateLines();
		}
	}
	else
	{
		UpdateScrollInfo();
		if (beginUpdLine < visLine)
			beginUpdLine = visLine;
		beginUpdLine -= visLine;
		endUpdLine = lastWrappedLine - visLine;
		InvalidateLines();
	}
	UpdateRenderInfo(false, false);

	/* Update the caret. */
	CalcCaretPos();
	UpdateCaretPos();
}

/* Marks a range of lines for updating.

   The shared variables "beginUpdLine" and "endUpdLine" specify the
   range of lines to invalidate.

   This function performs validity checks on "beginUpdLine" and
   "endUpdLine", so the calling function does not have to perform as
   many checks. */
static void InvalidateLines()
{
	RECT rt;
	if (endUpdLine <= beginUpdLine)
		endUpdLine = beginUpdLine + 1;
	if (caretLine >= visLine && endUpdLine <= caretLine - visLine)
		endUpdLine = caretLine + 1 - visLine;
	if (endUpdLine > totVisLines)
		endUpdLine = totVisLines;
	rt.left = 0;
	rt.right = winWidth;
	rt.top = (int)bordWidthY + beginUpdLine * fontHeight - visLnOffset;
	rt.bottom = (int)bordWidthY - visLnOffset;
	/* This is used if (numLines < totVisLines && oldNumLines >
	   numLines). */
	/* Otherwise, the last line that got deleted does not get erased
	   from screen. */
	if (visLine + numVisLines > numLines)
		rt.bottom += numVisLines * fontHeight;
	else
		rt.bottom += endUpdLine * fontHeight;
	InvalidateRect(lastHwnd, &rt, TRUE);
	CopyMemory(&updRect, &rt, sizeof(RECT));
}

/* Processes various non-inserting keyboard keys.

   This function should only be called from TextEditProc(). */
/* Incomplete */
static void ProcessMiscKey(WPARAM wParam)
{
	unsigned oldTextPos;
	bool cPosCalc;
	bool keybSetRegion;
	cPosCalc = true;
	keybSetRegion = false;

	oldTextPos = textPos;
	if (wParam == VK_HOME || wParam == VK_END ||
		wParam == VK_PRIOR || wParam == VK_NEXT ||
		wParam == VK_UP || wParam == VK_DOWN ||
		wParam == VK_LEFT || wParam == VK_RIGHT)
	{
		sameUndoOp = false;
		if (!(GetKeyState(VK_SHIFT) & 0x8000))
			UpdateRegion(false);
		else
		{
			if (regionActive == false)
				regionBegin = textPos;
			beginUpdLine = caretLine;
			keybSetRegion = true;
		}
	}

	if (GetKeyState(VK_CONTROL) & 0x8000 && wParam != VK_INSERT &&
		wParam != VK_DELETE)
	{
		switch (wParam)
		{
		case VK_HOME: /* Beginning of file */
			textPos = 0;
			break;
		case VK_END: /* End of file */
			textPos = textSize;
			break;
		case VK_PRIOR: /* Move caret to top of screen */
			lastMousePos.x = bordWidthX + leftMargin + caretX;
			lastMousePos.y = (int)bordWidthY - visLnOffset;
			TextPosFromPoint();
			cPosCalc = false;
			break;
		case VK_NEXT: /* Move caret to bottom of screen */
			lastMousePos.x = bordWidthX + leftMargin + caretX;
			lastMousePos.y = (int)bordWidthY + (numVisLines - 1) *
				fontHeight - visLnOffset;
			TextPosFromPoint();
			cPosCalc = false;
			break;
		case VK_UP: /* Scroll view one line up */
		{
			if (caretLine == 0)
				break;
			/** lastMousePos.x = bordWidthX + leftMargin + caretX;
			lastMousePos.y = (int)bordWidthY + fontHeight *
				(caretLine - 1 - visLine) - visLnOffset;
			TextPosFromPoint(); */
			ScrollContents(false, SB_LINEUP, 0, SB_VERT);
			if (caretLine >= visLine + numVisLines)
			{
				lastMousePos.x = bordWidthX + leftMargin + caretX;
				lastMousePos.y = (int)bordWidthY + fontHeight *
					(numVisLines - 1) - visLnOffset;
				TextPosFromPoint();
				cPosCalc = false;
			}
			/** cPosCalc = false; */
			break;
		}
		case VK_DOWN: /* Scroll view one line down */
		{
			/** lastMousePos.x = bordWidthX + leftMargin + caretX;
			lastMousePos.y = (int)bordWidthY + fontHeight *
				(caretLine + 1 - visLine) - visLnOffset;
			TextPosFromPoint(); */
			ScrollContents(false, SB_LINEDOWN, 0, SB_VERT);
			if (caretLine < visLine)
			{
				lastMousePos.x = bordWidthX + leftMargin + caretX;
				lastMousePos.y = (int)bordWidthY;
				TextPosFromPoint();
				cPosCalc = false;
			}
			/** cPosCalc = false; */
			break;
		}
		case VK_LEFT: /* Move one word left */
			SkipWord(false);
			CalcCaretLine();
			ScrollToCaret();
			break;
		case VK_RIGHT: /* Move one word right */
			SkipWord(true);
			CalcCaretLine();
			ScrollToCaret();
			break;
		}
	}
	else
	{
		switch (wParam)
		{
		case VK_INSERT: /* Insert/overwrite mode */
			overwrite = !overwrite;
			cPosCalc = false;
			break;
		case VK_DELETE: /* Delete character */
			if (regionActive == false)
			{
				if (textPos == textSize)
					break;
				regionBegin = textPos;
				if (GetKeyState(VK_CONTROL) & 0x8000)
					SkipWord(true);
				else
					textPos++;
			}
			else
				UpdateRegion(false);
			DeleteText(regionBegin, textPos, false);
			cPosCalc = false;
			break;
		case VK_HOME: /* Beginning of line */
			textPos = lineStarts[caretLine];
			break;
		case VK_END: /* End of line */
			if (caretLine == numLines - 1)
				textPos = textSize;
			else
				textPos = lineStarts[caretLine+1] - 1;
			break;
		case VK_PRIOR: /* Page up */
			if (textAreaHeight / fontHeight > caretLine)
				caretLine = 0;
			else
				caretLine -= textAreaHeight / fontHeight;
			textPos = lineStarts[caretLine];
			CalcCaretLine();
			/** ScrollToCaret(); */
			ScrollContents(false, SB_PAGEUP, 0, SB_VERT);
			break;
		case VK_NEXT: /* Page down */
			caretLine += textAreaHeight / fontHeight;
			if (caretLine > numLines - 1)
				caretLine = numLines - 1;
			textPos = lineStarts[caretLine];
			CalcCaretLine();
			/** ScrollToCaret(); */
			ScrollContents(false, SB_PAGEDOWN, 0, SB_VERT);
			break;
		case VK_UP: /* Move caret up */
		{
			if (caretLine == 0)
				break;
			lastMousePos.x = bordWidthX + leftMargin + caretX;
			lastMousePos.y = (int)bordWidthY + fontHeight *
				(caretLine - 1 - visLine) - visLnOffset;
			TextPosFromPoint();
			cPosCalc = false;
			break;
		}
		case VK_DOWN: /* Move caret down */
		{
			lastMousePos.x = bordWidthX + leftMargin + caretX;
			lastMousePos.y = (int)bordWidthY + fontHeight *
				(caretLine + 1 - visLine) - visLnOffset;
			TextPosFromPoint();
			cPosCalc = false;
			break;
		}
		case VK_LEFT: /* Move caret left */
			if (textPos == 0)
				break;
			textPos--;
			CalcCaretLine();
			ScrollToCaret();
			break;
		case VK_RIGHT: /* Move caret right */
			if (textPos == textSize)
				break;
			textPos++;
			CalcCaretLine();
			ScrollToCaret();
			break;
		}
	}
	if (cPosCalc == true)
	{
		CalcCaretPos();
		UpdateCaretPos();
	}
	if (keybSetRegion == true)
	{
		/* Incomplete bug fix */
		if (endUpdLine != 0 && (GetKeyState(VK_CONTROL) & 0x8000) &&
			(wParam == VK_UP || wParam == VK_DOWN))
		{
			InvalidateLines();
			UpdateWindow(lastHwnd);
		}
		endUpdLine = caretLine + 1;
		if (beginUpdLine < visLine)
			beginUpdLine = visLine;
		beginUpdLine -= visLine;
		endUpdLine -= visLine;
		UpdateRegion(true);
	}
}

/* Adds space for an undo entry to the undo queue.

   Once this function is called, variables at "undoHistory[curUndo]"
   should be set as necessary. */
static void AddUndo()
{
	if (numUndo == 0)
	{
		undoHistory = (UndoEntry*)malloc(sizeof(UndoEntry) * 20);
		numUndo++;
	}
	else if (curUndo != numUndo - 1)
	{
		/* Delete all the subsequent undos. */
		unsigned lastUndo;
		for (lastUndo = numUndo - 1; lastUndo > curUndo; lastUndo--)
		{
			free(undoHistory[lastUndo].data);
		}
		/* We want the allocation to be divisible by 20. */
		numUndo = curUndo + 2;
		undoHistory = (UndoEntry*)realloc(undoHistory, sizeof(UndoEntry) *
										  (numUndo + (20 - numUndo % 20)));
		curUndo++;
	}
	else
	{
		numUndo++;
		if (numUndo % 20 == 0)
		{
			undoHistory = (UndoEntry*)realloc(undoHistory,
				sizeof(UndoEntry) * (numUndo + 20));
		}
		curUndo++;
	}
	undoHistory[curUndo].data = NULL;
	undoHistory[curUndo].oldData = NULL;
	SendMessage(GetParent(lastHwnd), NM_CANUNDO, 0, TRUE);
	SendMessage(GetParent(lastHwnd), NM_CANUNDO, 1, FALSE);
}

/* Inserts a line start before the specified line.

   This function should only be used by WrapWords() and
   TruncateLines(), as those are the only functions that should modify
   the line starts cache. */
static void InsertLineStart(unsigned line)
{
	numLines++;
	if ((numLines + 1) % 20 == 0)
	{
		lineStarts = (unsigned*)realloc(lineStarts,
			sizeof(unsigned) * (numLines + 21));
	}
	memmove(&lineStarts[line+1], &lineStarts[line], sizeof(unsigned) *
		(numLines - line));
	lineStarts[numLines] = textSize;
}

/* Deletes the line starts from the range starting at "begin" to the
   element before "end".

   This function should only be used by WrapWords() and
   TruncateLines(), as those are the only functions that should modify
   the line starts cache. */
static void DeleteLineStarts(unsigned begin, unsigned end)
{
	memmove(&lineStarts[begin], &lineStarts[end], sizeof(unsigned) *
		(numLines - end));
	if (end - begin >= numLines)
		numLines = 1;
	else
		numLines -= end - begin;
	if (begin == 0)
		lineStarts[0] = 0;
	lineStarts[numLines] = textSize;
}

/* Copies the region to the clipboard. */
static void CopyRegion()
{
	if (OpenClipboard(lastHwnd))
	{
		unsigned p1, p2;
		unsigned numNewlines;
		HANDLE clipData;
		char* clipLock;
		/* Temporary variables */
		unsigned i;
		unsigned j;
		if (regionBegin < textPos)
		{
			p1 = regionBegin;
			p2 = textPos;
		}
		else
		{
			p1 = textPos;
			p2 = regionBegin;
		}
		/* We will need to convert any newlines to carriage-return
		   linefeed pairs. */
		numNewlines = 0;
		for (i = p1; i < p2; i++)
		{
			if (buffer[i] == '\n')
				numNewlines++;
		}
		clipData = GlobalAlloc(GMEM_MOVEABLE, p2 - p1 + 1 + numNewlines);
		clipLock = (char*)GlobalLock(clipData);
		memcpy(clipLock, buffer + p1, p2 - p1);
		for (i = p1, j = 0; i < p2; i++)
		{
			if (buffer[i] == '\n')
			{
				clipLock[j] = '\r';
				j++;
				clipLock[j] = '\n';
			}
			else
				clipLock[j] = buffer[i];
			j++;
		}
		clipLock[p2-p1+numNewlines] = '\0';
		GlobalUnlock(clipData);
		EmptyClipboard();
		SetClipboardData(CF_TEXT, clipData);
		CloseClipboard();
	}
	else
		MessageBeep(MB_OK);
}

/* Skips the caret before or after a nearby word. */
static void SkipWord(bool forward)
{
	if (forward == true)
	{
		while (buffer[textPos] != ' ' &&
			   buffer[textPos] != '\t' &&
			   buffer[textPos] != '\n' && textPos != textSize)
			textPos++;
		if (textPos + 1 < textSize && (buffer[textPos] == ' ' ||
			buffer[textPos] == '\t' || buffer[textPos] == '\n'))
			textPos++;
	}
	else
	{
		if (textPos > 0 && (buffer[textPos-1] == ' ' ||
			buffer[textPos-1] == '\t' || buffer[textPos-1] == '\n'))
			textPos--;
		if (textPos > 0)
			textPos--;
		while (buffer[textPos] != ' ' &&
			   buffer[textPos] != '\t' &&
			   buffer[textPos] != '\n' && textPos != 0)
			textPos--;
		if (buffer[textPos] == ' ' ||
			buffer[textPos] == '\t' ||
			buffer[textPos] == '\n')
			textPos++;
	}
}

/* Calculates the caret line by searching from the first line
   start. */
static void CalcCaretLine()
{
	unsigned newCaretLine;
	for (newCaretLine = 0; newCaretLine < numLines; newCaretLine++)
	{
		if (textPos < lineStarts[newCaretLine])
			break;
	}
	newCaretLine--;
	caretLine = newCaretLine;
}

/* Calculates the screen position of the caret from the text
   position.

   The calculation is not completed if the caret is not visible. */
/* Incomplete */
static void CalcCaretPos()
{
	if (norTextRI == NULL)
		return;
	/* Find which line the caret is on. */
	CalcCaretLine();

	/* If the caret is not visible, do not show it. */
	if (caretLine >= visLine + numVisLines ||
		caretLine < visLine)
	{
		caretVisible = false;
		return;
	}
	caretVisible = true;

	/* Find the x position of the caret. */
	caretX = 0;
	if (wrapWords == true)
	{
		unsigned i;
		for (i = lineStarts[caretLine]; i < textPos; i++)
			caretX +=
				norTextRI[caretLine-visLine].pdx[i-lineStarts[caretLine]];
	}
	else if (caretLine >= visLine && caretLine < visLine + numVisLines)
	{
		unsigned i;
		unsigned lineStart;
		caretX = 0;
		lineStart = lineStarts[caretLine] + firstVisChars[caretLine-visLine];
		for (i = lineStart; i < textPos; i++)
			caretX +=
				norTextRI[caretLine-visLine].pdx[i-lineStart];
		if (firstVisOffset[caretLine] > caretX)
		{
			caretVisible = false;
			return;
		}
		caretX -= firstVisOffset[caretLine-visLine];
	}
	if (wrapWords == true && caretX > textAreaWidth)
		caretX = textAreaWidth;
}

/* Updates the caret screen position.

   The cached values must be up-to-date for this function to work. */
static void UpdateCaretPos()
{
	caretY = (caretLine + caretLnOffst - visLine) * fontHeight - visLnOffset;
	if (wrapWords == true)
		SetCaretPos(caretX + leftMargin + bordWidthX, caretY + bordWidthY);
	if (wrapWords == false && caretLine >= visLine &&
		caretLine < visLine + numVisLines)
	{
		unsigned i;
		unsigned lineStart;
		caretX = 0;
		lineStart = lineStarts[caretLine] + firstVisChars[caretLine-visLine];
		for (i = lineStart; i < textPos; i++)
			caretX +=
				norTextRI[caretLine-visLine].pdx[i-lineStart];
		caretX -= firstVisOffset[caretLine-visLine];
		SetCaretPos(caretX + leftMargin + bordWidthX, caretY + bordWidthY);
	}
}

/* Computes the text position from a screen position specified in
   "lastMousePos".

   This function can also be used to facilitate other functions, such
   as moving the caret up and down with the arrow keys, by setting the
   "lastMousePos" shared variable appropriately. */
/* Incomplete */
static void TextPosFromPoint()
{
	unsigned oldX, oldY;
	int xPos;
	int yPos;
	int lineTest;
	unsigned lineEndRef;
	unsigned visHitLine;
	oldX = caretX; oldY = caretY;
	xPos = lastMousePos.x;
	yPos = lastMousePos.y;
	xPos -= (leftMargin + bordWidthX);
	yPos -= bordWidthY;
	if (xPos < 0) xPos = 0;
	/** if (yPos < 0) yPos = 0; */
	lineTest = (yPos + (int)visLnOffset) / (int)fontHeight;

	/* Make sure the caret is on a valid line. */
	if (lineTest < 0 && (unsigned)(-lineTest) > visLine)
		caretLine = 0;
	else if (lineTest < 0)
		caretLine = (unsigned)(visLine - (unsigned)(-lineTest));
	else
		caretLine = (unsigned)(visLine + lineTest);
	if (caretLine > numLines - 1)
		caretLine = numLines - 1;

	if (caretLine == numLines - 1)
		lineEndRef = textSize;
	else
		lineEndRef = lineStarts[caretLine+1];

	/* Scroll vertically, if necessary. */
	ScrollToCaret();

	/* Scroll horizontally, if necessary. */
	/* if (numVisChars[caretLine-visLine] == 0)
	{
		/* Find the end of this line. *
		/* Scroll so that it is in view. *
		ScrollContents(false, NULL, 0, SB_HORZ);
	}
	if (xPos < 0 || xPos >= textAreaWidth)
	{
		/* Check that there is a valid character in that direction. *
		/* If there is, scroll so that it is in view. *
	} */

	/* Find all the characters behind the caret. */
	caretX = 0;
	visHitLine = caretLine - visLine;
	if (wrapWords == true)
	{
	for (textPos = lineStarts[caretLine]; textPos < lineEndRef &&
		 caretX < xPos; textPos++)
	{
		if (buffer[textPos] == '\n')
			break;
		caretX += norTextRI[visHitLine].pdx[textPos-lineStarts[caretLine]];
	}

	/* Perform proper rounding. */
	if (textPos <= lineEndRef && caretX != 0 && buffer[textPos] != '\n')
	{
		POLYTEXT* hitRI;
		int midChar;
		hitRI = &norTextRI[visHitLine];
		textPos--;
		caretX -= hitRI->pdx[textPos-lineStarts[caretLine]];
		midChar = xPos - caretX;
		if (midChar > hitRI->pdx[textPos-lineStarts[caretLine]] / 2)
		{
			caretX += hitRI->pdx[textPos-lineStarts[caretLine]];
			textPos++;
		}
	}
	}
	else
	{
	for (textPos = lineStarts[caretLine] + firstVisChars[visHitLine]; textPos < lineEndRef &&
		 caretX < xPos + firstVisOffset[visHitLine]; textPos++)
	{
		if (buffer[textPos] == '\n')
			break;
		caretX += norTextRI[visHitLine].pdx[textPos-lineStarts[caretLine]-firstVisChars[visHitLine]];
	}
	caretX -= firstVisOffset[visHitLine];

	/* Perform proper rounding. */
	if (textPos <= lineEndRef && caretX != 0 && buffer[textPos] != '\n')
	{
		POLYTEXT* hitRI;
		int midChar;
		hitRI = &norTextRI[visHitLine];
		textPos--;
		caretX -= hitRI->
			pdx[textPos-lineStarts[caretLine]-firstVisChars[visHitLine]];
		midChar = xPos - caretX;
		if (midChar > hitRI->pdx[textPos-lineStarts[caretLine]-
								 firstVisChars[visHitLine]] / 2)
		{
			caretX += hitRI->pdx[textPos-lineStarts[caretLine]-
								 firstVisChars[visHitLine]];
			textPos++;
		}
	}
	}

	caretY = (caretLine + caretLnOffst - visLine) * fontHeight - visLnOffset;
	UpdateCaretPos();
}

/* Scrolls the screen so that the caret is visible.

   Use this function whenever the caret is explicitly moved. */
/* Incomplete */
static void ScrollToCaret()
{
	/* Right now, we only scroll to the caret line. */
	if (caretLine >= visLine + numVisLines)
	{
		ScrollContents(true, 0, (caretLine - (visLine + numVisLines) + 1) *
			fontHeight, SB_VERT);
	}
	if (caretLine == visLine + numVisLines - 1)
	{
		ScrollContents(true, 0, (int)fontHeight -
			(textAreaHeight % fontHeight) - visLnOffset, SB_VERT);
	}
	if (caretLine < visLine)
	{
		ScrollContents(true, 0, -((int)(visLine - caretLine)) *
			fontHeight - visLnOffset, SB_VERT);
	}
	if (caretLine == visLine && visLnOffset != 0)
		ScrollContents(true, 0, -(int)visLnOffset, SB_VERT);
}

/* Configures region flags and redraws the region area as
   necessary.

   This function will attempt to flag the minimum number of lines for
   updating. */
static void UpdateRegion(bool setRegActive)
{
	if (regionActive == false && setRegActive == true)
	{
		lastTextPos = regionBegin;
		regBeginLine = caretLine;
	}
	regionActive = setRegActive;

	if (regionBegin == textPos) /* Dummy check */
		regionActive = false;

	if (regionActive == true)
	{
		UpdateRenderInfo(true, false);
		if (endUpdLine == 0)
		{
			beginUpdLine = regVisBegLn;
			endUpdLine = regVisBegLn + numRegVisLines;
		}
		InvalidateLines();
		lastTextPos = textPos;
		lastCaretLine = caretLine;
		SendMessage(GetParent(lastHwnd), EN_SELCHANGE, 0, (LPARAM)TRUE);
	}
	else
	{
		RECT rt;
		/* Delete the region render info. */
		free(regTextRI);
		regTextRI = NULL;
		/* Update the window. */
		endUpdLine = 0;
		rt.left = 0;
		rt.top = (int)bordWidthY + regVisBegLn * fontHeight - visLnOffset;
		rt.right = winWidth;
		rt.bottom = (int)bordWidthY + (regVisBegLn + numRegVisLines) *
			fontHeight - visLnOffset;
		InvalidateRect(lastHwnd, &rt, TRUE);
		CopyMemory(&updRect, &rt, sizeof(RECT));
		numRegVisLines = 0;
		SendMessage(GetParent(lastHwnd), EN_SELCHANGE, 0, (LPARAM)FALSE);
	}
}

/* Attempts to show the cursor if hidden by typing. */
static void TryCursorUnhide()
{
	if (cursorVis == false)
	{
		cursorVis = true;
		ShowCursor(TRUE);
	}
}

/* Sorts two unsigned integers in ascending order. */
static void SortAscending(unsigned* p1, unsigned* p2)
{
	if (*p2 < *p1)
	{
		unsigned temp;
		temp = *p2;
		*p2 = *p1;
		*p1 = temp;
	}
}

/* Frees the dynamically allocated undo history. */
static void FreeUndoHist()
{
	/* Free all the undo data. */
	curUndo = 0;
	while (curUndo < numUndo)
	{
		free(undoHistory[curUndo].data);
		free(undoHistory[curUndo].oldData);
		curUndo++;
	}

	/* Do the usual. */
	free(undoHistory);
	undoHistory = NULL;
	numUndo = 0;
	curUndo = 0;
}

/* Frees the dynamically allocated render info. */
static void FreeRenderInfo(bool freeNorRI)
{
	if (freeNorRI == true)
	{
		riInitialized = false;
		endUpdLine = 0;
		if (norTextRI != NULL)
		{
			unsigned i;
			for (i = 0; i < totVisLines; i++)
				free(norTextRI[i].pdx);
		}
		free(norTextRI);
		norTextRI = NULL;
	}
	if (regionActive == false)
		return;
	free(regTextRI);
	regTextRI = NULL;
	DeleteObject(regionMask);
	regionMask = NULL;
	DeleteObject(ctRegMask);
	ctRegMask = NULL;
}

/* Frees the dynamically allocated line info for truncated line
   display. */
static void FreeLineInfo()
{
	free(numVisChars);
	numVisChars = NULL;
	free(firstVisChars);
	firstVisChars = NULL;
	free(firstVisOffset);
	firstVisOffset = NULL;
}
