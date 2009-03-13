/*
 * Copyright 2007-2009, Haiku. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 *	Authors:
 *		Stefano Ceccherini (burton666@libero.it)
 */

#include "SmartTabView.h"

#include <MenuItem.h>
#include <Message.h>
#include <Messenger.h>
#include <PopUpMenu.h>
#include <Screen.h>
#include <Window.h>

#include <stdio.h>

const static uint32 kCloseTab = 'ClTb';

SmartTabView::SmartTabView(BRect frame, const char *name, button_width width, 
				uint32 resizingMode, uint32 flags)
	:
	BTabView(frame, name, width, resizingMode, flags),
	fInsets(0, 0, 0, 0)
{
	// Resize the container view to fill the complete tab view for single-tab
	// mode. Later, when more than one tab is added, we shrink the container
	// view again.
	frame.OffsetTo(B_ORIGIN);
	ContainerView()->MoveTo(frame.LeftTop());
	ContainerView()->ResizeTo(frame.Width(), frame.Height());
}


SmartTabView::~SmartTabView()
{
}


void
SmartTabView::SetInsets(float left, float top, float right, float bottom)
{
	fInsets.left = left;
	fInsets.top = top;
	fInsets.right = right;
	fInsets.bottom = bottom;
}


void
SmartTabView::MouseDown(BPoint point)
{
	bool handled = false;

	if (CountTabs() > 1) {
		int32 tabIndex = _ClickedTabIndex(point);
		if (tabIndex >= 0) {		
			int32 buttons;
			Window()->CurrentMessage()->FindInt32("buttons", &buttons);
			if (buttons & B_SECONDARY_MOUSE_BUTTON) {	
				BMessage *message = new BMessage(kCloseTab);
				message->AddInt32("index", tabIndex);
				
				BPopUpMenu *popUpMenu = new BPopUpMenu("tab menu");
				popUpMenu->AddItem(new BMenuItem("Close Tab", message));
				popUpMenu->SetAsyncAutoDestruct(true);
				popUpMenu->SetTargetForItems(BMessenger(this));
				popUpMenu->Go(ConvertToScreen(point), true, true, true);
								
				handled = true;		
			} else if (buttons & B_TERTIARY_MOUSE_BUTTON) {
				RemoveAndDeleteTab(tabIndex);
				handled = true;
			}
		}	
	}

	if (!handled)
		BTabView::MouseDown(point);
		
}


void
SmartTabView::AttachedToWindow()
{
	BTabView::AttachedToWindow();
}


void
SmartTabView::AllAttached()
{
	BTabView::AllAttached();
}


void
SmartTabView::MessageReceived(BMessage *message)
{
	switch (message->what) {
		case kCloseTab:
		{
			int32 tabIndex = 0;
			if (message->FindInt32("index", &tabIndex) == B_OK)
				RemoveAndDeleteTab(tabIndex);
			break;
		}		
		default:
			BTabView::MessageReceived(message);
			break;	
	}
}


void
SmartTabView::Select(int32 index)
{
	BTabView::Select(index);
	BView *view = ViewForTab(index);
	if (view != NULL) {
		view->MoveTo(fInsets.LeftTop());
		view->ResizeTo(ContainerView()->Bounds().Width()
				- fInsets.left - fInsets.right,
			ContainerView()->Bounds().Height() - fInsets.top - fInsets.bottom);
	}
}


void
SmartTabView::RemoveAndDeleteTab(int32 index)
{
	// Select another tab
	if (index == Selection()) {
		if (index > 0)
			Select(index - 1);
		else if (index < CountTabs())
			Select(index + 1);
	}	
	delete RemoveTab(index);
}


void
SmartTabView::AddTab(BView *target, BTab *tab)
{
	if (target == NULL)
		return;

	BTabView::AddTab(target, tab);
	
	if (CountTabs() == 1) {
		// Call select on the tab, since
		// we're resizing the contained view
		// inside that function
		Select(0);
	} else if (CountTabs() == 2) {
		// Need to resize the view, since we're
		// switching from "normal" to tabbed mode
		ContainerView()->ResizeBy(0, -TabHeight());
		ContainerView()->MoveBy(0, TabHeight());	

		BScreen screen(Window());
		if (Window()->Frame().bottom + TabHeight() > screen.Frame().bottom - 5)
			Window()->MoveBy(0, -TabHeight());

		Window()->ResizeBy(0, TabHeight());
	}
	
	Invalidate(TabFrame(CountTabs() - 1).InsetByCopy(-2, -2));
}


BTab *
SmartTabView::RemoveTab(int32 index)
{
	if (CountTabs() == 2) {
		// see above
		BScreen screen(Window());
		if (Window()->Frame().bottom > screen.Frame().bottom - 5 - TabHeight())
			Window()->MoveBy(0, TabHeight());
		Window()->ResizeBy(0, -TabHeight());
		ContainerView()->MoveBy(0, -TabHeight());		
		ContainerView()->ResizeBy(0, TabHeight());	
	}
	return BTabView::RemoveTab(index);
}


BRect
SmartTabView::DrawTabs()
{
	if (CountTabs() > 1)
		return BTabView::DrawTabs();
	return BRect();
}


int32
SmartTabView::_ClickedTabIndex(const BPoint &point)
{
	if (point.y <= TabHeight()) {
		for (int32 i = 0; i < CountTabs(); i++) {
			if (TabFrame(i).Contains(point))
				return i;
		}
	}

	return -1;
}
