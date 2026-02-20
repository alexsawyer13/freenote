#include "freenote.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

static float square_vertices[] = {
	0.0f, 0.0f,
	1.0f, 0.0f,
	1.0f, 1.0f,
	0.0f, 0.0f,
	1.0f, 1.0f,
	0.0f, 1.0f,
};

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

	app.canvas_transform_uniform = glGetUniformLocation(app.canvas_shader, "u_transform");
	CLIB_ASSERT(app.canvas_transform_uniform != -1, "Failed to get uniform location");
	app.canvas_colour_uniform = glGetUniformLocation(app.canvas_shader, "u_colour");
	CLIB_ASSERT(app.canvas_colour_uniform != -1, "Failed to get uniform location");
	app.canvas_scale_uniform = glGetUniformLocation(app.canvas_shader, "u_scale");
	CLIB_ASSERT(app.canvas_scale_uniform != -1, "Failed to get uniform location");

	glGenBuffers(1, &app.stroke_buffer);
	glGenBuffers(1, &app.square_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, app.square_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(square_vertices), square_vertices, GL_STATIC_DRAW);

	fn_canvas canvas = {};
	canvas.width = 595.0f;
	canvas.height = 842.0f;

	fn_stroke stroke = {};
	
	fn_point points[5];
	points[0].x = 0.0f;
	points[0].y = 0.0f;
	points[1].x = 72.0f;
	points[1].y = 0.0f;
	points[2].x = 72.0f;
	points[2].y = 72.0f;
	points[3].x = 0.0f;
	points[3].y = 72.0f;
	points[4].x = 0.0f;
	points[4].y = 0.0f;

	stroke.points = points;
	stroke.point_count = 5;

	canvas.strokes = &stroke;
	canvas.stroke_count = 1;

	v2 viewport = {-10.0f, -10.0f};
	float DPI = 400.0f;

	float last_mouse_x, last_mouse_y;
	float mouse_x, mouse_y;
	mouse_x = mouse_y = last_mouse_x = last_mouse_y = 0.0f;

    while (!glfwWindowShouldClose(app.window))
    {
		glfwGetFramebufferSize(app.window, &app.fb_width, &app.fb_height);

		glViewport(0, 0, app.fb_width, app.fb_height);
		glClearColor(0.91f, 0.914f, 0.922f, 1.0f);
		//glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

		double x,y;
		glfwGetCursorPos(app.window, &x, &y);
		last_mouse_x = mouse_x;
		last_mouse_y = mouse_y;
		mouse_x = x;
		mouse_y = y;

		if (glfwGetMouseButton(app.window, GLFW_MOUSE_BUTTON_1))
		{
			v2 mouse = fn_pixel_to_point((v2){mouse_x, mouse_y}, viewport, (v2){(float)(app.fb_width), (float)(app.fb_height)}, DPI);
			printf("(%f, %f)\n", mouse.x, mouse.y);
		}

		fn_draw_canvas(&app, &canvas, viewport.x, viewport.y, DPI);

        glfwSwapBuffers(app.window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void fn_draw_canvas(
		fn_app_state *app,
		fn_canvas *canvas,
		f32 viewport_x, // Top left of viewport in point space
		f32 viewport_y,
		f32 DPI
		)
{
	// Size of frambuffer in point size
	f32 framebuffer_width_points = app->fb_width * 72.0f / DPI;
	f32 framebuffer_height_points = app->fb_height * 72.0f / DPI;

	// Centre of the frambuffer in point space
	f32 framebuffer_centre_point_x = viewport_x + framebuffer_width_points * 0.5f;
	f32 framebuffer_centre_point_y = viewport_y + framebuffer_height_points * 0.5f;

	// Use canvas shader with the appropriate point->NDC transform
	glUseProgram(app->canvas_shader);
	glUniform4f(app->canvas_transform_uniform,
			framebuffer_centre_point_x,
			framebuffer_centre_point_y,
			framebuffer_width_points,
			framebuffer_height_points
	);
	
	// Draw a white rectangle to denote the canvas
	glBindBuffer(GL_ARRAY_BUFFER, app->square_buffer);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);
	glEnableVertexAttribArray(0);
	glUniform4f(app->canvas_colour_uniform, 1.0f, 1.0f, 1.0f, 1.0f);
	glUniform2f(app->canvas_scale_uniform, canvas->width, canvas->height);
	glDrawArrays(GL_TRIANGLES, 0, 6);
		
	// Draw the strokes
	for (u64 i = 0; i < canvas->stroke_count; i++)
	{
		fn_stroke *stroke = &canvas->strokes[i];

		glBindBuffer(GL_ARRAY_BUFFER, app->stroke_buffer);
		glBufferData(GL_ARRAY_BUFFER, stroke->point_count * sizeof(fn_point), stroke->points, GL_DYNAMIC_DRAW);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(fn_point), offsetof(fn_point, x));
		glEnableVertexAttribArray(0);

		glUniform4f(app->canvas_colour_uniform, 1.0f, 0.0f, 0.0f, 1.0f);
		glUniform2f(app->canvas_scale_uniform, 1.0f, 1.0f);
		glLineWidth(2.0f);
		glDrawArrays(GL_LINE_STRIP, 0, stroke->point_count);
	}

}

GLuint fn_shader_load(
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

// (framebuffer centre in point space.xy, framebuffer in point space.xy)
//	float x = (a_point.x*u_scale.x - u_transform.x) / (u_transform.z * 0.5);
//	float y = (u_transform.y - a_point.y*u_scale.y) / (u_transform.w * 0.5);

v2 fn_point_to_pixel(v2 point, v2 viewport, v2 framebuffer, float DPI)
{
	v2 points_from_viewport = {
		point.x - viewport.x,
		point.y - viewport.y
	};
	v2 pixels_from_viewport = {
		points_from_viewport.x / 72.0f * DPI,
		points_from_viewport.y / 72.0f * DPI,
	};
	return pixels_from_viewport;
}

v2 fn_pixel_to_point(v2 point, v2 viewport, v2 framebuffer, float DPI)
{
	 v2 points_from_viewport = {
		 point.x / DPI * 72.0f,
		 point.y / DPI * 72.0f
	 };
	 v2 points_from_origin = {
		 points_from_viewport.x + viewport.x,
		 points_from_viewport.y + viewport.y,
	 };
	 return points_from_origin;
}
