/* Panel interface */

/* Brief description
   *****************

   A panel is a rectangular area that may have a horizontal or
   vertical division through it dividing it into two subpanels.  A
   subpanel may either contain more panels or be a terminal panel.

   Detailed background information
   *******************************

   A "frame window" is a window (usually an overlapped-window) that is
   designated to have a "paneled" layout; that is, the window will
   contain a logical hierarchy within its client area that defines the
   containment rectangles of various child windows.

   A "Panel" (this code library's main object) is an object that
   generally keeps track of a containment hierarchy.  There are two
   main types of panels: terminal panels and non-terminal panels.
   "Terminal panels" are simply structures that keep track of an HWND
   and the intended dimensions of that window.  The dimensions are
   stored in a RECT structure, "dims".  The "dims" member defines the
   top left and lower right coordinates of the HWND as client-area
   coordinates of the frame window.  "Non-terminal panels" are panels
   that not only have bounding dimensions, but also keep track of two
   "subpanels", each of which is on one side of a divider that runs
   either horizontally or vertically through the non-terminal panel.
   The divider's position is also given as a client-area coordinate of
   the frame window.  The divider can either be invisible, or it can
   have a visible width.  One of the subpanels can be flagged to be
   locked at its current size.  If neither of the subpanels are locked
   and the divider is visible, then it is possible for the user to
   grip and drag the divider to change the size of the subpanels.

   In order for a frame window to have the "paneled" containment
   structure, it must keep track of a "root panel" or "frame panel",
   which is the Panel data object at the top of the hierarchy.  This
   panel's dimensions should be equal to the window's client
   rectangle.  It also must handle painting the dividers that have a
   visible width and process messages that could result in beginning a
   divider dragging operation.

   How to use
   **********

   * An initial frame panel is created by calling
     CreateTerminalPanel() with the dimensions equal to the client
     area of the frame window.  If the entire client area of the frame
     window containing the panel structure will be covered with the
     panel windows (the typical case), the window class for that
     window should have a NULL background color.  In your designated
     frame window that contains the panel layout and all child
     windows, you must create a valid panel structure as soon as the
     WM_CREATE message is sent; otherwise, your code will crash later
     on.

   * To assign a child window to a terminal panel containment, use the
     ChangePanelHWND() function.  Do not call this function on a
     non-terminal panel.

   * Generally, the paneling interface only manages its internal data
     structures on panel sizes and does not change the actual size of
     the Windows window execpt when SizePanelWindows() and
     EndDivDrag() are called.

   * To split a panel into two subpanels, call InsertPanel() with
     "pBefore" set to the terminal panel to be replaced and moved down
     the hierarchy.

   * To create another split in one of the two sections, first call
     InsertPanel() with "pBefore" set to the terminal panel to be
     replaced and moved down the hierarchy, then call
     AddTerminalPanel() to fill in the empty slot in the new
     non-terminal panel.  If you are calling this on the frame panel,
     then you will need to make sure you update the frame panel to the
     newly created panel returned by InsertPanel().

   * Call DeletePanel() to merge and delete a section in a
     non-terminal panel.  This is effectively the reverse of
     InsertPanel(), except it will delete entire subtrees in the
     subpanel that is not kept.

   * Call ChangeDivPos() to change the position of a divider.
     ChangeDivPos() will call UpdatePanelSizes() and ScalePanels() as
     necessary.

   * Call ChangePanelDims() whenever a resize occurred on the parent
     object of a panel that requires subpanel size recalculations.

   * Call FreePanels() to delete panel hierarchies including and
     downward from the given panel.

   * Call SizePanelWindows() to update the size of the windows that
     are associated with the given panel frame.

   * Three helper functions are provided for the parent window to
     manage the panel layout, PaintDividers(), FrameWinSetCursor(),
     and DispatchDividerDrag().

   * There are some things that you should not do with this paneling
     interface.  In particular, you should not create panel
     hierarchies that either have NULL pointers to subpanels or empty,
     unused, but allocated subpanels (i.e. zeroing the size of a panel
     to hide it).  Rather than checking for potential NULL pointers
     throughout this code, this code has been written to explicitly
     crash when used incorrectly.  Of course, another option is to use
     debug assertions, but either way, you should remove the bugs from
     your code.  */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#include "Panel.h"
#include "resource.h"

/********************************************************************\
 * Panel data interface												*
\********************************************************************/

/* Our functions primarily treat the panel hierarchy as a
   linked binary tree.  */

/* Creates and returns a terminal panel by itself.  The dimensions are
   initialized to "dims", or zero if "dims" is NULL.  The panel's HWND
   is initialized to NULL.  */
Panel* CreateTerminalPanel(RECT* dims)
{
	Panel* newPanel = (Panel*)malloc(sizeof(Panel));
	if (newPanel == NULL)
		return NULL;
	ZeroMemory(newPanel, sizeof(Panel));
	if (dims != NULL)
		CopyRect(&newPanel->dims, dims);
	newPanel->terminal = TRUE;
	return newPanel;
}

/* Change the dimensions of the given panel, scaling child dimensions
   and divider positions if necessary.  */
void ChangePanelDims(Panel* panel, RECT* dims)
{
	CopyRect(&panel->dims, dims);
	ScalePanels(panel);
}

/* Change the HWND of a terminal panel.  Do not call this on a
   non-terminal panel.  */
void ChangePanelHWND(Panel* panel, HWND hwnd)
{
	panel->hwnd = hwnd;
}

/* Inserts a non-terminal panel before the given panel.

   "horzDiv" is TRUE if the dividing line should run horizontally, in
   which case the subpanes are located above and below the dividing
   line.  If "horzDiv" is FALSE, the dividing line runs vertically.

   See PanelSubProps for information on "subProps".

   "oldMoveTo" specifies which subpanel (0 or 1) of the new panel the
   previous panel should go in.  The other subpanel is filled with a
   newly created terminal panel (which starts with a NULL HWND).

   "divPos" is the absolute (not offset) position of the divider in
   the new non-terminal panel.

   Linking the parent to the new panel is automatic except when
   inserting before the root panel.

   The newly created panel is returned.  */
Panel* InsertPanel(Panel* pBefore, BOOL horzDiv, int subProps,
	unsigned oldMoveTo, long divPos)
{
	Panel* newPanel = (Panel*)malloc(sizeof(Panel));
	Panel* newSubPan;

	if (newPanel == NULL)
		return NULL;
	ZeroMemory(newPanel, sizeof(Panel));
	CopyRect(&newPanel->dims, dims);
	newPanel->terminal = FALSE;
	newPanel->horzDiv = horzDiv;
	newPanel->parent = parent;
	newPanel->subProps = subProps;

	newSubPan = CreateTerminalPanel(NULL);
	if (newSubPan == NULL)
	{
		free(newPanel);
		return NULL;
	}
	newSubPan->parent = newPanel;

	if (oldMoveTo == 0)
	{
		newPanel->sub0 = pBefore;
		newPanel->sub1 = newSubPan;
	}
	else
	{
		newPanel->sub0 = newSubPan;
		newPanel->sub1 = pBefore;
	}
	PanelSizeAdjust(newSubPan);

	/* Find whether the new non-terminal panel is the first or second
	   subpanel of the parent.  */
	if (pBefore->parent != NULL)
	{
		if (pBefore->parent->sub0 == pBefore)
			pBefore->parent->sub0 = newPanel;
		else
			pBefore->parent->sub1 = newPanel;
	}
	pBefore->parent = newPanel;

	ChangeDivPos(newPanel, divPos);
	return newPanel;
}

/* Change the divider position of the given panel to "divPos" and
   scale subpanels dimensions as necessary.  */
void ChangeDivPos(Panel* panel, long divPos)
{
	panel->divPos = divPos;
	UpdatePanelSizes(panel);
}

/* Search inside the given panel for the terminal panel that contains
   the given HWND.  */
Panel* PanelFromHWND(Panel* top, HWND hwnd)
{
	if (top->terminal)
	{
		if (top->panWin == hwnd)
			return top;
		return NULL;
	}
	else
	{
		Panel* result = PanelFromHWND(top->sub0, hwnd);
		if (!result)
			result = PanelFromHWND(top->sub1, hwnd);
		return result;
	}

	return NULL;
}

/* Deletes the given subpanel and modifies pointers as necessary.
   "subToKeep" is either zero or one.  The subpanel that is not kept
   is also deleted recursively.  Do not use a terminal panel as the
   "panel" argument.  The subpanel that was kept is returned.
   User-added terminal panel HWND's will not be destroyed by this
   code.  */
Panel* DeletePanel(Panel* panel, unsigned subToKeep)
{
	/* First modify the pointers, then scale the dimensions.  */
	Panel* thisPan; Panel* otherPan;
	if (subToKeep == 0)
	{ thisPan = panel->sub0; otherPan = panel->sub1; }
	else
	{ thisPan = panel->sub1; otherPan = panel->sub0; }
	FreePanels(otherPan);

	/* Find whether this panel is the first or second subpanel of the
	   parent panel.  */
	if (panel->parent != NULL)
	{
		if (panel->parent->sub0 == panel)
			panel->parent->sub0 = thisPan;
		else
			panel->parent->sub1 = thisPan;
	}
	thisPan->parent = panel->parent;

	/* Expand thisPan to the size of the deleted panel.  */
	CopyRect(&thisPan->dims, &panel->dims);
	free(panel);
	/* Recurse into the subpanels.  */
	ScalePanels(thisPan);

	return thisPan;
}

/* Frees all dynamic memory resources consumed by the given frame.
   User-added terminal panel HWND's will not be destroyed by this
   code.  */
void FreePanels(Panel* frame)
{
	if (frame->terminal)
	{
		free(frame);
		return;
	}

	FreePanels(frame->sub0);
	FreePanels(frame->sub1);
	free(frame);
}

/********************************************************************\
 * Internal functions												*
\********************************************************************/

/* Adjust the width and/or height of a subpanel to be consistent with
   that of the parent panel.  This function is mainly only of use
   internal to the panel code in places when a single subpanel's
   dimensions must be initialized to the correct portion of the parent
   panel.  */
void PanelSizeAdjust(Panel* subPanel)
{
	Panel* parent = subPanel->parent;
	if (parent->horzDiv == TRUE)
	{
		subPanel->dims.left = parent->dims.left;
		subPanel->dims.right = parent->dims.right;
		/* Find whether this panel is the first or second subpanel.  */
		if (parent->sub0 == subPanel)
			subPanel->dims.top = parent->dims.top;
		else
			subPanel->dims.bottom = parent->dims.bottom;
	}
	else
	{
		subPanel->dims.top = parent->dims.top;
		subPanel->dims.bottom = parent->dims.bottom;
		/* Find whether this panel is the first or second subpanel.  */
		if (parent->sub0 == subPanel)
			subPanel->dims.left = parent->dims.left;
		else
			subPanel->dims.right = parent->dims.right;
	}
}

/* Working downward from the given panel, resizes the panels to be
   consistent with the new divider position of the given panel.  This
   function is mainly only of use internal to the panel code.  */
void UpdatePanelSizes(Panel* panel)
{
	RECT* sub0Dims = &panel->sub0->dims;
	RECT* sub1Dims = &panel->sub1->dims;
	long cxFrame, cyFrame;

	/* Update divFrac.  */
	if (panel->horzDiv == TRUE)
	{
		panel->divFrac = (float)(panel->divPos - panel->dims.top) /
			CalcRtHeight(&panel->dims);
		cyFrame = GetSystemMetrics(SM_CYFRAME);
	}
	else
	{
		panel->divFrac = (float)(panel->divPos - panel->dims.left) /
			CalcRtWidth(&panel->dims);
		cxFrame = GetSystemMetrics(SM_CXFRAME);
	}

	if (!(panel->subProps & SUBPAN_NODIV))
	{
		/* Update divRect.  */
		if (panel->horzDiv == TRUE)
		{
			panel->divRect.left = panel->dims.left;
			panel->divRect.right = panel->dims.right;
			panel->divRect.top = panel->divPos - cyFrame / 2;
			panel->divRect.bottom = panel->divRect.top + cyFrame;
		}
		else
		{
			panel->divRect.left = panel->divPos - cxFrame / 2;
			panel->divRect.right = panel->divRect.left + cxFrame;
			panel->divRect.top = panel->dims.top;
			panel->divRect.bottom = panel->dims.bottom;
		}
	}

	/* Update the subpanels.  */
	if (panel->horzDiv == TRUE)
	{
		/* sub0Dims->top = panel->dims.top; */
		sub0Dims->bottom = panel->divPos;
		sub1Dims->top = panel->divPos;
		/* sub1Dims->bottom = panel->dims.bottom; */
		if (!(panel->subProps & SUBPAN_NODIV))
		{
			sub0Dims->bottom -= cyFrame / 2;
			/* Avoid rounding errors.  */
			sub1Dims->top += cyFrame - cyFrame / 2;
		}
	}
	else
	{
		/* sub0Dims->left = panel->dims.left; */
		sub0Dims->right = panel->divPos;
		sub1Dims->left = panel->divPos;
		/* sub1Dims->right = panel->dims.right; */
		if (!(panel->subProps & SUBPAN_NODIV))
		{
			sub0Dims->right -= cxFrame / 2;
			/* Avoid rounding errors.  */
			sub1Dims->left += cxFrame - cxFrame / 2;
		}
	}
	ScalePanels(panel->sub0);
	ScalePanels(panel->sub1);
}

/* Working downward from the given panel, resizes the panels to
   maintain the previous proportions in the new dimensions.  This
   function is mainly only of use internal to the panel code.  */
void ScalePanels(Panel* panel)
{
	if (panel->terminal == TRUE)
		return;

#define SCALE_GENERAL(low, high, CalcGenWidth, GEN_SM_CXFRAME) \
	{ \
		/* Caclulate the new lengths.  */ \
		BOOL noDiv = panel->subProps & SUBPAN_NODIV; \
		RECT* sub0Dims; \
		RECT* sub1Dims; \
		long subLen0, subLen1; \
		long newSubLen0, newSubLen1; \
		long cxFrame = 0; \
		sub0Dims = &panel->sub0->dims; \
		sub1Dims = &panel->sub1->dims; \
		subLen0 = CalcGenWidth(sub0Dims); \
		subLen1 = CalcGenWidth(sub1Dims); \
 \
		if (!(panel->subProps & SUBPAN_NODIV)) \
			cxFrame = GetSystemMetrics(GEN_SM_CXFRAME); \
		if (panel->subProps & SUBPAN_LOCK0) \
		{ \
			newSubLen0 = subLen0; \
			newSubLen1 = CalcGenWidth(&panel->dims) - subLen0 - cxFrame; \
		} \
		else if (panel->subProps & SUBPAN_LOCK1) \
		{ \
			newSubLen0 = CalcGenWidth(&panel->dims) - subLen1 - cxFrame; \
			newSubLen1 = subLen1; \
		} \
		else \
		{ \
			long prevLen = subLen0 + subLen1; \
			/* newSubLen0 = \
			   MulDiv(subLen0, CalcGenWidth(&panel->dims), prevLen); */ \
			newSubLen0 = (long)(panel->divFrac * \
								CalcGenWidth(&panel->dims)); \
			newSubLen1 = CalcGenWidth(&panel->dims) - newSubLen0; \
			newSubLen0 -= cxFrame / 2; \
			/* newSubLen1 = \
			   MulDiv(subLen1, CalcGenWidth(&panel->dims), prevLen); */ \
			/* Avoid rounding errors.  */ \
			newSubLen1 -= cxFrame - cxFrame / 2; \
		} \
 \
		/* Modify the dimensions.  */ \
		sub0Dims->low = panel->dims.low; \
		sub0Dims->high = sub0Dims->low + newSubLen0; \
		sub1Dims->low = sub0Dims->high + cxFrame; \
		sub1Dims->high = sub1Dims->low + newSubLen1; \
		panel->divPos = sub0Dims->high + cxFrame / 2; \
		panel->divRect.low = sub0Dims->high; \
		panel->divRect.high = panel->divRect.low + cxFrame; \
 \
		PanelSizeAdjust(panel->sub0); \
		PanelSizeAdjust(panel->sub1); \
		/* Recurse into the subpanels.  */ \
		ScalePanels(panel->sub0); \
		ScalePanels(panel->sub1); \
	}

	if (panel->horzDiv == TRUE)
	{
		SCALE_GENERAL(top, bottom, CalcRtHeight, SM_CYFRAME);
		panel->divRect.left = panel->dims.left;
		panel->divRect.right = panel->dims.right;
	}
	else
	{
		SCALE_GENERAL(left, right, CalcRtWidth, SM_CXFRAME);
		panel->divRect.top = panel->dims.top;
		panel->divRect.bottom = panel->dims.bottom;
	}

#undef SCALE_GENERAL
}

int CalcRtWidth(RECT* rt)
{
	return rt->right - rt->left;
}

int CalcRtHeight(RECT* rt)
{
	return rt->bottom - rt->top;
}

/********************************************************************\
 * Divider dragging													*
\********************************************************************/

/* Dragging window dividers should always be a synchronous, single
   tasking process.  */

/* static long draggingDiv; */
static Panel* dragPanel;
static BOOL keySplit = FALSE;

/* Variables that reduce function calling */
static unsigned vScrollWidth;
static HBITMAP hbmOld;
static long oldDivPos;

/* Determine if the given mouse point is within a divider.  */
Panel* HitTestDivider(Panel* panel, POINT mousePos)
{
	Panel* hitPanel;
	if (panel->terminal)
		return NULL;

	if (!(panel->subProps & SUBPAN_NODIV))
	{
		if (PtInRect(&panel->divRect, mousePos))
			return panel;
	}

	/* If not hits have been found, recurse into the subpanels.  */
	hitPanel = HitTestDivider(panel->sub0, mousePos);
	if (hitPanel != NULL)
		return hitPanel;
	hitPanel = HitTestDivider(panel->sub1, mousePos);

	return hitPanel;
}

void BeginDivDrag(HWND frameWin, Panel* panel)
{
	RECT rt;
	HBITMAP hbm;
	HBRUSH hBr;
	HDC hDC;
	HDC hDCMem;
	const char data[4] = {0x40, 0x00, 0x80, 0x00}; /* 0100 ... 1000 */

	dragPanel = panel;
	SetCapture(frameWin);
	oldDivPos = dragPanel->divPos;
	/* Set the hatched bar size.  */
	if (dragPanel->horzDiv == TRUE)
	{
		long cyFrame = GetSystemMetrics(SM_CYFRAME);
		rt.left = dragPanel->dims.left;
		rt.top = dragPanel->divPos - cyFrame / 2;
		rt.right = dragPanel->dims.right;
		rt.bottom = rt.top + cyFrame;
	}
	else
	{
		long cxFrame = GetSystemMetrics(SM_CXFRAME);
		rt.left = dragPanel->divPos - cxFrame / 2;
		rt.top = dragPanel->dims.top;
		rt.right = rt.left + cxFrame;
		rt.bottom = dragPanel->dims.bottom;
	}
	/* Create the pattern brush.  */
	hbm = CreateBitmap(2, 2, 1, 1, &data);
	hBr = CreatePatternBrush(hbm);
	/* Prepare the previous bits.  */
	hDC = GetDC(frameWin);
	hbmOld = CreateCompatibleBitmap(hDC, rt.right - rt.left,
		rt.bottom - rt.top);
	hDCMem = CreateCompatibleDC(hDC);
	SelectObject(hDCMem, hbmOld);
	BitBlt(hDCMem, 0, 0, rt.right - rt.left, rt.bottom - rt.top,
		hDC, rt.left, rt.top, SRCCOPY);
	DeleteDC(hDCMem);
	/* Draw the hatched bar.  */
	SelectObject(hDC, hBr);
	PatBlt(hDC, rt.left, rt.top, rt.right - rt.left,
		rt.bottom - rt.top, PATINVERT);
	DeleteObject(hBr);
	DeleteObject(hbm);
	ReleaseDC(frameWin, hDC);
}

/* This function redraws the divider drag graphic by first performing all
   screen operations on a back-buffer bitmap, then doing a single BitBlt()
   to the actual screen.  You should have already set dragPanel->dragPos
   to its new value.  "frameWin" is the parent window that contains the
   entire frame set.  */
void DrawDivDrag(HWND frameWin)
{
	long resBmXPos;
	long resBmYPos;
	long resBmWidth;
	long resBmHeight;
	long resDivWidth;
	long resDivHeight;
	long oldResDivPosX;
	long oldResDivPosY;
	long resDivPosX;
	long resDivPosY;

	/* Calculate horz./vert. independent measures.  */
	if (dragPanel->horzDiv == TRUE)
	{
		long cyFrame = GetSystemMetrics(SM_CYFRAME);
		resDivWidth = dragPanel->dims.right - dragPanel->dims.left;
		resDivHeight = cyFrame;

		/* Depending on whether the new position is before or after the old
		   position, the width and positioning calculations for the back
		   buffer bitmap (hRes) will be different.  */
		if (oldDivPos < dragPanel->divPos)
		{
			resBmHeight = dragPanel->divPos + cyFrame - oldDivPos;
			resBmYPos = oldDivPos - cyFrame / 2;
			oldResDivPosY = 0;
			resDivPosY = resBmHeight - cyFrame; /* Relative to back
												   buffer bitmap */
		}
		else
		{
			resBmHeight = oldDivPos + cyFrame - dragPanel->divPos;
			resBmYPos = dragPanel->divPos - cyFrame / 2;
			oldResDivPosY = resBmHeight - cyFrame;
			resDivPosY = 0; /* Relative to back buffer bitmap */
		}
		resBmXPos = dragPanel->dims.left;
		resBmWidth = CalcRtWidth(&dragPanel->dims);
		resDivWidth = resBmWidth;
		resDivHeight = cyFrame;
		oldResDivPosX = 0;
		resDivPosX = 0;
	}
	else
	{
		long cxFrame = GetSystemMetrics(SM_CXFRAME);
		resDivWidth = cxFrame;
		resDivHeight = dragPanel->dims.bottom - dragPanel->dims.top;

		/* Depending on whether the new position is before or after the old
		   position, the width and positioning calculations for the back
		   buffer bitmap (hRes) will be different.  */
		if (oldDivPos < dragPanel->divPos)
		{
			resBmWidth = dragPanel->divPos + cxFrame - oldDivPos;
			resBmXPos = oldDivPos - cxFrame / 2;
			oldResDivPosX = 0;
			resDivPosX = resBmWidth - cxFrame; /* Relative to back
												  buffer bitmap */
		}
		else
		{
			resBmWidth = oldDivPos + cxFrame - dragPanel->divPos;
			resBmXPos = dragPanel->divPos - cxFrame / 2;
			oldResDivPosX = resBmWidth - cxFrame;
			resDivPosX = 0; /* Relative to back buffer bitmap */
		}
		resBmYPos = dragPanel->dims.top;
		resBmHeight = CalcRtHeight(&dragPanel->dims);
		resDivHeight = resBmHeight;
		resDivWidth = cxFrame;
		oldResDivPosY = 0;
		resDivPosY = 0;
	}

	/* Time to rasterize!  */
	{
		HDC hDC;
		HBITMAP hbm;
		HBRUSH hBr;

		HBITMAP hRes; /* Resulting (back buffer) bitmap */
		HDC hDCMem;
		HDC hDCRes;

		const char data[4] = {(char)0x40, (char)0x00,
							  (char)0x80, (char)0x00}; /* 0100 ... 1000 */

		hDC = GetDC(frameWin);
		hbm = CreateBitmap(2, 2, 1, 1, &data);
		hBr = CreatePatternBrush(hbm);

		/* Create memory DC's and bitmaps.  */
		hRes = CreateCompatibleBitmap(hDC, resBmWidth, resBmHeight);
		hDCMem = CreateCompatibleDC(hDC);
		SelectObject(hDCMem, hbmOld);
		hDCRes = CreateCompatibleDC(hDC);
		SelectObject(hDCRes, hRes);
		SelectObject(hDCRes, hBr);
		/* Get a subcopy of the screen.  */
		BitBlt(hDCRes, 0, 0, resBmWidth, resBmHeight,
			   hDC, resBmXPos, resBmYPos, SRCCOPY);
		/* Erase the bits at the old position.  */
		BitBlt(hDCRes, oldResDivPosX, oldResDivPosY,
			resDivWidth, resDivHeight, hDCMem, 0, 0, SRCCOPY);
		/* Save the bits about to get modified.  */
		BitBlt(hDCMem, 0, 0, resDivWidth, resDivHeight,
			hDCRes, resDivPosX, resDivPosY, SRCCOPY);
		DeleteDC(hDCMem);
		/* Draw the divider drag marker at the new position.  */
		PatBlt(hDCRes, resDivPosX, resDivPosY,
			resDivWidth, resDivHeight, PATINVERT);
		BitBlt(hDC, resBmXPos, resBmYPos, resBmWidth, resBmHeight,
			hDCRes, 0, 0, SRCCOPY);
		DeleteDC(hDCRes);
		DeleteObject(hRes);

		DeleteObject(hBr);
		DeleteObject(hbm);
		ReleaseDC(frameWin, hDC);
	}
	oldDivPos = dragPanel->divPos;
}

/* You should have already set dragPanel->dragPos to its new value.
   "frameWin" is the parent window that contains the entire frame
   set.  */
void EndDivDrag(HWND frameWin, BOOL cancel)
{
	HDC hDC;
	HDC hDCMem;
	long xOfs;
	long yOfs;
	long clearWidth;
	long clearHeight;

	/* Calculate horz./vert. independent measures.  */
	if (dragPanel->horzDiv == TRUE)
	{
		long cyFrame = GetSystemMetrics(SM_CYFRAME);
		xOfs = dragPanel->dims.left;
		yOfs = dragPanel->divPos - cyFrame / 2;
		clearWidth = CalcRtWidth(&dragPanel->dims);
		clearHeight = cyFrame;
	}
	else
	{
		long cxFrame = GetSystemMetrics(SM_CXFRAME);
		xOfs = dragPanel->divPos - cxFrame / 2;
		yOfs = dragPanel->dims.top;
		clearWidth = cxFrame;
		clearHeight = CalcRtHeight(&dragPanel->dims);
	}

	/* Erase the old hatched bar.  */
	hDC = GetDC(frameWin);
	hDCMem = CreateCompatibleDC(hDC);
	SelectObject(hDCMem, hbmOld);
	BitBlt(hDC, xOfs, yOfs, clearWidth, clearHeight,
		hDCMem, 0, 0, SRCCOPY);
	DeleteDC(hDCMem);
	ReleaseDC(frameWin, hDC);
	DeleteObject(hbmOld);
	/* End the operation.  */
	ReleaseCapture();
	/* dragPanel->divPos = draggingDiv; */
	if (cancel == TRUE || (dragPanel->subProps & SUBPAN_LOCK0) ||
		(dragPanel->subProps & SUBPAN_LOCK1))
	{
		if (dragPanel->horzDiv == TRUE)
		{
			dragPanel->divPos = dragPanel->sub0->dims.bottom;
			if (!(dragPanel->subProps & SUBPAN_NODIV))
			{
				long cyFrame = GetSystemMetrics(SM_CYFRAME);
				dragPanel->divPos += cyFrame / 2;
			}
		}
		else
		{
			dragPanel->divPos = dragPanel->sub0->dims.right;
			if (!(dragPanel->subProps & SUBPAN_NODIV))
			{
				long cxFrame = GetSystemMetrics(SM_CXFRAME);
				dragPanel->divPos += cxFrame / 2;
			}
		}
	}
	UpdatePanelSizes(dragPanel);
	SizePanelWindows(dragPanel);
}

/********************************************************************\
 * Windowing and messaging											*
\********************************************************************/

/* Variables that reduce function calling */
static POINT oldCursorPos;

/* Change the actual dimensions of the Windows HWND windows associated
   within the given panel hierarchy.  */
void SizePanelWindows(Panel* panel)
{
	if (panel->terminal == TRUE)
	{
		MoveWindow(panel->panWin, panel->dims.left, panel->dims.top,
			CalcRtWidth(&panel->dims), CalcRtHeight(&panel->dims), TRUE);
		return;
	}

	/* Recurse into the subpanels.  */
	SizePanelWindows(panel->sub0);
	SizePanelWindows(panel->sub1);
}

/* For a frame window that contains a paneled layout, this function
   will handle drawing opaque rectangles at the locations of the
   movable panel dividers.  */
void PaintDividers(Panel* panel, HDC hDC)
{
	if (panel->terminal)
		return;

	if (!(panel->subProps & SUBPAN_NODIV))
		FillRect(hDC, &panel->divRect, GetSysColorBrush(COLOR_3DFACE));

	/* Recurse into the subpanels.  */
	PaintDividers(panel->sub0, hDC);
	PaintDividers(panel->sub1, hDC);
}

/* For a frame window that contains a paneled layout, this function
   will handle setting the cursor in WM_SETCURSOR so that the correct
   resizing cursor is displayed based off of the type of divider that
   the mouse hovers.  */
BOOL FrameWinSetCursor(Panel* panel, POINT mousePos)
{
	static HCURSOR cArrow = NULL;
	static HCURSOR cSizeNS = NULL;
	static HCURSOR cSizeWE = NULL;
	Panel* divPanel;
	BOOL retval = FALSE;

	if (cSizeNS == NULL)
	{
		cArrow = LoadCursor(NULL, IDC_ARROW);
		cSizeNS = LoadCursor(NULL, IDC_SIZENS);
		cSizeWE = LoadCursor(NULL, IDC_SIZEWE);
	}

	divPanel = HitTestDivider(panel, mousePos);
	if (divPanel == NULL)
		return FALSE;

	if ((divPanel->subProps & SUBPAN_LOCK0) ||
		(divPanel->subProps & SUBPAN_LOCK1))
		SetCursor(cArrow);
	else if (divPanel->horzDiv == TRUE)
		SetCursor(cSizeNS);
	else
		SetCursor(cSizeWE);

	return TRUE;
}

/* Dispatch a potential mouse divider drag event in WM_LBUTTONDOWN.  */
BOOL DispatchDividerDrag(Panel* mainFrame, HWND frameWin, POINTS mousePos)
{
	POINT lMousePos = { mousePos.x, mousePos.y };
	Panel* clickedPanel = HitTestDivider(mainFrame, lMousePos);
	if (clickedPanel == NULL ||
		(clickedPanel->subProps & SUBPAN_LOCK0) ||
		(clickedPanel->subProps & SUBPAN_LOCK1))
		return FALSE;
	BeginDivDrag(frameWin, clickedPanel);
	return TRUE;
}

/* Update a divider drag mark of a divider being mouse dragged.  */
void ProcDivDrag(HWND frameWin, int xpos, int ypos)
{
	long cnFrame;
	RECT rt;
	if (dragPanel->horzDiv == TRUE)
		cnFrame = GetSystemMetrics(SM_CYFRAME);
	else
		cnFrame = GetSystemMetrics(SM_CXFRAME);
	vScrollWidth = GetSystemMetrics(SM_CXVSCROLL); /* BAD TODO FIXME */
	CopyRect(&rt, &dragPanel->dims);
	rt.left += cnFrame + vScrollWidth * 2;
	rt.top += cnFrame + vScrollWidth * 2;
	rt.right -= cnFrame + vScrollWidth * 2;
	rt.bottom -= cnFrame + vScrollWidth * 2;
	if (CalcRtWidth(&dragPanel->dims) <= vScrollWidth * 2)
		xpos = (rt.left + rt.right) / 2;
	else if (xpos < rt.left)
		xpos = rt.left;
	else if (xpos >= rt.right)
		xpos = rt.right - 1;
	if (CalcRtHeight(&dragPanel->dims) <= vScrollWidth * 2)
		ypos = (rt.top + rt.bottom) / 2;
	else if (ypos < rt.top)
		ypos = rt.top;
	else if (ypos >= rt.bottom)
		ypos = rt.bottom - 1;

	if (dragPanel->horzDiv == TRUE)
		dragPanel->divPos = ypos;
	else
		dragPanel->divPos = xpos;
	DrawDivDrag(frameWin);
}

/* Initiate a keyboard-based divider adjusting session.  */
void KeyDividerDrag(HWND frameWin, HINSTANCE hInstance)
{
	keySplit = TRUE;
	/* draggingDiv = true; */
	GetCursorPos(&oldCursorPos);
	{
		POINT pt;
		WNDCLASSEX wcex;
		/* Set the cursor properly.  */
		GetCursorPos(&pt);
		ScreenToClient(frameWin, &pt);
		/* Start by highlighting the first draggable divider.  */
		pt.x = 2;
		pt.y = 2;
		ClientToScreen(frameWin, &pt);
		SetCursorPos(pt.x, pt.y);
		GetClassInfoEx(hInstance, "VdividerWindow", &wcex);
		SetCursor(wcex.hCursor);
		/* BeginDivDrag(frameWin); */
	}
}

/* Update a divider drag mark of a divider being keyboard edited.  */
void ProcKeyDivDrag()
{
	/* Prompt the user for the divider to change.  */
	/* Edit this divider.  */
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
			if (keySplit == TRUE)
			{
				keySplit = FALSE;
				SetCursorPos(oldCursorPos.x, oldCursorPos.y);
			}
}

void KeyLBtnDown()
{
		if (keySplit == TRUE)
		{
			SetCursorPos(oldCursorPos.x, oldCursorPos.y);
			keySplit = FALSE;
			/* draggingDiv = false; */
			/* EndDivDrag(hwnd, FALSE); */
		}
		/* For WM_CAPTURECHANGED */
				if (keySplit == TRUE)
					SetCursorPos(oldCursorPos.x, oldCursorPos.y);
}

/*

Using the keyboard to change the position of splits:

1. The user has to select which divider to change.  During this
process, the mouse cursor will be positioned over the divider and its
color will be inverted (solid XOR rectangle).  The users uses TAB and
SHIFT+TAB to navigate the hierarchy.

2. One the user selects the divider to drag by either hitting enter or
the space bar, the divider's new position is highlighted as usual, but
the user can use the left, right, up, and down keys to change the new
position.  Positioning ends when the user hits space, enter, or
escape.

*/
