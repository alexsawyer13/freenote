#ifndef _FREENOTE_H_
#define _FREENOTE_H_

#include "clib.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

/*
 * The units for sizes in canvas is POINTS
 * This is the default unit for a PDF file
 * 1 point = 1/72 inch
 * A4 = 595x842 points
*/

typedef struct v2
{
	float x;
	float y;
} v2;

typedef struct fn_point
{
	f32 x, y;
} fn_point;

typedef struct fn_stroke
{
	fn_point *points;
	u64 point_count;

	f32 bounding_box_x;
	f32 bounding_box_y;
	f32 bounding_box_width;
	f32 bounding_box_height;
} fn_stroke;

typedef struct fn_canvas
{
	fn_stroke *strokes;
	u64 stroke_count;

	f32 x, y;
	f32 width;
	f32 height;
} fn_canvas;

typedef enum fn_tool
{
	FN_TOOL_PEN,
	FN_TOOL_SELECT,
	FN_TOOL_LASSO,
} fn_tool;

typedef struct fn_input_settings
{
	fn_tool current_tool;
	u64 colour;
} fn_input_settings;

typedef struct fn_app_state
{
	// App data
	fn_canvas *current_canvas;

	// Platform data

	GLFWwindow *window;
	i32 fb_width;
	i32 fb_height;

	// Graphics data

	GLuint stroke_buffer;
	GLuint square_buffer;

	GLuint canvas_shader;
	GLint canvas_transform_uniform;
	GLint canvas_colour_uniform;
	GLint canvas_scale_uniform;
} fn_app_state;

void fn_draw_canvas(
		fn_app_state *app,
		fn_canvas *canvas,
		f32 viewport_x, // Top left of viewport in point space
		f32 viewport_y,
		f32 DPI
);

GLuint fn_shader_load(
		clib_arena *arena,
		const char *vertex_path,
		const char *fragment_path
);

// Converts from point space to pixel space and vice versa
v2 fn_point_to_pixel(v2 point, v2 viewport, v2 framebuffer, float DPI);
v2 fn_pixel_to_point(v2 point, v2 viewport, v2 framebuffer, float DPI);

#endif // _FREENOTE_H_
