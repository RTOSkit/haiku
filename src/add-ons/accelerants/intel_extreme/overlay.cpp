/*
 * Copyright 2006, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Axel Dörfler, axeld@pinc-software.de
 */


#include "accelerant.h"
#include "accelerant_protos.h"

#include <stdlib.h>


#define TRACE_OVERLAY
#ifdef TRACE_OVERLAY
extern "C" void _sPrintf(const char *format, ...);
#	define TRACE(x) _sPrintf x
#else
#	define TRACE(x) ;
#endif


static void
set_color_key(uint8 red, uint8 green, uint8 blue,
	uint8 redMask, uint8 greenMask, uint8 blueMask)
{
	overlay_registers *registers = gInfo->overlay_registers;

	registers->color_key_red = red;
	registers->color_key_green = green;
	registers->color_key_blue = blue;
	registers->color_key_mask_red = ~redMask;
	registers->color_key_mask_green = ~greenMask;
	registers->color_key_mask_blue = ~blueMask;
	registers->color_key_enabled = true;
}


static void
set_color_key(const overlay_window *window)
{
	switch (gInfo->shared_info->current_mode.space) {
		case B_CMAP8:
			set_color_key(0, 0, window->blue.value, 0x0, 0x0, 0xff);
			break;
		case B_RGB15:
			set_color_key(window->red.value << 3, window->green.value << 3,
				window->blue.value << 3, window->red.mask << 3, window->green.mask << 3,
				window->blue.mask << 3);
			break;
		case B_RGB16:
			set_color_key(window->red.value << 3, window->green.value << 2,
				window->blue.value << 3, window->red.mask << 3, window->green.mask << 2,
				window->blue.mask << 3);
			break;

		default:
			set_color_key(window->red.value, window->green.value,
				window->blue.value, window->red.mask, window->green.mask,
				window->blue.mask);
			break;
	}
}


static void
update_overlay(bool updateCoefficients)
{
	if (!gInfo->shared_info->overlay_active)
		return;

	ring_buffer &ringBuffer = gInfo->shared_info->primary_ring_buffer;
	write_to_ring(ringBuffer, COMMAND_FLUSH);
	write_to_ring(ringBuffer, COMMAND_NOOP);
	write_to_ring(ringBuffer, COMMAND_WAIT_FOR_EVENT | COMMAND_WAIT_FOR_OVERLAY_FLIP);
	write_to_ring(ringBuffer, COMMAND_NOOP);
	write_to_ring(ringBuffer, COMMAND_OVERLAY_FLIP | COMMAND_OVERLAY_CONTINUE);
	write_to_ring(ringBuffer, (uint32)gInfo->shared_info->physical_overlay_registers
		/*| (updateCoefficients ? OVERLAY_UPDATE_COEFFICIENTS : 0)*/);
	ring_command_complete(ringBuffer);

	TRACE(("update overlay: UPDATE: %lx, TEST: %lx, STATUS: %lx, EXTENDED_STATUS: %lx\n",
		read32(INTEL_OVERLAY_UPDATE), read32(INTEL_OVERLAY_TEST), read32(INTEL_OVERLAY_STATUS),
		read32(INTEL_OVERLAY_EXTENDED_STATUS)));
}


static void
show_overlay(void)
{
	if (gInfo->shared_info->overlay_active)
		return;

	overlay_registers *registers = gInfo->overlay_registers;

	gInfo->shared_info->overlay_active = true;
	registers->overlay_enabled = true;

	ring_buffer &ringBuffer = gInfo->shared_info->primary_ring_buffer;
	write_to_ring(ringBuffer, COMMAND_FLUSH);
	write_to_ring(ringBuffer, COMMAND_NOOP);
	write_to_ring(ringBuffer, COMMAND_OVERLAY_FLIP | COMMAND_OVERLAY_ON);
	write_to_ring(ringBuffer, (uint32)gInfo->shared_info->physical_overlay_registers
		/*| OVERLAY_UPDATE_COEFFICIENTS*/);
	ring_command_complete(ringBuffer);
}


static void
hide_overlay(void)
{
	if (!gInfo->shared_info->overlay_active)
		return;

	overlay_registers *registers = gInfo->overlay_registers;

	gInfo->shared_info->overlay_active = false;
	registers->overlay_enabled = false;

	ring_buffer &ringBuffer = gInfo->shared_info->primary_ring_buffer;

	// flush pending commands
	write_to_ring(ringBuffer, COMMAND_FLUSH);
	write_to_ring(ringBuffer, COMMAND_NOOP);
	write_to_ring(ringBuffer, COMMAND_WAIT_FOR_EVENT | COMMAND_WAIT_FOR_OVERLAY_FLIP);
	write_to_ring(ringBuffer, COMMAND_NOOP);

	// clear overlay enabled bit
	write_to_ring(ringBuffer, COMMAND_OVERLAY_FLIP | COMMAND_OVERLAY_CONTINUE);
	write_to_ring(ringBuffer, (uint32)gInfo->shared_info->physical_overlay_registers);
	write_to_ring(ringBuffer, COMMAND_WAIT_FOR_EVENT | COMMAND_WAIT_FOR_OVERLAY_FLIP);
	write_to_ring(ringBuffer, COMMAND_NOOP);

	// turn off overlay engine
	write_to_ring(ringBuffer, COMMAND_OVERLAY_FLIP | COMMAND_OVERLAY_OFF);
	write_to_ring(ringBuffer, (uint32)gInfo->shared_info->physical_overlay_registers);
	write_to_ring(ringBuffer, COMMAND_WAIT_FOR_EVENT | COMMAND_WAIT_FOR_OVERLAY_FLIP);
	write_to_ring(ringBuffer, COMMAND_NOOP);
	ring_command_complete(ringBuffer);

	gInfo->current_overlay = NULL;
}


//	#pragma mark -


uint32
intel_overlay_count(const display_mode *mode)
{
	// TODO: make this depending on the amount of RAM and the screen mode
	return 1;
}


const uint32 *
intel_overlay_supported_spaces(const display_mode *mode)
{
	static const uint32 kSupportedSpaces[] = {B_RGB15, B_RGB16, B_RGB32, B_YCbCr422, 0};

	return kSupportedSpaces;
}


uint32
intel_overlay_supported_features(uint32 colorSpace)
{
	return B_OVERLAY_COLOR_KEY
		| B_OVERLAY_HORIZONTAL_FILTERING
		| B_OVERLAY_VERTICAL_FILTERING
		| B_OVERLAY_HORIZONTAL_MIRRORING;
}


const overlay_buffer *
intel_allocate_overlay_buffer(color_space colorSpace, uint16 width,
	uint16 height)
{
	TRACE(("intel_allocate_overlay_buffer(width %u, height %u, colorSpace %lu)\n",
		width, height, colorSpace));

	uint32 bytesPerPixel;

	switch (colorSpace) {
		case B_RGB15:
			bytesPerPixel = 2;
			break;
		case B_RGB16:
			bytesPerPixel = 2; 
			break;
		case B_RGB32:
			bytesPerPixel = 4; 
			break;
		case B_YCbCr422:
			bytesPerPixel = 2;
			break;
		default:
			return NULL;
	}

	struct overlay *overlay = (struct overlay *)malloc(sizeof(struct overlay));
	if (overlay == NULL)
		return NULL;

	// TODO: locking!

	// alloc graphics mem
	overlay_buffer *buffer = &overlay->buffer;

	buffer->space = colorSpace;
	buffer->width = width;
	buffer->height = height;
	buffer->bytes_per_row = (width * bytesPerPixel + 0x3f) & ~0x3f;

	status_t status = intel_allocate_memory(buffer->bytes_per_row * height,
		overlay->buffer_handle, overlay->buffer_offset);
	if (status < B_OK) {
		free(overlay);
		return NULL;
	}

	buffer->buffer = gInfo->shared_info->graphics_memory + overlay->buffer_offset;
	buffer->buffer_dma = gInfo->shared_info->physical_graphics_memory + overlay->buffer_offset;

	TRACE(("allocated overlay buffer: handle=%x, offset=%x, address=%x, physical address=%x\n", 
		overlay->buffer_handle, overlay->buffer_offset, buffer->buffer, buffer->buffer_dma));

	return buffer;
}


status_t
intel_release_overlay_buffer(const overlay_buffer *buffer)
{
	TRACE(("intel_release_overlay_buffer(buffer %p)\n", buffer));

	struct overlay *overlay = (struct overlay *)buffer;

	// TODO: locking!

	if (gInfo->current_overlay == overlay)
		hide_overlay();

	intel_free_memory(overlay->buffer_handle);
	free(overlay);

	return B_OK;
}


status_t
intel_get_overlay_constraints(const display_mode *mode, const overlay_buffer *buffer,
	overlay_constraints *constraints)
{
	TRACE(("intel_get_overlay_constraints(buffer %p)\n", buffer));

	// taken from the Radeon driver...

	// scaler input restrictions
	// TODO: check all these values; most of them are probably too restrictive

	// position
	constraints->view.h_alignment = 0;
	constraints->view.v_alignment = 0;

	// alignment
	switch (buffer->space) {
		case B_RGB15:
			constraints->view.width_alignment = 7;
			break;
		case B_RGB16:
			constraints->view.width_alignment = 7;
			break;
		case B_RGB32:
			constraints->view.width_alignment = 3;
			break;
		case B_YCbCr422:
			constraints->view.width_alignment = 7;
			break;
		case B_YUV12:
			constraints->view.width_alignment = 7;
		default:
			return B_BAD_VALUE;
	}
	constraints->view.height_alignment = 0;

	// size
	constraints->view.width.min = 4;		// make 4-tap filter happy
	constraints->view.height.min = 4;
	constraints->view.width.max = buffer->width;
	constraints->view.height.max = buffer->height;

	// scaler output restrictions
	constraints->window.h_alignment = 0;
	constraints->window.v_alignment = 0;
	constraints->window.width_alignment = 0;
	constraints->window.height_alignment = 0;
	constraints->window.width.min = 2;
	constraints->window.width.max = mode->virtual_width;
	constraints->window.height.min = 2;
	constraints->window.height.max = mode->virtual_height;

	// TODO: the minimum values are not tested
	constraints->h_scale.min = 1.0f / (1 << 4);
	constraints->h_scale.max = buffer->width * 7;
	constraints->v_scale.min = 1.0f / (1 << 4);
	constraints->v_scale.max = buffer->height * 7;

	return B_OK;
}


overlay_token
intel_allocate_overlay(void)
{
	TRACE(("intel_allocate_overlay()\n"));

	// we only have a single overlay channel
	if (atomic_or(&gInfo->shared_info->overlay_channel_used, 1) != 0)
		return NULL;

	return (overlay_token)++gInfo->shared_info->overlay_token;
}


status_t
intel_release_overlay(overlay_token overlayToken)
{
	TRACE(("intel_allocate_overlay(token %ld)\n", (uint32)overlayToken));

	// we only have a single token, which simplifies this
	if (overlayToken != (overlay_token)gInfo->shared_info->overlay_token)
		return B_BAD_VALUE;

	atomic_and(&gInfo->shared_info->overlay_channel_used, 0);

	return B_OK;
}


status_t
intel_configure_overlay(overlay_token overlayToken, const overlay_buffer *buffer,
	const overlay_window *window, const overlay_view *view)
{
	TRACE(("intel_configure_overlay: buffer %p, window %p, view %p\n",
		buffer, window, view));

	if (overlayToken != (overlay_token)gInfo->shared_info->overlay_token)
		return B_BAD_VALUE;

	if (window == NULL && view == NULL) {
		hide_overlay();
		return B_OK;
	}

	struct overlay *overlay = (struct overlay *)buffer;
	overlay_registers *registers = gInfo->overlay_registers;
	bool updateCoefficients = false;

	if (!gInfo->shared_info->overlay_active
		|| memcmp(&gInfo->last_overlay_view, view, sizeof(overlay_view))
		|| memcmp(&gInfo->last_overlay_frame, window, sizeof(overlay_frame))) {
		// scaling has changed, program window and scaling factor

		// clip the window to on screen bounds
		// TODO: this is not yet complete or correct - especially if we start
		// to support moving the display!
		int32 left, top, right, bottom;
		left = window->h_start;
		right = window->h_start + window->width;
		top = window->v_start;
		bottom = window->v_start + window->height;
		if (left < 0)
			left = 0;
		if (top < 0)
			top = 0;
		if (right > gInfo->shared_info->current_mode.timing.h_display)
			right = gInfo->shared_info->current_mode.timing.h_display;
		if (bottom > gInfo->shared_info->current_mode.timing.v_display)
			bottom = gInfo->shared_info->current_mode.timing.v_display;

		registers->window_left = left;
		registers->window_top = top;
		registers->window_width = right - left;
		registers->window_height = bottom - top;

		// Note: in non-planar mode, you *must* not program the source width/height
		// UV registers - they must stay cleared, or the chip is doing strange stuff.
		// On the other hand, you have to program the UV scaling registers, or the
		// result will be wrong, too.
		registers->source_width_rgb = view->width;
		registers->source_height_rgb = view->height;
		registers->source_bytes_per_row_rgb = (((overlay->buffer_offset + (view->width << 1)
			+ 0x1f) >> 5) - (overlay->buffer_offset >> 5) - 1) << 2;

		uint32 horizontalScale = ((view->width - 1) << 12) / window->width;
		uint32 verticalScale = ((view->height - 1) << 12) / window->height;
		uint32 horizontalScaleUV = horizontalScale >> 1;
		uint32 verticalScaleUV = verticalScale >> 1;
		horizontalScale = horizontalScaleUV << 1;
		verticalScale = verticalScaleUV << 1;

		// horizontal scaling
		registers->scale_rgb.horizontal_downscale_factor = horizontalScale >> 12;
		registers->scale_rgb.horizontal_scale_fraction = horizontalScale & 0xfff;
		registers->scale_uv.horizontal_downscale_factor = horizontalScaleUV >> 12;
		registers->scale_uv.horizontal_scale_fraction = horizontalScaleUV & 0xfff;

		// vertical scaling
		registers->scale_rgb.vertical_scale_fraction = verticalScale & 0xfff;
		registers->scale_uv.vertical_scale_fraction = verticalScaleUV & 0xfff;
		registers->vertical_scale_rgb = verticalScale >> 12;
		registers->vertical_scale_uv = verticalScaleUV >> 12;

		TRACE(("scale: h = %ld.%ld, v = %ld.%ld\n", horizontalScale >> 12,
			horizontalScale & 0xfff, verticalScale >> 12, verticalScale & 0xfff));

		if (verticalScale != gInfo->last_vertical_overlay_scale
			|| horizontalScale != gInfo->last_horizontal_overlay_scale) {
			// TODO: recompute phase coefficients (take from X driver)
			updateCoefficients = true;

			gInfo->last_vertical_overlay_scale = verticalScale;
			gInfo->last_horizontal_overlay_scale = horizontalScale;
		}

		gInfo->last_overlay_view = *view;
		gInfo->last_overlay_frame = *(overlay_frame *)window;
	}

	registers->color_control_output_mode = true;
	registers->select_pipe = 0;

	// program buffer

	registers->buffer_rgb0 = overlay->buffer_offset;
	registers->stride_rgb = buffer->bytes_per_row;

	switch (buffer->space) {
		case B_RGB15:
			registers->source_format = OVERLAY_FORMAT_RGB15;
			break;
		case B_RGB16:
			registers->source_format = OVERLAY_FORMAT_RGB16;
			break;
		case B_RGB32:
			registers->source_format = OVERLAY_FORMAT_RGB32;
			break;
		case B_YCbCr422:
			registers->source_format = OVERLAY_FORMAT_YCbCr422;
			break;
	}

	registers->mirroring_mode = (window->flags & B_OVERLAY_HORIZONTAL_MIRRORING) != 0
		? OVERLAY_MIRROR_HORIZONTAL : OVERLAY_MIRROR_NORMAL;
	registers->ycbcr422_order = 0;

	if (!gInfo->shared_info->overlay_active) {
		// overlay is shown for the first time
		set_color_key(window);
		show_overlay();
	} else
		update_overlay(updateCoefficients);

	gInfo->current_overlay = overlay;
	return B_OK;
}

