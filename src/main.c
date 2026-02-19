#include "clib.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/*
 * The units for sizes in canvas is POINTS
 * This is the default unit for a PDF file
 * 1 point = 1/72 inch
 * A4 = 595x842 points
*/

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
	GLFWwindow *window;
	i32 fb_width;
	i32 fb_height;

	GLuint buffer;

	GLuint canvas_shader;
	GLint canvas_transform_uniform_location;
} fn_app_state;

void
fn_draw_canvas(
		fn_app_state *app,
		fn_canvas *canvas,
		f32 viewport_x, // Top left of viewport in point space
		f32 viewport_y,
		f32 DPI
);

GLuint
fn_shader_load(
		clib_arena *arena,
		const char *vertex_path,
		const char *fragment_path
);

int main()
{
	fn_app_state app = {};
	clib_arena arena = {};

	clib_arena_init(&arena, 64*1024*1024);

	CLIB_ASSERT(glfwInit(), "Failed to initialise GLFW");
    app.window = glfwCreateWindow(1000, 800, "Hello World", NULL, NULL);
	CLIB_ASSERT(app.window, "Failed to create window");
    glfwMakeContextCurrent(app.window);
	CLIB_ASSERT(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress), "Failed to load GLAD");

	app.canvas_shader = fn_shader_load(&arena, "src/canvas.vert", "src/canvas.frag");
	CLIB_ASSERT(app.canvas_shader, "Failed to load canvas shader");

	app.canvas_transform_uniform_location = glGetUniformLocation(app.canvas_shader, "u_transform");
	CLIB_ASSERT(app.canvas_transform_uniform_location != -1, "Failed to get uniform location");

	glGenBuffers(1, &app.buffer);

	fn_canvas canvas = {};
	canvas.width = 595.0f;
	canvas.height = 842.0f;

	fn_stroke stroke = {};
	stroke.points = malloc(5 * sizeof(fn_point));
	stroke.point_count = 5;
	stroke.points[0].x = 0.0f;
	stroke.points[0].y = 0.0f;
	stroke.points[1].x = 0.0f;
	stroke.points[1].y = 72.0f;
	stroke.points[2].x = 72.0f;
	stroke.points[2].y = 72.0f;
	stroke.points[3].x = 72.0f;
	stroke.points[3].y = 0.0f;
	stroke.points[4].x = 0.0f;
	stroke.points[4].y = 0.0f;
	canvas.strokes = &stroke;
	canvas.stroke_count = 1;

    while (!glfwWindowShouldClose(app.window))
    {
		glfwGetFramebufferSize(app.window, &app.fb_width, &app.fb_height);
		printf("(%d, %d)\n", app.fb_width, app.fb_height);

		glViewport(0, 0, app.fb_width, app.fb_height);
		glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

		if (glfwGetMouseButton(app.window, GLFW_MOUSE_BUTTON_1))
		{
			double x,y;
			glfwGetCursorPos(app.window, &x, &y);
			printf("%f,%f\n", x, y);
		}

		fn_draw_canvas(&app, &canvas, -10.0f, -10.0f, 400.0f);

        glfwSwapBuffers(app.window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void
fn_draw_canvas(
		fn_app_state *app,
		fn_canvas *canvas,
		f32 viewport_x, // Top left of viewport in point space
		f32 viewport_y,
		f32 DPI
		)
{
	// Size of canvas in pixel space
	// f32 width_pixels = canvas->width / 72.0f * DPI;
	// f32 height_pixels = canvas->height / 72.0f * DPI;

	// Size of frambuffer in point size
	f32 framebuffer_width_points = app->fb_width * 72.0f / DPI;
	f32 framebuffer_height_points = app->fb_height * 72.0f / DPI;

	// Centre of the frambuffer in point space
	f32 framebuffer_centre_point_x = viewport_x + framebuffer_width_points * 0.5f;
	f32 framebuffer_centre_point_y = viewport_y + framebuffer_height_points * 0.5f;

	// (x, y) - framebuffer_centre_point is vector from framebuffer centre to point in point space
	// (x, -y) flips the y axis for NDC axes
	// divide by (framebuffer_width_points, framebuffer_height_points)/2 and subtract (1, 1) to get -1 to 1

	for (u64 i = 0; i < canvas->stroke_count; i++)
	{
		fn_stroke *stroke = &canvas->strokes[i];
		for (u64 j = 0; j < stroke->point_count; j++)
		{
			fn_point *point = &stroke->points[j];	
			printf("(%f, %f)\n", point->x, point->y);
		}
		printf("\n\n\n");

		glBindBuffer(GL_ARRAY_BUFFER, app->buffer);
		glBufferData(GL_ARRAY_BUFFER, canvas->strokes[i].point_count * sizeof(fn_point), canvas->strokes[i].points, GL_DYNAMIC_DRAW);

		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(fn_point), offsetof(fn_point, x));
		glEnableVertexAttribArray(0);

		glUseProgram(app->canvas_shader);
		glUniform4f(app->canvas_transform_uniform_location,
				framebuffer_centre_point_x,
				framebuffer_centre_point_y,
				framebuffer_width_points,
				framebuffer_height_points
		);

		glDrawArrays(GL_LINE_STRIP, 0, stroke->point_count);
	}

}

GLuint
fn_shader_load(
		clib_arena *arena,
		const char *vertex_path,
		const char *fragment_path
		)
{
	GLuint vert, frag, prog;
	vert = 0;
	frag = 0;
	prog = 0;

	char *data;
	u64 size;

	i32 success;

	if (!clib_file_read(arena, vertex_path, &data, &size)) return 0;
	vert = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vert, 1, (const char * const*)&data, NULL);
	glCompileShader(vert);

	glGetShaderiv(vert, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		GLint info_log_length;
		char *info_log;
		glGetShaderiv(vert, GL_INFO_LOG_LENGTH, &info_log_length);
		clib_arena_alloc(arena, info_log_length);
		glGetShaderInfoLog(vert, info_log_length, NULL, info_log);
		printf("Failed to compile vertex shader: %s\n", vertex_path);
		printf("%s\n", info_log);

		glDeleteShader(vert);
		return 0;
	}

	if (!clib_file_read(arena, fragment_path, &data, &size)) return 0;
	frag = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(frag, 1, (const char * const*)&data, NULL);
	glCompileShader(frag);

	glGetShaderiv(frag, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		GLint info_log_length;
		char *info_log;
		glGetShaderiv(frag, GL_INFO_LOG_LENGTH, &info_log_length);
		clib_arena_alloc(arena, info_log_length);
		glGetShaderInfoLog(frag, info_log_length, NULL, info_log);
		printf("Failed to compile fragment shader: %s\n", fragment_path);
		printf("%s\n", info_log);

		glDeleteShader(vert);
		glDeleteShader(frag);
		return 0;
	}

	prog = glCreateProgram();
	glAttachShader(prog, vert);
	glAttachShader(prog, frag);
	glLinkProgram(prog);

	glGetProgramiv(prog, GL_LINK_STATUS, &success);
	if (!success)
	{
		GLint info_log_length;
		char *info_log;
		glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &info_log_length);
		clib_arena_alloc(arena, info_log_length);
		glGetProgramInfoLog(prog, info_log_length, NULL, info_log);
		printf("Failed to link program: (%s, %s)\n", vertex_path, fragment_path);
		printf("%s\n", info_log);

		if (vert) glDeleteShader(vert);
		if (frag) glDeleteShader(frag);
		if (prog) glDeleteProgram(prog);
		return 0;
	}

	glDeleteShader(vert);
	glDeleteShader(frag);

	return prog;
}
