#include <CtrlCore/CtrlCore.h>

#ifdef GUI_GTK

NAMESPACE_UPP

#define LLOG(x)  // DLOG(x)

Rect Ctrl::frameMargins;

Rect Ctrl::GetFrameMargins()
{
	GuiLock __;
	return frameMargins != Rect(0, 0, 0, 0) ? frameMargins : Rect(8, 32, 8, 8);
}

void    TopWindow::SyncSizeHints()
{
	GuiLock __;
	if(!top)
		return;
	GdkGeometry m;
	Size sz0 = GetRect().GetSize();
	Size sz = sz0;
	if(sizeable)
		sz = GetMinSize();
	m.min_width = sz.cx;
	m.min_height = sz.cy;
	sz = sz0;
	if(sizeable)
		sz = GetMaxSize();
	m.max_width = sz.cx;
	m.max_height = sz.cy;
	gtk_window_set_resizable(gtk(), sizeable);
	gtk_window_set_geometry_hints(gtk(), top->window, &m,
	                              GdkWindowHints(GDK_HINT_MIN_SIZE|GDK_HINT_MAX_SIZE));
}

void TopWindow::SyncTitle()
{
	GuiLock __;
	if(top)
		gtk_window_set_title(gtk(), FromUnicode(title, CHARSET_UTF8));
}

void TopWindow::SyncCaption()
{
	GuiLock __;
	SyncTitle();
	if(top) {
		if(gdk_icon.Set(icon))
			gtk_window_set_icon(gtk(), gdk_icon);
		gtk_window_set_decorated(gtk(), !frameless);
		gtk_window_set_urgency_hint(gtk(), urgent);
	}
}

void TopWindow::CenterRect(Ctrl *owner)
{
	GuiLock __;
	SetupRect();
	if(owner && center == 1 || center == 2) {
		Size sz = GetRect().Size();
		Rect r, wr;
		wr = Ctrl::GetWorkArea();
		Rect fm = GetFrameMargins();
		if(center == 1)
			r = owner->GetRect();
		else
			r = wr;
		Point p = r.CenterPos(sz);
		r = RectC(p.x, p.y, sz.cx, sz.cy);
		wr.left += fm.left;
		wr.right -= fm.right;
		wr.top += fm.top;
		wr.bottom -= fm.bottom;
		if(r.top < wr.top) {
			r.bottom += wr.top - r.top;
			r.top = wr.top;
		}
		if(r.bottom > wr.bottom)
			r.bottom = wr.bottom;
		minsize.cx = min(minsize.cx, r.GetWidth());
		minsize.cy = min(minsize.cy, r.GetHeight());
		SetRect(r);
	}
}

gboolean TopWindow::StateEvent(GtkWidget *widget, GdkEventWindowState *event, gpointer user_data)
{
	TopWindow *w = (TopWindow *)user_data;
	dword h = event->new_window_state;
	if(h & GDK_WINDOW_STATE_FULLSCREEN)
		w->state = FULLSCREEN;
	else
	if(h & GDK_WINDOW_STATE_ICONIFIED)
		w->state = MINIMIZED;
	else
	if(h & GDK_WINDOW_STATE_MAXIMIZED)
		w->state = MAXIMIZED;
	else
		w->state = OVERLAPPED;
	w->topmost = h & GDK_WINDOW_STATE_ABOVE;
	return FALSE;
}

void TopWindow::Open(Ctrl *owner)
{
	GuiLock __;
	LLOG("OPEN " << Name() << " owner: " << UPP::Name(owner));
	if(dokeys && (!GUI_AKD_Conservative() || GetAccessKeysDeep() <= 1))
		DistributeAccessKeys();
	if(fullscreen)
		SetRect(GetPrimaryScreenArea());
	else
		CenterRect(owner);
	IgnoreMouseUp();
	Create(owner, false);
	g_signal_connect(top->window, "window-state-event", G_CALLBACK(StateEvent), this);
	SyncSizeHints();
	SyncCaption();
	PlaceFocus();
	int q = state;
	state = OVERLAPPED;
	SetMode(q);
	SyncTopMost();
	GdkRectangle fr;
	gdk_window_get_frame_extents(gdk(), &fr);
	Rect r = GetRect();
	frameMargins.left = max(frameMargins.left, minmax(r.left - fr.x, 0, 32));
	frameMargins.right = max(frameMargins.right, minmax(fr.x + fr.width - r.right, 0, 32));
	frameMargins.top = max(frameMargins.top, minmax(r.top - fr.y, 0, 64));
	frameMargins.bottom = max(frameMargins.bottom, minmax(fr.y + fr.height - r.bottom, 0, 48));
}

void TopWindow::Open()
{
	Open(GetActiveWindow());
}

void TopWindow::OpenMain()
{
	Open(NULL);
}

void TopWindow::SyncTopMost()
{
	GuiLock __;
	if(top)
		gtk_window_set_keep_above(gtk(), topmost);
}

void TopWindow::SetMode(int mode)
{
	GuiLock __;
	GtkWindow *w = gtk();
	if(w)
		switch(state) {
		case MINIMIZED:
			gtk_window_deiconify(w);
			break;
		case MAXIMIZED:
			gtk_window_unmaximize(w);
			break;
		case FULLSCREEN:
			gtk_window_unfullscreen(w);
			break;
		}
	state = mode;
	if(w)
		switch(state) {
		case MINIMIZED:
			gtk_window_iconify(w);
			break;
		case MAXIMIZED:
			gtk_window_maximize(w);
			break;
		case FULLSCREEN:
			gtk_window_fullscreen(w);
			break;
		}
}

void TopWindow::Minimize(bool effect)
{
	SetMode(MINIMIZED);
}

TopWindow& TopWindow::FullScreen(bool b)
{
	SetMode(FULLSCREEN);
	return *this;
}

void TopWindow::Maximize(bool effect)
{
	SetMode(MAXIMIZED);
}

void TopWindow::Overlap(bool effect)
{
	SetMode(OVERLAPPED);
}

TopWindow& TopWindow::TopMost(bool b, bool)
{
	GuiLock __; 
	topmost = b;
	SyncTopMost();
	return *this;
}

bool TopWindow::IsTopMost() const
{
	GuiLock __; 
	return topmost;
}

void TopWindow::GuiPlatformConstruct()
{
	topmost = false;
}

void TopWindow::GuiPlatformDestruct()
{
}

void TopWindow::SerializePlacement(Stream& s, bool reminimize)
{
	GuiLock __;
	int version = 0;
	s / version;
	Rect rect = GetRect();
	s % overlapped % rect;
	bool mn = state == MINIMIZED;
	bool mx = state == MAXIMIZED;
	s.Pack(mn, mx);
	LLOG("TopWindow::SerializePlacement / " << (s.IsStoring() ? "write" : "read"));
	LLOG("minimized = " << mn << ", maximized = " << mx);
	LLOG("rect = " << rect << ", overlapped = " << overlapped);
	if(s.IsLoading()) {
		if(mn) rect = overlapped;
		Rect limit = GetWorkArea();
		Rect fm = GetFrameMargins();
		limit.left += fm.left;
		limit.right -= fm.right;
		limit.top += fm.top;
		limit.bottom -= fm.bottom;
		Size sz = min(rect.Size(), limit.Size());
		rect = RectC(
			minmax(rect.left, limit.left, limit.right - sz.cx),
			minmax(rect.top,  limit.top,  limit.bottom - sz.cy),
			sz.cx, sz.cy);
		state = OVERLAPPED;
		if(mn && reminimize)
			state = MINIMIZED;
		if(mx)
			state = MAXIMIZED;
		if(state == OVERLAPPED)
			SetRect(rect);
		if(IsOpen()) {
			if(state == MINIMIZED)
				Minimize(false);
			if(state == MAXIMIZED)
				Maximize(false);
		}
	}
}

END_UPP_NAMESPACE

#endif
