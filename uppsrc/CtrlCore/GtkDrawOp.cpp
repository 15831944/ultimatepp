#include <CtrlCore/CtrlCore.h>

#ifdef GUI_GTK

NAMESPACE_UPP

#define LLOG(x)

void SystemDraw::SetColor(Color c)
{
	cairo_set_source_rgb(cr, c.GetR() / 255.0, c.GetG() / 255.0, c.GetB() / 255.0);
}

dword SystemDraw::GetInfo() const
{
	return DRAWTEXTLINES;
}

Point SystemDraw::GetOffset() const
{
	return offset.GetCount() ? offset.Top() : Point(0, 0);
}

Rect  SystemDraw::GetClip() const
{
	return clip.GetCount() ? clip.Top() : Rect(-999999, -999999, 999999, 999999);
}

void SystemDraw::Push()
{
	cairo_save(cr);
	offset.Add(GetOffset());
	clip.Add(GetClip());
}

void SystemDraw::Pop()
{
	if(offset.GetCount())
		offset.Drop();
	if(clip.GetCount())
		clip.Drop();
	cairo_restore(cr);
}

void SystemDraw::BeginOp()
{
	Push();
}

void SystemDraw::EndOp()
{
	Pop();
}

void SystemDraw::OffsetOp(Point p)
{
	Push();
	offset.Top() += p;
	cairo_translate(cr, p.x, p.y);
}

void SystemDraw::RectPath(const Rect& r)
{
	cairo_rectangle(cr, r.left, r.top, r.GetWidth(), r.GetHeight());
}

bool SystemDraw::ClipOp(const Rect& r)
{
	Push();
	clip.Top() &= r.Offseted(GetOffset());
	RectPath(r);
	cairo_clip(cr);
	return true;
}

bool SystemDraw::ClipoffOp(const Rect& r)
{
	Push();
	clip.Top() &= r.Offseted(GetOffset());
	offset.Top() += r.TopLeft();
	RectPath(r);
	cairo_clip(cr);
	cairo_translate(cr, r.left, r.top);
	return true;
}

bool SystemDraw::ExcludeClipOp(const Rect& r)
{
	RectPath(Rect(-99999, -99999, 99999, r.top));
	RectPath(Rect(-99999, r.top, r.left, 99999));
	RectPath(Rect(r.right, r.top, 99999, 99999));
	RectPath(Rect(r.left, r.bottom, r.right, 99999));
	cairo_clip(cr);
	return true;
}

bool SystemDraw::IntersectClipOp(const Rect& r)
{
	RectPath(r);
	cairo_clip(cr);
	return true;
}

bool SystemDraw::IsPaintingOp(const Rect& r) const
{
	return r.Offseted(GetOffset()).Intersects(GetClip());
}

Rect SystemDraw::GetPaintRect() const
{
	return Rect(0, 0, INT_MAX, INT_MAX);
}

void SystemDraw::DrawRectOp(int x, int y, int cx, int cy, Color color)
{
	if(IsNull(color))
		return;
	cairo_rectangle(cr, x, y, cx, cy);
	if(color == InvertColor()) {
		SetColor(White());
		cairo_set_operator(cr, CAIRO_OPERATOR_DIFFERENCE);
		cairo_fill(cr);
		cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	}
	else {
		SetColor(color);
		cairo_fill(cr);
	}
}

static void sDrawLineStroke(cairo_t *cr, int width)
{
	static double dash[] = { 18, 6 };
	static double dot[] = { 3, 3 };
	static double dashdot[] = { 9, 6, 3, 6 };
	static double dashdotdot[] = { 9, 3, 3, 3, 3, 3 };
	switch(width) {
	case PEN_NULL:       return;
	case PEN_DASH:       cairo_set_dash(cr, dash, __countof(dash), 0); break;
	case PEN_DOT:        cairo_set_dash(cr, dot, __countof(dot), 0); break;
	case PEN_DASHDOT:    cairo_set_dash(cr, dashdot, __countof(dashdot), 0); break;
	case PEN_DASHDOTDOT: cairo_set_dash(cr, dashdotdot, __countof(dashdotdot), 0); break;
	default:             break;
	}
	cairo_set_line_width(cr, width < 0 ? 1 : width);
	cairo_stroke(cr);
	if(width < 0)
		cairo_set_dash(cr, dot, 0, 0);
}

void SystemDraw::DrawLineOp(int x1, int y1, int x2, int y2, int width, Color color)
{
	SetColor(color);
	if(width == PEN_SOLID)
		width = 1;
	if(y1 == y2 && width >= 0)
		DrawRect(x1, y1, x2 - x1, width, color);
	else
	if(x1 == x2 && width >= 0)
		DrawRect(x1, y1, width, y2 - y1, color);
	else {
		int w = width < 0 ? 1 : width;
		double d = w / 2.0;
		if(y1 == y2) {
			cairo_move_to(cr, min(x1, x1) + 0.5, y1 + d);
			cairo_line_to(cr, max(x1, x2) - 0.5, y1 + d);
		}
		else
		if(x1 == x2) {
			cairo_move_to(cr, x1 + d, min(y1, y2) + 0.5);
			cairo_line_to(cr, x1 + d, max(y1, y2) - 0.5);
		}
		else {
			cairo_move_to(cr, x1, y1);
			cairo_line_to(cr, x2, y2);
		}
		sDrawLineStroke(cr, width);
	}
}

void SystemDraw::DrawPolyPolylineOp(const Point *vertices, int vertex_count, const int *counts,
                                    int count_count, int width, Color color, Color doxor)
{
	while(--count_count >= 0) {
		const Point *pp = vertices;
		vertices += *counts++;
		cairo_move_to(cr, pp->x, pp->y);
		while(++pp < vertices)
			cairo_line_to(cr, pp->x, pp->y);
		SetColor(color);
		sDrawLineStroke(cr, width);
	}
}

cairo_surface_t *CreateCairoSurface(const Image& img);

void SystemDraw::DrawPolyPolyPolygonOp(const Point *vertices, int vertex_count,
                                  const int *subpolygon_counts, int scc, const int *disjunct_polygon_counts,
                                  int dpcc, Color color, int width, Color outline, uint64 pattern, Color doxor)
{
	Image fill_img;
	if(pattern && !IsNull(color)) {
		ImageBuffer ibuf(8, 8);
		RGBA r[2] = { color, RGBAZero() };
		for(RGBA *out = ibuf, *end = out + 64; out < end; pattern >>= 1)
			*out++ = r[(byte)pattern & 1];
		fill_img = ibuf;
	}
	
	while(--dpcc >= 0) {
		const Point *sp = vertices;
		vertices += *disjunct_polygon_counts++;
		while(sp < vertices) {
			const Point *pp = sp;
			sp += *subpolygon_counts++;
			cairo_move_to(cr, pp->x, pp->y);
			while(++pp < sp)
				cairo_line_to(cr, pp->x, pp->y);
			cairo_close_path(cr);
		}
		if(!IsNull(fill_img)) {
			cairo_surface_t *surface = CreateCairoSurface(fill_img);
			Push();
			cairo_set_source_surface(cr, surface, 0, 0);
			cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
			cairo_fill_preserve(cr);
			Pop();
			cairo_surface_destroy(surface);
		}
		else
		if(!IsNull(color)) {
			SetColor(color);
			cairo_fill_preserve(cr);
		}
		if(!IsNull(outline)) {
			SetColor(outline);
			sDrawLineStroke(cr, width);
		}
	}
	
}

void SystemDraw::DrawArcOp(const Rect& rc, Point start, Point end, int width, Color color)
{
	if(rc.Width() <= 0 || rc.Height() <= 0)
		return;
	Sizef radius = Sizef(rc.Size()) / 2.0;
	Pointf center = Pointf(rc.TopLeft()) + radius;
	double ang1 = Bearing((Pointf(start) - center) / radius);
	double ang2 = Bearing((Pointf(end) - center) / radius);
	
	cairo_move_to(cr, center.x + radius.cx * cos(ang1), center.y + radius.cy * sin(ang1));
	cairo_save(cr);
	cairo_translate(cr, rc.left + radius.cx, rc.top + radius.cy);
	cairo_scale(cr, radius.cx, radius.cy);
	cairo_arc_negative(cr, 0, 0, 1, ang1, ang2);
	cairo_restore(cr);

	SetColor(color);
	sDrawLineStroke(cr, width);
}

void SystemDraw::DrawEllipseOp(const Rect& r, Color color, int pen, Color pencolor)
{
	cairo_save(cr);
	Sizef h = Sizef(r.GetSize()) / 2.0;
	cairo_translate (cr, r.left + h.cx, r.top + h.cy);
	cairo_scale(cr, h.cx, h.cy);
	cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
	cairo_restore(cr);
	if(!IsNull(color)) {
		SetColor(color);
		cairo_fill_preserve(cr);
	}
	if(!IsNull(pencolor)) {
		SetColor(pencolor);
		sDrawLineStroke(cr, pen);
	}
}

END_UPP_NAMESPACE

#endif
