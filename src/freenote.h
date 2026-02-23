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

#define VERSION_MAJOR 0
#define VERSION_MINOR 0
#define VERSION_REVISION 0
#define VERSION_STRING "v0.0.0"

#define FN_NUM_SEGMENT_POINTS 16
#define FN_POINT_SAMPLE_TIME 0.01f
#define FN_PAGE_ARENA_SIZE (1024*1024)

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
	clib_arena *mem;

	v2 position;
	u64 page_number;

	fn_stroke *first_stroke;	
	fn_stroke *final_stroke;

	struct fn_page *prev;
	struct fn_page *next;
} fn_page;

typedef struct
{
	clib_arena *mem;

	fn_page *first_page;

	v2 viewport;
	f32 DPI;

	v2 page_size;
	f32 page_separation;
} fn_note;

typedef enum
{
	FN_MODE_MENU,
	FN_MODE_NOTE,
} fn_mode;

typedef enum
{
	FN_TOOL_PEN,
} fn_tool;

typedef struct fn_app_state
{
	clib_arena *mem;

	// App data
	fn_note *current_note;

	// Moving
	f32 move_speed;
	v2 old_viewport;
	v2 movement_anchor;	

	// Drawing
	fn_page *drawing_page;
	fn_stroke *drawing_stroke;
	fn_segment *drawing_segment;
	f32 last_point_time;

	fn_mode mode;
	fn_tool tool;

	// Platform data
	GLFWwindow *window;

	v2 mouse_screen;
	v2 mouse_canvas;

	i32 framebuffer_width;
	i32 framebuffer_height;
	v2  framebuffer_size;
	
	f32 time;

	// Graphics data
	GLuint stroke_buffer;
	GLuint square_buffer;

	struct {
		GLuint program;
		GLint transform;
		GLint colour;
		GLint scale;
		GLint translate;
	} canvas_shader;
} fn_app_state;

int main();

void fn_app_init(fn_app_state *app);

void fn_process_input(fn_app_state *app);
void fn_input_pen(fn_app_state *app, i32 is_pen_down);
void fn_input_move(fn_app_state *app, i32 is_move_down);

void fn_glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

void fn_note_init(fn_note *note);
void fn_note_destroy(fn_note *note);

void fn_note_draw(fn_app_state *app, fn_note *note);
void fn_note_write_file(fn_app_state *app, fn_note *note, const char *path);
void fn_note_read_file(fn_app_state *app, fn_note *note, const char *path);

void fn_note_print_info(fn_note *note);

void fn_page_init(fn_page *page);
void fn_page_destroy(fn_page *page);
fn_page *fn_page_at_point(fn_note *note, v2 point);
void fn_page_info_recalc(fn_note *note);

fn_stroke *fn_page_begin_stroke(fn_page *page);
fn_segment *fn_stroke_begin_segment(fn_page *page, fn_stroke *stroke);
void fn_segment_add_point(fn_segment *segment, fn_point point);

GLuint fn_shader_load( clib_arena *arena, const char *vertex_path, const char *fragment_path);

// Converts from point space to pixel space and vice versa
v2 fn_point_to_pixel(v2 point, v2 viewport, v2 framebuffer, float DPI);
v2 fn_pixel_to_point(v2 point, v2 viewport, v2 framebuffer, float DPI);



#endif // _FREENOTE_H_
