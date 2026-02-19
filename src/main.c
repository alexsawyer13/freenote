#include "clib.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

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

	f64 width;
	f64 height;
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

	GLuint buffer;

	GLuint canvas_shader;
} fn_app_state;

void
fn_draw_canvas(
		fn_app_state *app,
		fn_canvas *canvas,
		float xpos,
		float ypos,
		float pixels_per_uni
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

	glGenBuffers(1, &app.buffer);

	fn_canvas canvas = {};
	fn_stroke stroke = {};
	stroke.points = malloc(5 * sizeof(fn_point));
	stroke.point_count = 5;
	stroke.points[0].x = 0.0f;
	stroke.points[0].y = 0.0f;
	stroke.points[1].x = 0.0f;
	stroke.points[1].y = 0.5f;
	stroke.points[2].x = 0.5f;
	stroke.points[2].y = 0.5f;
	stroke.points[3].x = 0.5f;
	stroke.points[3].y = 0.0f;
	stroke.points[4].x = 0.0f;
	stroke.points[4].y = 0.0f;
	canvas.strokes = &stroke;
	canvas.stroke_count = 1;

    while (!glfwWindowShouldClose(app.window))
    {
		i32 width, height;
		glfwGetWindowSize(app.window, &width, &height);

		glViewport(0, 0, width, height);
		glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

		if (glfwGetMouseButton(app.window, GLFW_MOUSE_BUTTON_1))
		{
			double x,y;
			glfwGetCursorPos(app.window, &x, &y);
			printf("%f,%f\n", x, y);
		}

		fn_draw_canvas(&app, &canvas, 0.0f, 0.0f, 0.0f);

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
		float xpos,
		float ypos,
		float pixels_per_unit
		)
{
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
