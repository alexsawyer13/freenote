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

#define FN_NUM_SEGMENT_POINTS 16
#define FN_POINT_SAMPLE_TIME 0.01f

#define V2_ZERO ((v2){0.0f, 0.0f})
#define V2_A4_SIZE ((v2){595.0f, 842.0f})

typedef struct
{
	f32 x;
	f32 y;
} v2;

typedef struct fn_point
{
	v2 pos;
	f32 t;
	f32 pressure;
} fn_point;

typedef struct fn_segment
{
	fn_point points[FN_NUM_SEGMENT_POINTS];
	u64 num_points;
	struct fn_segment *next;
} fn_segment;

typedef struct fn_stroke
{
	fn_segment first_segment;
	fn_segment *final_segment;
	v2 bounding_box_pos;
	v2 bounding_box_size;
	struct fn_stroke *next;
} fn_stroke;

typedef struct fn_page
{
	clib_arena mem;
	fn_stroke first_stroke;	
	fn_stroke *final_stroke;
	struct fn_page *prev;
	struct fn_page *next;
} fn_page;

typedef struct
{
	fn_page first_page;
	v2 page_size;
	v2 viewport;
	float DPI;
} fn_note;

typedef struct fn_app_state
{
	// App data
	fn_note current_note;

	fn_stroke *current_stroke;
	fn_segment *current_segment;
	f32 last_point_time;

	// Platform data
	GLFWwindow *window;

	v2 mouse_pos;
	v2 last_mouse_pos;

	i32 framebuffer_width;
	i32 framebuffer_height;
	v2  framebuffer_size;
	
	f32 time;

	// Graphics data
	GLuint stroke_buffer;
	GLuint square_buffer;

	GLuint canvas_shader;
	GLint canvas_transform_uniform;
	GLint canvas_colour_uniform;
	GLint canvas_scale_uniform;
} fn_app_state;

void fn_note_init(fn_note *note);

void fn_note_draw(
		fn_app_state *app,
		fn_note *note
);

fn_stroke *fn_page_begin_stroke(fn_page *page);
fn_segment *fn_stroke_begin_segment(fn_page *page, fn_stroke *stroke);

GLuint fn_shader_load(
		clib_arena *arena,
		const char *vertex_path,
		const char *fragment_path
);

// Converts from point space to pixel space and vice versa
v2 fn_point_to_pixel(v2 point, v2 viewport, v2 framebuffer, float DPI);
v2 fn_pixel_to_point(v2 point, v2 viewport, v2 framebuffer, float DPI);

#endif // _FREENOTE_H_
