/* Panel interface */
/* This is platform dependent code: include windows.h before this
   header. */
/* To learn how to use Panel, see the top of "Panel.c".  */

#ifndef PANEL_H
#define PANEL_H

typedef struct Panel_t Panel;

/* This should be SUBPROP_* */
enum PanelSubProps
{
	SUBPAN_NORMAL = 0, /* No specials */
	SUBPAN_NODIV = 1, /* Do not use a divider window */
	SUBPAN_LOCK0 = 2, /* Size lock "sub1" */
	SUBPAN_LOCK1 = 4 /* Size lock "sub2" */
};

struct Panel_t
{
	RECT dims; /* Relative to the frame window's client area */
	BOOL terminal;
	Panel* parent;
	HWND panWin; /* Only applicable for terminal panels */
	/* Variables below are for non-terminal panels only */
	BOOL horzDiv; /* dividing line runs horizontal */
	RECT divRect;
	long divPos; /* Specifies the absolute (not offset) position of
				    the divider. */
	float divFrac; /* Fractional offset relative to current panel
				      dimensions, used to avoid precision loss while
					  resizing. */
	int subProps; /* See PanelSubProps */
	Panel* sub0;
	Panel* sub1;
};

Panel* CreateTerminalPanel(RECT* dims);
void ChangePanelDims(Panel* panel, RECT* dims);
void ChangePanelHWND(Panel* panel, HWND hwnd);
Panel* InsertPanel(Panel* pBefore, BOOL horzDiv, int subProps,
	unsigned oldMoveTo, long divPos);
void ChangeDivPos(Panel* panel, long divPos);
Panel* PanelFromHWND(Panel* top, HWND hwnd);
Panel* DeletePanel(Panel* panel, unsigned subToKeep);
void FreePanels(Panel* frame);

void PanelSizeAdjust(Panel* subPanel);
void UpdatePanelSizes(Panel* panel);
void ScalePanels(Panel* panel);
int CalcRtWidth(RECT* rt);
int CalcRtHeight(RECT* rt);

Panel* HitTestDivider(Panel* panel, POINT mousePos);
void BeginDivDrag(HWND frameWin, Panel* panel);
void DrawDivDrag(HWND frameWin);
void EndDivDrag(HWND frameWin, BOOL cancel);

void SizePanelWindows(Panel* panel);
void PaintDividers(Panel* panel, HDC hDC);
BOOL FrameWinSetCursor(Panel* panel, POINT mousePos);
BOOL DispatchDividerDrag(Panel* mainFrame, HWND frameWin, POINTS mousePos);
void ProcDivDrag(HWND frameWin, int xpos, int ypos);

#endif /* not PANEL_H */
