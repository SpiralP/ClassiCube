#include "Widgets.h"
#include "GraphicsAPI.h"
#include "Drawer2D.h"
#include "GraphicsCommon.h"
#include "ExtMath.h"
#include "Funcs.h"
#include "Window.h"

void Widget_SetLocation(Widget* widget, Anchor horAnchor, Anchor verAnchor, Int32 xOffset, Int32 yOffset) {
	widget->HorAnchor = horAnchor; widget->VerAnchor = verAnchor;
	widget->XOffset = xOffset; widget->YOffset = yOffset;
	widget->Reposition(widget);
}

void TextWidget_SetHeight(TextWidget* widget, Int32 height) {
	if (widget->ReducePadding) {
		Drawer2D_ReducePadding_Height(&height, font.Size, 4);
	}
	widget->DefaultHeight = height;
	widget->Base.Height = height;
}

void TextWidget_Init(GuiElement* elem) {
	TextWidget* widget = (TextWidget*)elem;
	Int32 height = Drawer2D_FontHeight(widget->Font, true);
	TextWidget_SetHeight(widget, height);
}

void TextWidget_Render(GuiElement* elem, Real64 delta) {
	TextWidget* widget = (TextWidget*)elem;	
	if (Texture_IsValid(&widget->Texture)) {
		Texture_RenderShaded(&widget->Texture, widget->Col);
	}
}

void TextWidget_Free(Widget* elem) {
	TextWidget* widget = (TextWidget*)elem;
	Gfx_DeleteTexture(&widget->Texture.ID);
}

void TextWidget_Reposition(Widget* elem) {
	TextWidget* widget = (TextWidget*)elem;
	Int32 oldX = elem->X, oldY = elem->Y;
	Widget_DoReposition(elem);
	widget->Texture.X += elem->X - oldX;
	widget->Texture.Y += elem->Y - oldY;
}

void TextWidget_Create(TextWidget* widget, STRING_TRANSIENT String* text, void* font) {
	Widget_Init(&widget->Base);
	widget->Font = font;
	widget->Base.Reposition  = TextWidget_Reposition;
	widget->Base.Base.Init   = TextWidget_Init;
	widget->Base.Base.Render = TextWidget_Render;
	widget->Base.Base.Free   = TextWidget_Free;
}

void TextWidget_SetText(TextWidget* widget, STRING_TRANSIENT String* text) {
	Gfx_DeleteTexture(&widget->Texture.ID);
	Widget* elem = (Widget*)widget;
	if (Drawer2D_IsEmptyText(text)) {
		widget->Texture = Texture_MakeInvalid();
		elem->Width = 0; elem->Height = widget->DefaultHeight;
	} else {
		DrawTextArgs args;
		DrawTextArgs_Make(&args, text, widget->Font, true);
		widget->Texture = Drawer2D_MakeTextTexture(&args, 0, 0);
		if (widget->ReducePadding) {
			Drawer2D_ReducePadding_Tex(&widget->Texture, font.Size, 4);
		}

		elem->Width = widget->Texture.Width; elem->Height = widget->Texture.Height;
		elem->Reposition(elem);
		widget->Texture.X = elem->X; widget->Texture.Y = elem->Y;
	}
}

#define TABLEWIDGET_MAX_ROWS_DISPLAYED 8
#define SCROLLWIDGET_WIDTH 22
#define SCROLLWIDGET_BORDER 2
#define SCROLLWIDGET_NUBS_WIDTH 3
PackedCol ScrollWidget_BackCol = { 10, 10, 10, 220 };
PackedCol ScrollWidget_BarCol = { 100, 100, 100, 220 };
PackedCol ScrollWidget_HoverCol = { 122, 122, 122, 220 };

void ScrollbarWidget_Init(GuiElement* elem) { }
void ScrollbarWidget_Free(GuiElement* elem) { }

Real32 ScrollbarWidget_GetScale(ScrollbarWidget* widget) {
	Real32 rows = (Real32)widget->TotalRows;
	return (widget->Base.Height - SCROLLWIDGET_BORDER * 2) / rows;
}

void ScrollbarWidget_GetScrollbarCoords(ScrollbarWidget* widget, Int32* y, Int32* height) {
	Real32 scale = ScrollbarWidget_GetScale(widget);
	*y = Math_Ceil(widget->ScrollY * scale) + SCROLLWIDGET_BORDER;
	*height = Math_Ceil(TABLEWIDGET_MAX_ROWS_DISPLAYED * scale);
	*height = min(*y + *height, widget->Base.Height - SCROLLWIDGET_BORDER) - *y;
}

void ScrollbarWidget_Render(GuiElement* elem, Real64 delta) {
	ScrollbarWidget* widget = (ScrollbarWidget*)elem;
	Widget* elemW = &widget->Base;
	Int32 x = elemW->X, width = elemW->Width;
	GfxCommon_Draw2DFlat(x, elemW->Y, width, elemW->Height, ScrollWidget_BackCol);

	Int32 y, height;
	ScrollbarWidget_GetScrollbarCoords(widget, &y, &height);
	x += SCROLLWIDGET_BORDER; y += elemW->Y;
	width -= SCROLLWIDGET_BORDER * 2; 

	Point2D mouse = Window_GetDesktopCursorPos();
	bool hovered = mouse.Y >= y && mouse.Y < (y + height) && mouse.X >= x && mouse.X < (x + width);
	PackedCol barCol = hovered ? ScrollWidget_HoverCol : ScrollWidget_BarCol;
	GfxCommon_Draw2DFlat(x, y, width, height, barCol);

	if (height < 20) return;
	x += SCROLLWIDGET_NUBS_WIDTH; y += (height / 2);
	width -= SCROLLWIDGET_NUBS_WIDTH * 2;

	GfxCommon_Draw2DFlat(x, y - 1 - 4, width, SCROLLWIDGET_BORDER, ScrollWidget_BackCol);
	GfxCommon_Draw2DFlat(x, y - 1,     width, SCROLLWIDGET_BORDER, ScrollWidget_BackCol);
	GfxCommon_Draw2DFlat(x, y - 1 + 4, width, SCROLLWIDGET_BORDER, ScrollWidget_BackCol);
}

bool ScrollbarWidget_HandlesMouseScroll(GuiElement* elem, Real32 delta) {
	ScrollbarWidget* widget = (ScrollbarWidget*)elem;
	Int32 steps = Math_AccumulateWheelDelta(&widget->ScrollingAcc, delta);
	widget->ScrollY -= steps;
	ScrollbarWidget_ClampScrollY(widget);
	return true;
}

bool ScrollbarWidget_HandlesMouseMove(GuiElement* elem, Int32 x, Int32 y) {
	ScrollbarWidget* widget = (ScrollbarWidget*)elem;
	if (widget->DraggingMouse) {
		y -= widget->Base.Y;
		Real32 scale = ScrollbarWidget_GetScale(widget);
		widget->ScrollY = (Int32)((y - widget->MouseOffset) / scale);
		ScrollbarWidget_ClampScrollY(widget);
		return true;
	}
	return false;
}

bool ScrollbarWidget_HandlesMouseClick(GuiElement* elem, Int32 x, Int32 y, MouseButton btn) {
	ScrollbarWidget* widget = (ScrollbarWidget*)elem;
	if (widget->DraggingMouse) return true;
	if (btn != MouseButton_Left) return false;
	if (x < widget->Base.X || x >= widget->Base.X + widget->Base.Width) return false;

	y -= widget->Base.Y;
	Int32 curY, height;
	ScrollbarWidget_GetScrollbarCoords(widget, &curY, &height);

	if (y < curY) {
		widget->ScrollY -= TABLEWIDGET_MAX_ROWS_DISPLAYED;
	} else if (y >= curY + height) {
		widget->ScrollY += TABLEWIDGET_MAX_ROWS_DISPLAYED;
	} else {
		widget->DraggingMouse = true;
		widget->MouseOffset = y - curY;
	}
	ScrollbarWidget_ClampScrollY(widget);
	return true;
}

bool ScrollbarWidget_HandlesMouseUp(GuiElement* elem, Int32 x, Int32 y, MouseButton btn) {
	ScrollbarWidget* widget = (ScrollbarWidget*)elem;
	widget->DraggingMouse = false;
	widget->MouseOffset = 0;
	return true;
}

void ScrollbarWidget_Create(ScrollbarWidget* widget) {
	Widget_Init(widget);
	widget->Base.Width = SCROLLWIDGET_WIDTH;
	widget->Base.Base.Init = ScrollbarWidget_Init;
	widget->Base.Base.Render = ScrollbarWidget_Render;
	widget->Base.Base.Free = ScrollbarWidget_Free;
	widget->Base.Base.HandlesMouseUp = ScrollbarWidget_HandlesMouseUp;

	widget->TotalRows = 0;
	widget->ScrollY = 0;
	widget->ScrollingAcc = 0.0f;
	widget->DraggingMouse = false;
	widget->MouseOffset = 0;
}

void ScrollbarWidget_ClampScrollY(ScrollbarWidget* widget) {
	Int32 maxRows = widget->TotalRows - TABLEWIDGET_MAX_ROWS_DISPLAYED;
	if (widget->ScrollY >= maxRows) widget->ScrollY = maxRows;
	if (widget->ScrollY < 0) widget->ScrollY = 0;
}