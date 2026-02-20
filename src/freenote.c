#include "freenote.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

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
    app.window = glfwCreateWindow(630, 891, "Hello World", NULL, NULL);
	CLIB_ASSERT(app.window, "Failed to create window");
    glfwMakeContextCurrent(app.window);
	CLIB_ASSERT(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress), "Failed to load GLAD");

	app.canvas_shader = fn_shader_load(&arena, "src/canvas.vert", "src/canvas.frag");
	CLIB_ASSERT(app.canvas_shader, "Failed to load canvas shader");
	clib_arena_reset(&arena);

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
	
	fn_note_init(&app.current_note);

    while (!glfwWindowShouldClose(app.window))
    {
		// Update framebuffer size
		glfwGetFramebufferSize(app.window, &app.framebuffer_width, &app.framebuffer_height);
		app.framebuffer_size = (v2){(float)app.framebuffer_width, (float)app.framebuffer_height};
		
		// Update time
		app.time = (f32)glfwGetTime();

		// Track mouse position in window
		{
			double x,y;
			glfwGetCursorPos(app.window, &x, &y);
			app.last_mouse_pos.x = app.mouse_pos.x;
			app.last_mouse_pos.y = app.mouse_pos.y;
			app.mouse_pos.x = (f32)x;
			app.mouse_pos.y = (f32)y;
		}

		// Process input

		if (glfwGetMouseButton(app.window, GLFW_MOUSE_BUTTON_1))
		{
			v2 last_mouse = fn_pixel_to_point(app.mouse_pos, app.current_note.viewport, app.framebuffer_size, app.current_note.DPI);
			v2 mouse = fn_pixel_to_point(app.mouse_pos, app.current_note.viewport, app.framebuffer_size, app.current_note.DPI);

			if (app.time - app.last_point_time > FN_POINT_SAMPLE_TIME)
			{
				fn_page *page = &app.current_note.first_page;

				if (app.current_stroke == NULL)
				{
					app.current_stroke = fn_page_begin_stroke(page);
				}

				if (app.current_segment == NULL || (app.current_segment->num_points >= FN_NUM_SEGMENT_POINTS))
				{
					app.current_segment = fn_stroke_begin_segment(page, app.current_stroke);
				}

				app.current_segment->points[app.current_segment->num_points] = (fn_point) {
					.pos = mouse,
					.t = 0.0f,
					.pressure = 0.0f
				};
				app.current_segment->num_points++;
				app.last_point_time = app.time;
			}
		}
		else
		{
			app.current_stroke = NULL;
			app.current_segment = NULL;
		}


		// Prepare for rendering
		glViewport(0, 0, app.framebuffer_width, app.framebuffer_height);
		glClearColor(0.91f, 0.914f, 0.922f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

		fn_note_draw(&app, &app.current_note);

        glfwSwapBuffers(app.window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void fn_note_draw(
		fn_app_state *app,
		fn_note *note
)
{
	// Size of frambuffer in point size
	f32 framebuffer_width_points = app->framebuffer_width * 72.0f / note->DPI;
	f32 framebuffer_height_points = app->framebuffer_height * 72.0f / note->DPI;

	// Centre of the frambuffer in point space
	f32 framebuffer_centre_point_x = note->viewport.x + framebuffer_width_points * 0.5f;
	f32 framebuffer_centre_point_y = note->viewport.y + framebuffer_height_points * 0.5f;

	// Use canvas shader with the appropriate point->NDC transform
	glUseProgram(app->canvas_shader);
	glUniform4f(app->canvas_transform_uniform,
			framebuffer_centre_point_x,
			framebuffer_centre_point_y,
			framebuffer_width_points,
			framebuffer_height_points
	);

	fn_page *page = &note->first_page;
	while (page != NULL)
	{
		// Draw a white rectangle to represent the page
		glBindBuffer(GL_ARRAY_BUFFER, app->square_buffer);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);
		glEnableVertexAttribArray(0);
		glUniform4f(app->canvas_colour_uniform, 1.0f, 1.0f, 1.0f, 1.0f);
		glUniform2f(app->canvas_scale_uniform, note->page_size.x, note->page_size.y);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		// Render each stroke separately for now...

		fn_stroke *stroke = &page->first_stroke;
		while (stroke != NULL)
		{
			// Collect points from stroke segments into a buffer
			v2 points[1024];
			u64 num_points = 0;

			fn_segment *segment = &stroke->first_segment;
			while (segment != NULL)
			{
				CLIB_ASSERT(num_points + segment->num_points <= 1024, "Out of room in buffer!");
				for (u64 i = 0; i < segment->num_points; i++)
				{
					points[num_points] = segment->points[i].pos;
					num_points++;
				}

				segment = segment->next;
			}

			// Render the stroke
			
			glBindBuffer(GL_ARRAY_BUFFER, app->stroke_buffer);
			glBufferData(GL_ARRAY_BUFFER, num_points * sizeof(v2), points, GL_DYNAMIC_DRAW);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(v2), 0);
			glEnableVertexAttribArray(0);

			glUniform4f(app->canvas_colour_uniform, 1.0f, 0.0f, 0.0f, 1.0f);
			glUniform2f(app->canvas_scale_uniform, 1.0f, 1.0f);
			glLineWidth(2.0f);
			glDrawArrays(GL_LINE_STRIP, 0, num_points);
			
			stroke = stroke->next;
		}

		page = page->next;
	}
	
		
	// Draw the strokes
//	for (u64 i = 0; i < canvas->stroke_count; i++)
//	{
//		fn_stroke *stroke = &canvas->strokes[i];
//
//		glBindBuffer(GL_ARRAY_BUFFER, app->stroke_buffer);
//		glBufferData(GL_ARRAY_BUFFER, stroke->point_count * sizeof(fn_point), stroke->points, GL_DYNAMIC_DRAW);
//		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(fn_point), offsetof(fn_point, x));
//		glEnableVertexAttribArray(0);
//
//		glUniform4f(app->canvas_colour_uniform, 1.0f, 0.0f, 0.0f, 1.0f);
//		glUniform2f(app->canvas_scale_uniform, 1.0f, 1.0f);
//		glLineWidth(2.0f);
//		glDrawArrays(GL_LINE_STRIP, 0, stroke->point_count);
//	}

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

void fn_note_init(fn_note *note)
{
	note->viewport = (v2){-10.0f, -10.0f};
	note->page_size = V2_A4_SIZE;
	note->DPI = 100.0f;
	note->first_page = (fn_page){};
	note->first_page.final_stroke = &note->first_page.first_stroke;
	clib_arena_init(&note->first_page.mem, 1024*1024);

	fn_segment *segment = &note->first_page.first_stroke.first_segment;

	segment->points[0].pos.x = 0.0f;
	segment->points[0].pos.y = 0.0f;
	segment->points[1].pos.x = 72.0f;
	segment->points[1].pos.y = 0.0f;
	segment->points[2].pos.x = 72.0f;
	segment->points[2].pos.y = 72.0f;
	segment->points[3].pos.x = 0.0f;
	segment->points[3].pos.y = 72.0f;
	segment->points[4].pos.x = 0.0f;
	segment->points[4].pos.y = 0.0f;
	segment->num_points = 5;
	segment->next = NULL;
}

fn_stroke *fn_page_begin_stroke(fn_page *page)
{
	if (page->final_stroke == NULL)
	{
		page->final_stroke = &page->first_stroke;
		return page->final_stroke;
	}

	fn_stroke *stroke = clib_arena_alloc(&page->mem, sizeof(fn_stroke));
	page->final_stroke->next = stroke;
	page->final_stroke = stroke;

	return stroke;
}

fn_segment *fn_stroke_begin_segment(fn_page *page, fn_stroke *stroke)
{
	if (stroke->final_segment == NULL) {
		stroke->final_segment = &stroke->first_segment;
		return stroke->final_segment;
	}

	fn_segment *segment = clib_arena_alloc(&page->mem, sizeof(fn_segment));
	stroke->final_segment->next = segment;
	stroke->final_segment = segment;
	
	return segment;
}
