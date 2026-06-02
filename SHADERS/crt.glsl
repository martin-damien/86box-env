/* From https://github.com/BulbulatorMacher/86Box-shader */

#pragma parameter CRT_ASPECT_RATIO "Crt aspect ratio" 1.33 .5 2.
#pragma parameter DISP_ASPECT_RATIO "Display aspect ratio" 1.33 .5 2.
#pragma parameter DISP_SIZE "Display size" 1. .5 1.
#pragma parameter CURVATURE "Curvature" 1. 0. 2.
#pragma parameter EDGE_DIST "Crt edge" 0. 0. .5
#pragma parameter CORNER_R "Corner radius" 0. 0. .5
#pragma parameter SCAN_I "Scanline intensity" .6 0. 1.
#pragma parameter SCAN_MAX "Maximum scanline resolution" 400. 0. 9999. 1.
#pragma parameter OVERSCAN "Overscan" 0. 0. 1. 1.
#pragma parameter OFF_R "Off color red" 0. 0. 255. 1.
#pragma parameter OFF_G "Off color green" 0. 0. 255. 1.
#pragma parameter OFF_B "Off color blue" 0. 0. 255. 1.
#pragma parameter MONO "Monochromatic crt" 0. 0. 1. 1.
#pragma parameter MONO_R "Mono color red" 45. 0. 255. 1.
#pragma parameter MONO_G "Mono color green" 255. 0. 255. 1.
#pragma parameter MONO_B "Mono color blue" 216. 0. 255. 1.
uniform float CRT_ASPECT_RATIO;
uniform float DISP_ASPECT_RATIO;
uniform float DISP_SIZE;
uniform float CURVATURE;
uniform float EDGE_DIST;
uniform float CORNER_R;
uniform float SCAN_I;
uniform float SCAN_MAX;
uniform float OVERSCAN;
uniform float OFF_R;
uniform float OFF_G;
uniform float OFF_B;
uniform float MONO;
uniform float MONO_R;
uniform float MONO_G;
uniform float MONO_B;

#if defined(VERTEX)

#if __VERSION__ >= 130
#define COMPAT_VARYING out
#define COMPAT_ATTRIBUTE in
#else
#define COMPAT_VARYING varying 
#define COMPAT_ATTRIBUTE attribute 
#endif

uniform mat4 MVPMatrix;
COMPAT_ATTRIBUTE vec4 VertexCoord;
COMPAT_ATTRIBUTE vec4 TexCoord;
COMPAT_VARYING vec4 TEX0;

void main()
{
    gl_Position = MVPMatrix * VertexCoord;
    TEX0.xy = TexCoord.xy;
}

#elif defined(FRAGMENT)

#if __VERSION__ >= 130
#define COMPAT_VARYING in
#define COMPAT_TEXTURE texture
out vec4 FragColor;
#else
#define COMPAT_VARYING varying
#define FragColor gl_FragColor
#define COMPAT_TEXTURE texture2D
#endif

#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#define COMPAT_PRECISION mediump
#else
#define COMPAT_PRECISION
#endif

uniform COMPAT_PRECISION vec2 InputSize;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 OutputSize;
uniform sampler2D Texture;
COMPAT_VARYING vec4 TEX0;

void main()
{
	float view_aspect_ratio = OutputSize.x / OutputSize.y;
	vec2 pos = (TEX0.xy * TextureSize / InputSize - vec2(.5, .5)) * vec2(view_aspect_ratio, 1.);

	vec2 src_pos = pos;
	float abs_brightness = 1.;
	float img_brightness = 1.;

	// crt curvature calculation
	bool has_curvature = CURVATURE > .005;
	if (has_curvature)
	{
		float curv_r = 1. / CURVATURE * max(CRT_ASPECT_RATIO, 1.);
		float max_h = sin(.5 / curv_r) * curv_r / .5;
		float max_w = sin((.5 * CRT_ASPECT_RATIO) / curv_r) * curv_r / (.5 * view_aspect_ratio);

		float dst_r = sqrt(pos.x * pos.x + pos.y * pos.y) * max(max_w, max_h);
		float dst_a = atan(pos.y, pos.x);
		float src_r = asin(dst_r / curv_r) * curv_r;
		float src_a = dst_a;
		float defl_a = src_r / curv_r;

		src_pos = vec2(cos(src_a), sin(src_a)) * src_r;
		abs_brightness *= cos(defl_a);
	}
	else if (CRT_ASPECT_RATIO > view_aspect_ratio)
	{
		src_pos *= CRT_ASPECT_RATIO / view_aspect_ratio;
	}

	// crt screen edge and corner radius calculation
	bool has_edge = EDGE_DIST > .005;
	bool has_corner = CORNER_R > .005;
	vec2 edge_pos = abs(src_pos) - vec2(.5 * CRT_ASPECT_RATIO, .5);
	float edge_dist = min(- edge_pos.x, - edge_pos.y);
	if (has_corner)
	{
		vec2 corner_pos = edge_pos + CORNER_R;
		if (corner_pos.x > 0 && corner_pos.y > 0)
		{
			vec2 squared = corner_pos * corner_pos;
			float corner_r = sqrt(squared.x + squared.y);
			edge_dist = min(edge_dist, CORNER_R - corner_r);
		}
	}
	if (edge_dist < 0)
		abs_brightness = 0.;
	else if (has_edge && edge_dist < EDGE_DIST)
	{
		float dist_norm = 1. - edge_dist / EDGE_DIST;
		abs_brightness *= 1. - dist_norm * dist_norm;
	}

	// overscan
	bool has_overscan = OVERSCAN > .5;
	vec2 eff_InputSize = InputSize;
	if (has_overscan)
	{
#define WIDTHS 6
#define HEIGHTS 11
		int widths[WIDTHS] = int[](160, 320, 640, 720, 800, 1024);
		int heights[HEIGHTS] = int[](100, 200, 344, 348, 350, 400, 480, 540, 560, 600, 768);
		int w = int(InputSize.x);
		int h = int(InputSize.y);

		for (int i = 1; i < WIDTHS; ++i)
		{
			if (w < widths[i])
			{
				eff_InputSize.x = float(widths[i - 1]);
				break;
			}
			if (i == WIDTHS - 1)
				eff_InputSize.x = widths[WIDTHS - 1];
		}
		for (int i = 1; i < HEIGHTS; ++i)
		{
			if (h < heights[i])
			{
				eff_InputSize.y = float(heights[i - 1]);
				break;
			}
			if (i == HEIGHTS - 1)
				eff_InputSize.x = heights[HEIGHTS - 1];
		}
#undef HEIGHTS
#undef WIDTHS

		src_pos *= eff_InputSize / InputSize;
	}

	// sizing display on the screen
	vec2 tex_pos = src_pos / vec2(DISP_ASPECT_RATIO, 1.);
	if (DISP_ASPECT_RATIO > CRT_ASPECT_RATIO)
		tex_pos *= DISP_ASPECT_RATIO / CRT_ASPECT_RATIO;
	tex_pos /= DISP_SIZE;
	tex_pos += .5;
	tex_pos *= InputSize / TextureSize;

	// scanlines
	bool has_scanlines = eff_InputSize.y < SCAN_MAX + .5 && SCAN_I > 0.005;
	if (has_scanlines)
	{
		float px_pos = tex_pos.y * TextureSize.y;
		float mult = SCAN_I * abs(sin(3.14 * px_pos));
		img_brightness *= mult + 1. - SCAN_I;
		tex_pos.y = (floor(px_pos) + .5) / TextureSize.y;
	}

	//overscan
	if (has_overscan)
	{
		vec2 prop = (1. - eff_InputSize / InputSize) / 4.;
		vec2 min_xy = prop * InputSize / TextureSize;
		vec2 max_xy = (1. - prop) * InputSize / TextureSize;
		if (int(InputSize.x - eff_InputSize.x) != 0)
		{
			tex_pos.x = max(tex_pos.x, min_xy.x);
			tex_pos.x = min(tex_pos.x, max_xy.x);
		}
		if (int(InputSize.y - eff_InputSize.y) != 0)
		{
			tex_pos.y = max(tex_pos.y, min_xy.y);
			tex_pos.y = min(tex_pos.y, max_xy.y);
		}
	}

	vec3 tex_col = img_brightness * COMPAT_TEXTURE(Texture, tex_pos).rgb;

	// monochromatic
	bool is_mono = MONO > .5;
	if (is_mono)
	{
		vec3 mono_col = vec3(MONO_R, MONO_G, MONO_B) / 255.;
		float l = dot(tex_col, vec3(.2, .7, .1));
		tex_col = l * mono_col;
	}

	// black level
	vec3 off_col = vec3(OFF_R, OFF_G, OFF_B) / 255.;
	float off_l = dot(off_col, vec3(.2, .7, .1));
	tex_col = off_col + (1. - off_l) * tex_col;

	FragColor = vec4(abs_brightness * tex_col.r, abs_brightness * tex_col.g, abs_brightness * tex_col.b, 1.);
}

#endif
