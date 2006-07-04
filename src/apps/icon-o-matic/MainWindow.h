/*
 * Copyright 2006, Haiku.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Stephan Aßmus <superstippi@gmx.de>
 */

#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <Window.h>

#include "Observer.h"

class BMenuBar;
class BMenuItem;
class CanvasView;
class Document;
class IconEditorApp;
class IconView;
class PathListView;
class ShapeListView;
class SwatchGroup;
class TransformerListView;

class MultipleManipulatorState;

class MainWindow : public BWindow,
				   public Observer {
 public:
								MainWindow(IconEditorApp* app,
										   Document* document);
	virtual						~MainWindow();

	// BWindow interface
	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

	// Observer interface
	virtual	void				ObjectChanged(const Observable* object);

 private:
			void				_Init();
			BView*				_CreateGUI(BRect frame);
			BMenuBar*			_CreateMenuBar(BRect frame);

	IconEditorApp*				fApp;
	Document*					fDocument;

	BMenuItem*					fUndoMI;
	BMenuItem*					fRedoMI;

	CanvasView*					fCanvasView;
	SwatchGroup*				fSwatchGroup;

	IconView*					fIconPreview16;
	IconView*					fIconPreview32;
	IconView*					fIconPreview48;
	IconView*					fIconPreview64;

	PathListView*				fPathListView;
	ShapeListView*				fShapeListView;
	TransformerListView*		fTransformerListView;

	// TODO: for testing only...
	MultipleManipulatorState*	fState;
};

#endif // MAIN_WINDOW_H
