/*
 * Lips3Cap.h
 * Copyright 1999-2000 Y.Takagi. All Rights Reserved.
 */

#ifndef __LIPS3CAP_H
#define __LIPS3CAP_H

#include "PrinterCap.h"

class Lips3Cap : public PrinterCap {
public:
	Lips3Cap(const PrinterData *printer_data);
	virtual int countCap(CapID) const;
	virtual bool isSupport(CapID) const;
	virtual const BaseCap **enumCap(CapID) const;
};

#endif	/* __LIPS3CAP_H */
