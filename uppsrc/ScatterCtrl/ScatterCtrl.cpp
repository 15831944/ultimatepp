#include "ScatterCtrl.h"

NAMESPACE_UPP

#define IMAGECLASS ScatterImg
#define IMAGEFILE <ScatterCtrl/ScatterCtrl.iml>
#include <Draw/iml.h>

#define TFILE <ScatterCtrl/ScatterCtrl.t>
#include <Core/t.h>

ScatterCtrl::MouseBehaviour defaultMouse[] = {
	{false, false, false, true , false, 0, false, ScatterCtrl::SHOW_INFO}, 
	{false, false, false, false, false, 0, true , ScatterCtrl::CONTEXT_MENU},
	{false, false, false, false, true , 0, false, ScatterCtrl::SCROLL},
	{false, false, false, false, false, 1, false, ScatterCtrl::ZOOM_H_RED},
	{false, false, false, false, false, 1, false, ScatterCtrl::ZOOM_V_RED},
	{false, false, false, false, false,-1, false, ScatterCtrl::ZOOM_H_ENL},
	{false, false, false, false, false,-1, false, ScatterCtrl::ZOOM_V_ENL},
	{false, false, false, false, false, 0, false, ScatterCtrl::NO_ACTION}};
	

#ifdef PLATFORM_WIN32

void ScatterCtrl::SaveAsMetafile(const char* file)
{
	GuiLock __;
	WinMetaFileDraw wmfd;	
	wmfd.Create(copyRatio*ScatterDraw::GetSize().cx, copyRatio*ScatterDraw::GetSize().cy, "ScatterCtrl", "chart", file);
	SetDrawing<Draw>(wmfd, ScatterDraw::GetSize(), copyRatio);	
	wmfd.Close();	
}

void ScatterCtrl::SaveToClipboard(bool saveAsMetafile) 
{
	GuiLock __;
	if (saveAsMetafile) {
		WinMetaFileDraw wmfd;	
		wmfd.Create(copyRatio*ScatterDraw::GetSize().cx, copyRatio*ScatterDraw::GetSize().cy, "ScatterCtrl", "chart");
		SetDrawing<Draw>(wmfd, ScatterDraw::GetSize(), copyRatio);	
		WinMetaFile wmf = wmfd.Close();	 
		wmf.WriteClipboard();
	} else {
		Image img = GetImage(ScatterDraw::GetSize(), copyRatio, false);
		WriteClipboardImage(img);	
	}
}
#else

void ScatterCtrl::SaveToClipboard(bool) 
{
	GuiLock __;
	Image img = GetImage(ScatterDraw::GetSize(), copyRatio, false);
	WriteClipboardImage(img);
}

#endif

void ScatterCtrl::Paint(Draw& w) 
{
	if (IsNull(highlight_0) && highlighting) {
		highlighting = false;
		KillTimeCallback();
	}
	if (!IsNull(highlight_0) && !highlighting) {
		highlighting = true;
		SetTimeCallback(-200, THISBACK(TimerCallback));
	}
	TimeStop t;
	lastRefresh0_ms = GetTickCount();
	if (mode == MD_DRAW) {
		ScatterCtrl::SetDrawing(w, GetSize(), 1);
		PlotTexts(w, GetSize(), 1);
	} else {
		ImageBuffer ib(GetSize());
		BufferPainter bp(ib, mode);
		//bp.Clear(White());
		//PlotTexts(bp, GetSize(), 1);
		ScatterCtrl::SetDrawing(bp, GetSize(), 1);
		w.DrawImage(0, 0, ib);
		PlotTexts(w, GetSize(), 1);
	}
	lastRefresh_ms = t.Elapsed();
}

void ScatterCtrl::TimerCallback() {
	Refresh();
}

void ScatterCtrl::ProcessPopUp(const Point & pt)
{
	double _x  = (popLT.x - hPlotLeft)*xRange/(GetSize().cx - (hPlotLeft + hPlotRight)-1) + xMin;		
	double _y  = -(popLT.y - vPlotTop - titleHeight)*yRange/(GetSize().cy - (vPlotTop + vPlotBottom + titleHeight)+1) + yMin + yRange;		
	double _y2 = -(popLT.y - vPlotTop - titleHeight)*yRange2/(GetSize().cy - (vPlotTop + vPlotBottom + titleHeight)+1) + yMin2 + yRange2;		
	double x   = (pt.x - hPlotLeft)*xRange/(GetSize().cx - (hPlotLeft + hPlotRight)-1) + xMin;		
	double y   = -(pt.y - vPlotTop - titleHeight)*yRange/(GetSize().cy - (vPlotTop + vPlotBottom + titleHeight)+1) + yMin + yRange;		
	double y2  = -(pt.y - vPlotTop - titleHeight)*yRange2/(GetSize().cy - (vPlotTop + vPlotBottom + titleHeight)+1) + yMin2 + yRange2;		
	
	double dx  = x  - _x;
	double dy  = y  - _y;
	double dy2 = y2 - _y2;

	String strx, _strx, dstrx, stry, _stry, dstry;
	if (cbModifFormatX) {
		cbModifFormatX(strx,  0, x); 		
		strx.Replace("\n", " ");
		cbModifFormatX(_strx, 0, _x); 		
		_strx.Replace("\n", " ");
	} else {
		strx  = VariableFormatX(x);
		_strx = VariableFormatX(_x);
	}
	if (cbModifFormatDeltaX) {
		cbModifFormatDeltaX(dstrx, 0, dx);	
		dstrx.Replace("\n", " "); 
	} else 
		dstrx = VariableFormatX(dx);
	
	if (cbModifFormatY) {
		cbModifFormatY(stry,  0, y);		
		stry.Replace("\n", " ");
		cbModifFormatY(_stry, 0, _y);		
		_stry.Replace("\n", " ");
	} else {
		stry  = VariableFormatY(y);
		_stry = VariableFormatY(_y);
	}
	if (cbModifFormatDeltaY) {
		cbModifFormatDeltaY(dstry, 0, dy);	
		dstry.Replace("\n", " ");
	} else 
		dstry = VariableFormatY(dy);
	
	String str = popTextX + ": " + _strx;
	if (strx != _strx)
		str << "; " + popTextX + "': " + strx + "; Δ" + popTextX + ": " + dstrx;
	str << "\n" + popTextY + ": " + _stry;
	if (stry != _stry)	
 		str << "; " + popTextY + "': " + stry + "; Δ" + popTextY + ": " + dstry;
	if (drawY2Reticle) {
		String stry2, _stry2, dstry2;
		if (cbModifFormatY2) {
			cbModifFormatY2(stry2,  0, y2);			
			stry2.Replace("\n", " ");
			cbModifFormatY2(_stry2, 0, _y2);		
			_stry2.Replace("\n", " ");
		} else {
			stry2  = VariableFormatY2(y2);
			_stry2 = VariableFormatY2(_y2);
		}
		if (cbModifFormatDeltaY2) {
			cbModifFormatDeltaY2(dstry2, 0, dy2);	
			dstry2.Replace("\n", " ");
		} else 
			dstry2 = VariableFormatY(dy2);
		
		str << "\n" + popTextY2 + ": " + _stry2;
		if (stry2 != _stry2)		
			str << "; " + popTextY2 + ": " + stry2 + "; Δ" + popTextY2 + ": " + dstry2;
	}
	const Point p2 = pt + offset;
	popText.SetText(str).Move(this, p2.x, p2.y);
}

void ScatterCtrl::DoMouseAction(bool down, Point pt, MouseAction action, int value)
{
	switch (action) {
	case SCROLL: 		Scrolling(down, pt);
						break;
	case ZOOM_H_ENL:	 
	case ZOOM_H_RED:	MouseZoom(value, true, false); 
						break;
	case ZOOM_V_ENL:
	case ZOOM_V_RED:	MouseZoom(value, false, true); 
						break;
	case SHOW_INFO:		LabelPopUp(down, pt);
						break;
	case CONTEXT_MENU:	if(showContextMenu)
							MenuBar::Execute(THISBACK(ContextMenu));
						break;
	case NO_ACTION:;
	}
}

bool ScatterCtrl::SetMouseBehavior(MouseBehaviour *_mouseBehavior) 
{
	if (!_mouseBehavior)
		return false;
	int i;
	for (i = 0; _mouseBehavior[i].action != NO_ACTION && i < MAX_MOUSEBEHAVIOR; ++i) ;
	if (i == MAX_MOUSEBEHAVIOR)
		return false;
	mouseBehavior = _mouseBehavior;
	return true;
}

void ScatterCtrl::ProcessMouse(bool down, Point &pt, bool ctrl, bool alt, bool shift, bool left, bool middle, int middleWheel, bool right) 
{
	for (int i = 0; mouseBehavior[i].action != NO_ACTION && i < MAX_MOUSEBEHAVIOR; ++i) {
		if (mouseBehavior[i].ctrl   == ctrl   && mouseBehavior[i].alt   == alt   &&
		    mouseBehavior[i].shift  == shift  && mouseBehavior[i].left  == left  &&
		    mouseBehavior[i].middle == middle && mouseBehavior[i].right == right &&
		    ((mouseBehavior[i].middleWheel == 0) || mouseBehavior[i].middleWheel == ((middleWheel > 0) - (middleWheel < 0))))
		    DoMouseAction(down, pt, mouseBehavior[i].action, middleWheel);
	}	
}

void ScatterCtrl::LabelPopUp(bool down, Point &pt) 
{
	if (down) {
		if(paintInfo && hPlotLeft <= pt.x && pt.x <= GetSize().cx - hPlotRight && 
		  				(vPlotTop + titleHeight) <= pt.y && pt.y <= GetSize().cy - vPlotBottom) {
			popText.AppearOnly(this);
			
			isLabelPopUp = true;
			if (IsNull(popLT))
				popLT = pt;
			popRB = pt;
			Rect wa = GetWorkArea();
			Rect rc = GetScreenRect();
			if (wa.right - (rc.left + pt.x) < 200)
				pt.x -= 200;
			if (wa.bottom - (rc.top + pt.y) < 200)
				pt.y -= 200;
			ProcessPopUp(pt);		
		}	
	} else {
		if(paintInfo && isLabelPopUp) {
			popText.Close();
			isLabelPopUp = false;
			popLT = popRB = Null;
			Refresh();
		}		
	}
}

#ifdef PLATFORM_LINUX
	#include <X11/cursorfont.h>
#endif

void ScatterCtrl::Scrolling(bool down, Point &pt, bool isOut)
{
	static Image mouseImg;
	if (down) {
		if((mouseHandlingX || mouseHandlingY) && hPlotLeft <= pt.x && pt.x <= GetSize().cx - hPlotRight && 
			(vPlotTop + titleHeight) <= pt.y && pt.y <= GetSize().cy - vPlotBottom) {
			butDownX = pt.x;
			butDownY = pt.y;	
			isScrolling = true;
			INTERLOCKED { 
				mouseImg = Ctrl::OverrideCursor(Image::SizeAll());
			}
		}
	} else {
		if (isScrolling) {
			if (!isOut)
				MouseMove(pt, 0);
			isScrolling = false;
			Ctrl::OverrideCursor(mouseImg);
		}
	}
}

void ScatterCtrl::LeftDown(Point pt, dword keyFlags) 
{
	ProcessMouse(true, pt, keyFlags & K_CTRL, keyFlags & K_ALT, keyFlags & K_SHIFT, true, false, 0, false);
}

void ScatterCtrl::LeftUp(Point pt, dword keyFlags)
{
	ProcessMouse(false, pt, keyFlags & K_CTRL, keyFlags & K_ALT, keyFlags & K_SHIFT, true, false, 0, false); 
}

void ScatterCtrl::MiddleDown(Point pt, dword keyFlags)
{
	ProcessMouse(true, pt, keyFlags & K_CTRL, keyFlags & K_ALT, keyFlags & K_SHIFT, false, true, 0, false);
}

void ScatterCtrl::MiddleUp(Point pt, dword keyFlags)
{
	ProcessMouse(false, pt, keyFlags & K_CTRL, keyFlags & K_ALT, keyFlags & K_SHIFT, false, true, 0, false);
}

void ScatterCtrl::RightDown(Point pt, dword keyFlags) 
{
	ProcessMouse(true, pt, keyFlags & K_CTRL, keyFlags & K_ALT, keyFlags & K_SHIFT, false, false, 0, true);
}

void ScatterCtrl::RightUp(Point pt, dword keyFlags)
{
	ProcessMouse(false, pt, keyFlags & K_CTRL, keyFlags & K_ALT, keyFlags & K_SHIFT, false, false, 0, true); 
}

void ScatterCtrl::MouseWheel(Point pt, int zdelta, dword keyFlags) 
{
	if (zdelta == 0)
		return;
	ProcessMouse(true, pt, keyFlags & K_CTRL, keyFlags & K_ALT, keyFlags & K_SHIFT, false, false, zdelta, false);
}

void ScatterCtrl::MouseMove(Point pt, dword)
{
	if (isScrolling) {
		double factorX = 0, factorY = 0;
		int shiftX = pt.x - butDownX;
		if (mouseHandlingX && shiftX != 0) 
			factorX = double(shiftX)/(GetSize().cx - (hPlotLeft + hPlotRight) - 1);
		int shiftY = pt.y - butDownY;
		if (mouseHandlingY && shiftY != 0) 
			factorY = double(shiftY)/(GetSize().cy - (vPlotTop + vPlotBottom) - 1);
		butDownX = pt.x;
		butDownY = pt.y;
		if ((mouseHandlingX && shiftX != 0) || (mouseHandlingY && shiftY != 0)) 
			ScatterDraw::Scroll(factorX, -factorY);
	} 
	if(isLabelPopUp) {
		if (paintInfo && hPlotLeft <= pt.x && pt.x <= GetSize().cx - hPlotRight && 
						(vPlotTop + titleHeight) <= pt.y && pt.y <= GetSize().cy - vPlotBottom) {
			if (IsNull(popLT))
				popLT = pt;
			popRB = pt;
			popText.AppearOnlyOpen(this);
			
			ProcessPopUp(pt);
			Refresh();
		}
	}	
}

void ScatterCtrl::MouseLeave()
{
	Point p = Null;
	Scrolling(false, p, true);
}

void ScatterCtrl::MouseZoom(int zdelta, bool hor, bool ver) 
{
	double scale = zdelta > 0 ? zdelta/100. : -100./zdelta;
//	if (hor && (lastxRange < xRange*scale))
//		return;
//	if (ver && (lastyRange < yRange*scale))
//		return;
	if (lastRefresh_sign != ((scale >= 0) ? 1 : -1))	
		lastRefresh_ms = Null;					// Change of scale sign resets lastRefresh check
	if (GetTickCount() - lastRefresh0_ms > 1000)
		lastRefresh_ms = Null;					// 1 sec resets lastRefresh check
	if (!IsNull(lastRefresh_ms) && (lastRefresh_ms > maxRefresh_ms))
		return;
	Zoom(scale, mouseHandlingX, mouseHandlingY);
}

Image ScatterCtrl::CursorImage(Point p, dword keyflags)
{
	if (paintInfo)
		return ScatterImg::cursor1();
	return Image::Arrow();
}

ScatterCtrl &ScatterCtrl::SetMouseHandling(bool valx, bool valy) 
{
	mouseHandlingX = valx;
	mouseHandlingY = valy;
	return *this;
}

void ScatterCtrl::ContextMenu(Bar& bar)
{
	if (mouseHandlingX || mouseHandlingY) {
		bar.Add(t_("Fit to data"), 	ScatterImg::ShapeHandles(), THISBACK1(FitToData, mouseHandlingY));
		bar.Add(t_("Zoom +"), 		ScatterImg::ZoomPlus(), 	THISBACK3(Zoom, 1/1.2, true, mouseHandlingY));
		bar.Add(t_("Zoom -"), 		ScatterImg::ZoomMinus(), 	THISBACK3(Zoom, 1.2, true, mouseHandlingY));
		bar.Add(t_("Scroll Left"), 	ScatterImg::LeftArrow(), 	THISBACK2(ScatterDraw::Scroll, 0.2, 0));
		bar.Add(t_("Scroll Right"), ScatterImg::RightArrow(), 	THISBACK2(ScatterDraw::Scroll, -0.2, 0));
		if (mouseHandlingY) {
			bar.Add(t_("Scroll Up"), 	ScatterImg::UpArrow(), 	THISBACK2(ScatterDraw::Scroll, 0, -0.2));
			bar.Add(t_("Scroll Down"), 	ScatterImg::DownArrow(), THISBACK2(ScatterDraw::Scroll, 0, 0.2));			
		}
		bar.Separator();
	}
#ifndef _DEBUG
	if (showPropDlg)
#endif	
	{
		bar.Add(t_("Properties"), ScatterImg::Gear(), THISBACK(DoShowEditDlg));			
		bar.Separator();
	}
	bar.Add(t_("Copy"), ScatterImg::Copy(), 		THISBACK1(SaveToClipboard, false)).Key(K_CTRL_C);
	bar.Add(t_("Save to file"), ScatterImg::Save(), THISBACK1(SaveToFile, Null));
}

void ScatterCtrl::SaveToFile(String fileName)
{
	GuiLock __;
	if (IsNull(fileName)) {
		FileSel fs;
		
		fs.Type("PNG file", "*.png");
		fs.Type("JPEG file", "*.jpg");
	    if(!fs.ExecuteSaveAs(t_("Saving plot to PNG or JPEG file"))) {
	        Exclamation(t_("Plot has not been saved"));
	        return;
	    }
        fileName = fs;
	} 
	if (GetFileExt(fileName) == ".png") {
		PNGEncoder encoder;
		encoder.SaveFile(fileName, GetImage(ScatterDraw::GetSize(), copyRatio, false));
	} else if (GetFileExt(fileName) == ".jpg") {	
		JPGEncoder encoder(90);
		encoder.SaveFile(fileName, GetImage(ScatterDraw::GetSize(), copyRatio, false));		
	} else
		Exclamation(Format(t_("File format \"%s\" not found"), GetFileExt(fileName)));
}

ScatterCtrl &ScatterCtrl::AddSeries(ArrayCtrl &data, bool useCols, int idX, int idY, int idZ, int beginData, int numData)
{
	AddSeries<ArrayCtrlSource>(data, useCols, idX, idY, beginData, numData);
	return *this;
}

void ScatterCtrl::InsertSeries(int id, ArrayCtrl &data, bool useCols, int idX, int idY, int idZ, int beginData, int numData)
{
	InsertSeries<ArrayCtrlSource>(id, data, useCols, idX, idY, beginData, numData);
}

ScatterCtrl::ScatterCtrl() : offset(10,12), copyRatio(1)
{
	paintInfo = mouseHandlingX = mouseHandlingY = isScrolling = isLabelPopUp = false;
	popTextX = "x";
	popTextY = "y1";
	popTextY2 = "y2";
	popLT = popRB = Null;
	showContextMenu = false;
	showPropDlg = false;
	Color(graphColor);	
	BackPaint();
	popText.SetColor(SColorFace);        
	SetMouseBehavior(defaultMouse);
	lastRefresh_ms = Null;
	maxRefresh_ms = 500;
	highlighting = false;
}

END_UPP_NAMESPACE