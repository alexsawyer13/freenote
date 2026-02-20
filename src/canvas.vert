#version 330 core

layout (location = 0) in vec2 a_point;

// (x, y) - framebuffer_centre_point is vector from framebuffer centre to point in point space
// (x, -y) flips the y axis for NDC axes
// divide by (framebuffer_width_points, framebuffer_height_points)/2 and subtract (1, 1) to get -1 to 1

// (framebuffer centre in point space.xy, framebuffer in point space.xy)
uniform vec4 u_transform; // (ax, ay, bx, by)
uniform vec2 u_scale;
uniform vec2 u_translate;

void main()
{
	float x = ((a_point.x * u_scale.x + u_translate.x) - u_transform.x) / (u_transform.z * 0.5);
	float y = (u_transform.y - (a_point.y * u_scale.y + u_translate.y)) / (u_transform.w * 0.5);
	gl_Position = vec4(x, y, 0.0, 1.0);
}
