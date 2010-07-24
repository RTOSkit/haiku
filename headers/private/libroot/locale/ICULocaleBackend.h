/*
 * Copyright 2010, Oliver Tappe, zooey@hirschkaefer.de.
 * Distributed under the terms of the MIT License.
 */
#ifndef _ICU_LOCALE_BACKEND_H
#define _ICU_LOCALE_BACKEND_H


#include "LocaleBackend.h"

#include <locale.h>
#include <timelocal.h>

#include "ICUCollateData.h"
#include "ICUCtypeData.h"
#include "ICUMessagesData.h"
#include "ICUMonetaryData.h"
#include "ICUNumericData.h"
#include "ICUTimeData.h"


namespace BPrivate {


class ICULocaleBackend : public LocaleBackend {
public:
								ICULocaleBackend();
	virtual						~ICULocaleBackend();

	virtual void				Initialize(LocaleDataBridge* dataBridge);

	virtual	const char*			SetLocale(int category,
									const char* posixLocaleName);
	virtual	const struct lconv*	LocaleConv();
	virtual	const struct lc_time_t*	LCTimeInfo();

	virtual	int					IsWCType(wint_t wc, wctype_t charClass);
	virtual	status_t			ToWCTrans(wint_t wc, wctrans_t transition,
									wint_t& result);

	virtual	const char*			GetLanginfo(int index);

	virtual	status_t			Strcoll(const char* a, const char* b, int& out);
	virtual status_t			Strxfrm(char* out, const char* in, size_t size,
									size_t& outSize);

private:
			const char*			_QueryLocale(int category);
			const char*			_SetPosixLocale(int category);

			// buffer for locale names (up to one per category)
			char				fLocaleDescription[512];

			// data containers
			struct lconv 	 	fLocaleConv;
			struct lc_time_t 	fLCTimeInfo;

			// these work on the data containers above
			ICUCollateData		fCollateData;
			ICUCtypeData		fCtypeData;
			ICUMessagesData		fMessagesData;
			ICUMonetaryData		fMonetaryData;
			ICUNumericData		fNumericData;
			ICUTimeData			fTimeData;

			// static posix langinfo data
			const char**		fPosixLanginfo;
};


}	// namespace BPrivate


#endif	// _ICU_LOCALE_BACKEND_H