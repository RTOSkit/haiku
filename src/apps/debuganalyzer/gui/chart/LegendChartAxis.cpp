/*
 * Copyright 2009, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Distributed under the terms of the MIT License.
 */

#include "chart/LegendChartAxis.h"

#include <limits.h>
#include <stdio.h>

#include <algorithm>
#include <new>

#include <Font.h>
#include <View.h>

#include "chart/ChartLegend.h"
#include "chart/ChartAxisLegendSource.h"


static const int32 kChartRulerDistance = 2;
static const int32 kRulerSize = 3;
static const int32 kRulerMarkSize = 3;
static const int32 kRulerLegendDistance = 2;
static const int32 kChartLegendDistance
	= kChartRulerDistance + kRulerSize + kRulerMarkSize + kRulerLegendDistance;



struct LegendChartAxis::LegendInfo {
	ChartLegend*	legend;
	double			value;
	BSize			size;

	LegendInfo(ChartLegend* legend, double value, BSize size)
		:
		legend(legend),
		value(value),
		size(size)
	{
	}

	~LegendInfo()
	{
		delete legend;
	}
};


float
LegendChartAxis::_LegendPosition(double value, float legendSize,
	float totalSize, double scale)
{
	float position = (value - fRange.min) * scale - legendSize / 2;
	if (position + legendSize > totalSize)
		position = totalSize - legendSize;
	if (position < 0)
		position = 0;
	return position;
}


void
LegendChartAxis::_FilterLegends(int32 totalSize, int32 spacing,
	float BSize::* sizeField)
{
printf("LegendChartAxis::_FilterLegends(%ld, %ld)\n", totalSize, spacing);
	// compute the min/max legend levels
	int32 legendCount = fLegends.CountItems();
	int32 minLevel = INT_MAX;
	int32 maxLevel = 0;

	for (int32 i = 0; i < legendCount; i++) {
		LegendInfo* info = fLegends.ItemAt(i);
		int32 level = info->legend->Level();
		if (level < minLevel)
			minLevel = level;
		if (level > maxLevel)
			maxLevel = level;
	}

	if (maxLevel <= 0)
		return;

	double rangeSize = fRange.max - fRange.min;
	if (rangeSize == 0)
		rangeSize = 1.0;
	double scale = (double)totalSize / rangeSize;

	// Filter out all higher level legends colliding with lower level or
	// preceeding same-level legends. We iterate backwards from the lower to
	// the higher levels
	for (int32 level = std::max(minLevel, 0L); level <= maxLevel;) {
printf("  level: %ld\n", level);
		legendCount = fLegends.CountItems();

		// get the first legend position/end
		LegendInfo* info = fLegends.ItemAt(0);
		float position = _LegendPosition(info->value, info->size.*sizeField,
			(float)totalSize, scale);;

		int32 previousEnd = (int32)ceilf(position + info->size.*sizeField);
		int32 previousLevel = info->legend->Level();
printf("     first legend: pos: %f, size: %f\n", position, info->size.*sizeField);

		for (int32 i = 1; (info = fLegends.ItemAt(i)) != NULL; i++) {
			float position = _LegendPosition(info->value, info->size.*sizeField,
				(float)totalSize, scale);;
printf("     legend %ld: pos: %f, size: %f\n", i, position, info->size.*sizeField);

			if (position - spacing < previousEnd
				&& (previousLevel <= level
					|| info->legend->Level() <= level)
				&& std::max(previousLevel, info->legend->Level()) > 0) {
				// The item intersects with the previous one -- remove the
				// one at the higher level.
				if (info->legend->Level() >= previousLevel) {
					// This item is at the higher level -- remove it.
					delete fLegends.RemoveItemAt(i);
					i--;
					continue;
				}

				// The previous item is at the higher level -- remove it.
				delete fLegends.RemoveItemAt(i - 1);
				i--;
			}

			if (i == 0 && position < 0)
				position = 0;
			previousEnd = (int32)ceilf(position + info->size.*sizeField);
			previousLevel = info->legend->Level();
		}

		// repeat with the level as long as we've removed something
		if (legendCount == fLegends.CountItems())
			level++;
	}
}


LegendChartAxis::LegendChartAxis(ChartAxisLegendSource* legendSource,
	ChartLegendRenderer* legendRenderer)
	:
	fLegendSource(legendSource),
	fLegendRenderer(legendRenderer),
	fLocation(CHART_AXIS_BOTTOM),
	fRange(),
	fFrame(),
	fLegends(20, true),
	fHorizontalSpacing(20),
	fVerticalSpacing(10),
	fLayoutValid(false)
{
}


LegendChartAxis::~LegendChartAxis()
{
}


void
LegendChartAxis::SetLocation(ChartAxisLocation location)
{
	if (location != fLocation) {
		fLocation = location;
		_InvalidateLayout();
	}
}


void
LegendChartAxis::SetRange(const ChartDataRange& range)
{
	if (range != fRange) {
		fRange = range;
		_InvalidateLayout();
	}
}


void
LegendChartAxis::SetFrame(BRect frame)
{
printf("LegendChartAxis::SetFrame((%f, %f) - (%f, %f))\n", frame.left, frame.top, frame.right, frame.bottom);
	if (frame != fFrame) {
		fFrame = frame;
		_InvalidateLayout();
	}
}


BSize
LegendChartAxis::PreferredSize(BView* view)
{
// TODO: Implement for real!
	BSize size = fLegendRenderer->MaximumLegendSize(view);
	if (fLocation == CHART_AXIS_LEFT || fLocation == CHART_AXIS_RIGHT) {
		size.width += kChartLegendDistance;
		size.height = std::max(size.height * 4, 100.0f);
	} else {
		size.width = std::max(size.width * 4, 100.0f);
		size.height += kChartLegendDistance;
	}

	return size;
}


void
LegendChartAxis::Render(BView* view, BRect updateRect)
{
printf("LegendChartAxis::Render()\n");
	if (!_ValidateLayout(view))
		return;

	if (fLocation == CHART_AXIS_BOTTOM) {
printf("  rendering...\n");
		float totalSize = floorf(fFrame.Width()) + 1;
		double rangeSize = fRange.max - fRange.min;
		if (rangeSize == 0)
			rangeSize = 1.0;
		double scale = (double)totalSize / rangeSize;
		float BSize::* sizeField = &BSize::width;

		// draw the ruler
		float rulerLeft = fFrame.left;
		float rulerTop = fFrame.top + kChartRulerDistance;
		float rulerRight = fFrame.right;
		float rulerBottom = rulerTop + kRulerSize;
printf("  ruler: (%f, %f) - (%f, %f)\n", rulerLeft, rulerTop, rulerRight, rulerBottom);

		rgb_color black = { 0, 0, 0, 255 };
		view->BeginLineArray(3 + fLegends.CountItems());
		view->AddLine(BPoint(rulerLeft, rulerTop),
			BPoint(rulerLeft, rulerBottom), black);
		view->AddLine(BPoint(rulerLeft, rulerBottom),
			BPoint(rulerRight, rulerBottom), black);
		view->AddLine(BPoint(rulerRight, rulerBottom),
			BPoint(rulerRight, rulerTop), black);

		// marks
		for (int32 i = 0; LegendInfo* info = fLegends.ItemAt(i); i++) {
			float position = (info->value - fRange.min) * scale;
			position += rulerLeft;
printf("  drawing mark at (%f, %f)\n", position, rulerBottom);
			view->AddLine(BPoint(position, rulerBottom),
				BPoint(position, rulerBottom + kRulerMarkSize), black);
		}
		view->EndLineArray();

		// draw the legends
		float legendTop = rulerBottom + kRulerMarkSize + kRulerLegendDistance;

		for (int32 i = 0; LegendInfo* info = fLegends.ItemAt(i); i++) {
			float position = _LegendPosition(info->value, info->size.*sizeField,
				(float)totalSize, scale);;
printf("  legend %ld: position: (%f, %f), size: (%f, %f)\n", i, position, legendTop, info->size.width, info->size.height);

			fLegendRenderer->RenderLegend(info->legend, view,
				BPoint(rulerLeft + position, legendTop));
		}
	}
}


void
LegendChartAxis::_InvalidateLayout()
{
	fLayoutValid = false;
}


bool
LegendChartAxis::_ValidateLayout(BView* view)
{
	if (fLayoutValid)
		return true;
printf("LegendChartAxis::_ValidateLayout()\n");

	fLegends.MakeEmpty();

	int32 width = fFrame.IntegerWidth() + 1;
	int32 height = fFrame.IntegerHeight() + 1;
printf("  width: %ld, height: %ld\n", width, height);

	fLegendRenderer->GetMinimumLegendSpacing(view, &fHorizontalSpacing,
		&fVerticalSpacing);

	// estimate the maximum legend count we might need
	int32 maxLegends;
	if (fLocation == CHART_AXIS_LEFT || fLocation == CHART_AXIS_RIGHT)
		maxLegends = height / (10 + fVerticalSpacing);
	else
		maxLegends = width / (20 + fHorizontalSpacing);

printf("  max %ld legends\n", maxLegends);
	if (maxLegends == 0)
{
printf("  too small for any legends!\n");
		return false;
}

	// get the legends
	ChartLegend* legends[maxLegends];
	double values[maxLegends];

	int32 legendCount = fLegendSource->GetAxisLegends(fRange, legends, values,
		maxLegends);
printf("  got %ld legends\n", legendCount);
	if (legendCount == 0)
{
printf("  got no legends from source!\n");
		return false;
}

printf("  range: %f - %f\n", fRange.min, fRange.max);
	// create legend infos
	for (int32 i = 0; i < legendCount; i++) {
		ChartLegend* legend = legends[i];
		BSize size = fLegendRenderer->LegendSize(legend, view);
		LegendInfo* info = new(std::nothrow) LegendInfo(legend, values[i],
			size);
printf("    legend %ld: size: (%f, %f), value: %f\n", i, size.width, size.height, info->value);
		if (info == NULL || !fLegends.AddItem(info)) {
			// TODO: Report error!
			delete info;
			for (int32 k = i; k < legendCount; k++)
				delete legends[k];
printf("  failed to create legend info!\n");
			return false;
		}
	}

	if (fLocation == CHART_AXIS_LEFT || fLocation == CHART_AXIS_RIGHT)
		_FilterLegends(height, fVerticalSpacing, &BSize::height);
	else
		_FilterLegends(width, fHorizontalSpacing, &BSize::width);

	fLayoutValid = true;
	return true;
}
