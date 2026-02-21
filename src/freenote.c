#include "freenote.h"
#include "GLFW/glfw3.h"
#include "clib.h"

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
	fn_app_init(&app);

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
			app.mouse_pixels.x = (f32)x;
			app.mouse_pixels.y = (f32)y;
			app.mouse_points = fn_pixel_to_point(app.mouse_pixels, app.current_note.viewport, app.framebuffer_size, app.current_note.DPI);
		}

		fn_process_input(&app);

		// Prepare for rendering
		glViewport(0, 0, app.framebuffer_width, app.framebuffer_height);
		glClearColor(0.91f, 0.914f, 0.922f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

		fn_note_draw(&app, &app.current_note);

        glfwSwapBuffers(app.window);
        glfwPollEvents();
    }

	glfwDestroyWindow(app.window);
    glfwTerminate();
    return 0;
}

// TODO: Use depth testing...
void fn_note_draw(fn_app_state *app, fn_note *note)
{
	// Size of frambuffer in point size
	f32 framebuffer_width_points = app->framebuffer_width * 72.0f / note->DPI;
	f32 framebuffer_height_points = app->framebuffer_height * 72.0f / note->DPI;

	// Centre of the frambuffer in point space
	f32 framebuffer_centre_point_x = note->viewport.x + framebuffer_width_points * 0.5f;
	f32 framebuffer_centre_point_y = note->viewport.y + framebuffer_height_points * 0.5f;

	// Use canvas shader with the appropriate point->NDC transform
	glUseProgram(app->canvas_shader.program);
	glUniform4f(app->canvas_shader.transform,
			framebuffer_centre_point_x,
			framebuffer_centre_point_y,
			framebuffer_width_points,
			framebuffer_height_points
	);
	glUniform2f(app->canvas_shader.translate, 0.0f, 0.0f);

	fn_page *page = note->first_page;
	while (page != NULL)
	{
		// Draw a white rectangle to represent the page
		glBindBuffer(GL_ARRAY_BUFFER, app->square_buffer);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);
		glEnableVertexAttribArray(0);
		glUniform4f(app->canvas_shader.colour, 1.0f, 1.0f, 1.0f, 1.0f);
		glUniform2f(app->canvas_shader.scale, note->page_size.x, note->page_size.y);
		glUniform2f(app->canvas_shader.translate, page->position.x, page->position.y);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		// Render each stroke separately for now...

		fn_stroke *stroke = page->first_stroke;
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

			glUniform4f(app->canvas_shader.colour, 1.0f, 0.0f, 0.0f, 1.0f);
			glUniform2f(app->canvas_shader.scale, 1.0f, 1.0f);
			glUniform2f(app->canvas_shader.transform, page->position.x, page->position.y);
			glLineWidth(2.0f);
			glDrawArrays(GL_LINE_STRIP, 0, num_points);
			
			stroke = stroke->next;
		}

		page = page->next;
	}
}

GLuint fn_shader_load(clib_arena *arena, const char *vertex_path, const char *fragment_path)
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
	note->page_separation = 72.0f;
	note->mem = clib_arena_init(100*1024);

	note->first_page = clib_arena_alloc(note->mem, sizeof(fn_page));
	fn_page_init(note->first_page);

	note->first_page->next = clib_arena_alloc(note->mem, sizeof(fn_page));
	fn_page_init(note->first_page->next);

	fn_page_info_recalc(note);
}

fn_stroke *fn_page_begin_stroke(fn_page *page)
{
	if (page->first_stroke == NULL)
	{
		page->first_stroke = clib_arena_alloc(page->mem, sizeof(fn_stroke));
	}
	if (page->final_stroke == NULL)
	{
		page->final_stroke = page->first_stroke;
		return page->final_stroke;
	}

	fn_stroke *stroke = clib_arena_alloc(page->mem, sizeof(fn_stroke));
	page->final_stroke->next = stroke;
	page->final_stroke = stroke;

	return stroke;
}

fn_segment *fn_stroke_begin_segment(fn_page *page, fn_stroke *stroke)
{
	CLIB_ASSERT(page, "no page");
	CLIB_ASSERT(stroke, "no stroke");

	if (stroke->final_segment == NULL) {
		stroke->final_segment = &stroke->first_segment;
		return stroke->final_segment;
	}

	fn_segment *segment = clib_arena_alloc(page->mem, sizeof(fn_segment));
	stroke->final_segment->next = segment;
	stroke->final_segment = segment;
	
	return segment;
}

void fn_page_init(fn_page *page)
{
	*page = (fn_page){0};
	page->mem = clib_arena_init(FN_PAGE_ARENA_SIZE);
}

void fn_segment_add_point(fn_segment *segment, fn_point point)
{
	CLIB_ASSERT(segment->num_points < FN_NUM_SEGMENT_POINTS, "Segment full!");
	segment->points[segment->num_points] = point;
	segment->num_points++;
}

fn_page *fn_page_at_point(fn_note *note, v2 point)
{
	fn_page *page = note->first_page;
	v2 page_pos = (v2){0.0f, 0.0f};

	while (page != NULL)
	{
		// Work out the y bounds for the page

		// Just need to check it's not overlapping into the NEXT page
		// Pages are always in order, so if we are at a page, we've already checked previous pages
		// so it's definitely NOT in a previous page.
		// This has the added benefit of attaching anything at a negative coordinate to page 1
		// especially important for infinite page in the future I think...
		if (point.y < (page_pos.y + note->page_size.y + note->page_separation)) return page;
		if (page->next == NULL) return page;


		page = page->next;
		page_pos.y += note->page_size.y + note->page_separation;
	}
	
	// If we didn't find the page, then it must be further down than the last page so attach it to that??
	// TODO: This might need some rework when adding an additional page at the end...
	return page;
}

void fn_page_info_recalc(fn_note *note)
{
	fn_page *page = note->first_page;
	u64 current_page = 0;
	v2 page_pos = V2_ZERO;

	while (page != NULL)
	{
		page->page_number = current_page;
		page->position = page_pos;

		current_page++;
		page = page->next;
		page_pos.y += note->page_size.y + note->page_separation;
	}
}

void fn_process_input(fn_app_state *app)
{
	if (app->tool == FN_TOOL_PEN)
		fn_input_drawing(app);
	else
		app->drawing_page = NULL;
}

void fn_input_drawing(fn_app_state *app)
{
	// If not holding left click, then we are no longer drawing

	if (glfwGetMouseButton(app->window, GLFW_MOUSE_BUTTON_1) != GLFW_PRESS)
	{
		app->drawing_page = NULL;
		app->drawing_stroke = NULL;
		app->drawing_segment = NULL;
		return;
	}

	// If we're not currently drawing and we're within a page, start drawing to it!
	if (app->drawing_page == NULL)
	{
		fn_page *page = fn_page_at_point(&app->current_note, app->mouse_points);

		v2 point_from_page = (v2){
			app->mouse_points.x - page->position.x,
			app->mouse_points.y - page->position.y,
		};

		if ((point_from_page.x > 0.0f && point_from_page.x < app->current_note.page_size.x) && 
				(point_from_page.y > 0.0f && point_from_page.y < app->current_note.page_size.y))
		{
			// Create a stroke and segment to start drawing to
			app->drawing_page = page;
			app->drawing_stroke = fn_page_begin_stroke(app->drawing_page);
			app->drawing_segment = fn_stroke_begin_segment(app->drawing_page, app->drawing_stroke);
		}
	}

	// If we are now actually drawing, and the polling rate hasn't been exceeded, then draw!
	if (app->drawing_page && (app->time - app->last_point_time > FN_POINT_SAMPLE_TIME))
	{
		CLIB_ASSERT(app->drawing_stroke, "No stroke");
		CLIB_ASSERT(app->drawing_segment, "No segment");

		// Calculated mouse position from current page origin
		v2 point_from_page  = (v2){
			app->mouse_points.x - app->drawing_page->position.x,
			app->mouse_points.y - app->drawing_page->position.y,
		};

		// Allocate a new segment if needed
		if (app->drawing_segment->num_points >= FN_NUM_SEGMENT_POINTS)
			app->drawing_segment = fn_stroke_begin_segment(app->drawing_page, app->drawing_stroke);

		// Clamp point to page
		// Drawing can only begin in page, but can go back out, I don't really want this...

		if (point_from_page.x < 0.0f) point_from_page.x = 0.0f;
		if (point_from_page.y < 0.0f) point_from_page.y = 0.0f;
		if (point_from_page.x > app->current_note.page_size.x) point_from_page.x = app->current_note.page_size.x;
		if (point_from_page.y > app->current_note.page_size.y) point_from_page.y = app->current_note.page_size.y;

		// Add point to segment
		fn_segment_add_point(app->drawing_segment, (fn_point) {
				.pos = point_from_page,
				.t = 0.0f,
				.pressure = 0.0f
		});

		// Track time so we can stick to polling rate
		app->last_point_time = app->time;
	}
}

void fn_app_init(fn_app_state *app)
{
	*app = (fn_app_state){0};

	app->mode = FN_MODE_NOTE;
	app->tool = FN_TOOL_PEN;

	clib_arena *startup_arena;
	startup_arena = clib_arena_init(1024*1024);

	CLIB_ASSERT(glfwInit(), "Failed to initialise GLFW");
	app->window = glfwCreateWindow(630, 891, "Hello World", NULL, NULL);
	CLIB_ASSERT(app->window, "Failed to create window");
	glfwMakeContextCurrent(app->window);
	CLIB_ASSERT(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress), "Failed to load GLAD");

	glfwSetWindowUserPointer(app->window, app);
	glfwSetKeyCallback(app->window, fn_glfw_key_callback);

	app->canvas_shader.program = fn_shader_load(startup_arena, "src/canvas.vert", "src/canvas.frag");
	CLIB_ASSERT(app->canvas_shader.program, "Failed to load canvas shader");
	clib_arena_reset(startup_arena);

	app->canvas_shader.transform = glGetUniformLocation(app->canvas_shader.program, "u_transform");
	CLIB_ASSERT(app->canvas_shader.transform != -1, "Failed to get uniform location");
	app->canvas_shader.colour = glGetUniformLocation(app->canvas_shader.program, "u_colour");
	CLIB_ASSERT(app->canvas_shader.colour != -1, "Failed to get uniform location");
	app->canvas_shader.scale = glGetUniformLocation(app->canvas_shader.program, "u_scale");
	CLIB_ASSERT(app->canvas_shader.scale != -1, "Failed to get uniform location");
	app->canvas_shader.translate = glGetUniformLocation(app->canvas_shader.program, "u_translate");
	CLIB_ASSERT(app->canvas_shader.translate != -1, "Failed to get uniform location");

	glGenBuffers(1, &app->stroke_buffer);
	glGenBuffers(1, &app->square_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, app->square_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(square_vertices), square_vertices, GL_STATIC_DRAW);

	fn_note_init(&app->current_note);

	clib_arena_destroy(&startup_arena);
}

void fn_note_destroy(fn_note *note)
{
	fn_page *page = note->first_page;
	while (page != NULL)
	{
		fn_page *next_page = page->next;
		fn_page_destroy(page);
		page = next_page;
	}
	clib_arena_destroy(&note->mem);
	*note = (fn_note){0};
}

void fn_page_destroy(fn_page *page)
{
	clib_arena_destroy(&page->mem);
	*page = (fn_page){0};
}

void fn_note_print_info(fn_note *note)
{
	u64 total_page_data = 0;

	clib_arena_print_info(note->mem);

	fn_page *page = note->first_page;
	while (page != NULL)
	{
		printf("Page %llu\n", page->page_number);
		clib_arena_print_info(page->mem);
		total_page_data += page->mem->total_allocation_size;

		page = page->next;
	}
	printf("Total page data: %llu bytes\n", total_page_data);
	printf("Total note data: %llu bytes\n", total_page_data + note->mem->total_allocation_size);

}

void fn_glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	fn_app_state *app = (fn_app_state*)glfwGetWindowUserPointer(window);
	CLIB_ASSERT(app, "app is NULL");
	
	if (key == GLFW_KEY_M && action == GLFW_PRESS)
	{
		fn_note_print_info(&app->current_note);
	}
}
