#include "graphics.hpp"

namespace gph
{

GWindow::GWindow(int width, int height, std::string title)
: WINDOW_WIDTH(width)
, WINDOW_HEIGHT(height)
, WINDOW_TITLE(title)
, program_start_clock(std::chrono::high_resolution_clock::now())
{
	WINDOW_PIXEL = 4 * WINDOW_HEIGHT * WINDOW_WIDTH;

	m_Display = XOpenDisplay(NIL);
	assert(m_Display);

	elapsed_time	= std::chrono::high_resolution_clock::now() - program_start_clock;
	double_timestep = (sin(elapsed_time.count()) + 1) / 2.0;

	screenbuffer = (unsigned char*) malloc(WINDOW_PIXEL);
	memset(screenbuffer, 255, 4 * WINDOW_HEIGHT * WINDOW_WIDTH);

	int blackColor = BlackPixel(m_Display, DefaultScreen(m_Display));
	int whiteColor = WhitePixel(m_Display, DefaultScreen(m_Display));

	m_Window = XCreateSimpleWindow(m_Display,
								   DefaultRootWindow(m_Display),
								   0,
								   0,
								   WINDOW_WIDTH,
								   WINDOW_HEIGHT,
								   0,
								   blackColor,
								   blackColor);

	XSelectInput(m_Display, m_Window, StructureNotifyMask | ExposureMask | KeyPressMask);

	XMapWindow(m_Display, m_Window);

	m_Graphics = XCreateGC(m_Display, m_Window, 0, NIL);
	m_Visual   = DefaultVisual(m_Display, DefaultScreen(m_Display));

	m_Image = create_ximage(m_Display, m_Visual, WINDOW_WIDTH, WINDOW_HEIGHT);
	memcpy(m_Image->data, screenbuffer, WINDOW_PIXEL);

	bool drawMode = false;

	Start();

	for (;;)
	{
		while (XPending(m_Display))
		{
			XNextEvent(m_Display, &m_Event);
		}
		if (m_Event.type == MapNotify)
		{
			drawMode = true;
		}
		if (drawMode)
		{
			if (m_Event.type == Expose)
			{
				memcpy(m_Image->data, screenbuffer, WINDOW_PIXEL);
				memset(screenbuffer, 255, WINDOW_PIXEL);

				Update();

				XPutImage(m_Display,
						  m_Window,
						  m_Graphics,
						  m_Image,
						  0,
						  0,
						  0,
						  0,
						  WINDOW_WIDTH,
						  WINDOW_HEIGHT);
			}
		}

		if (m_Event.type == KeyPress)
		{
			break;
		}

		elapsed_time	= std::chrono::high_resolution_clock::now() - program_start_clock;
		double_timestep = (sin(elapsed_time.count()) + 1) / 2.0;
		Tick();
	}

	XCloseDisplay(m_Display);
	XDestroyWindow(m_Display, m_Window);
	XDestroyImage(m_Image);
	free(screenbuffer);
};

GWindow::~GWindow() { Close(); }

XImage* GWindow::create_ximage(Display* display, Visual* visual, int width, int height)
{
	unsigned char* image32 = (unsigned char*) malloc(width * height * BYTES_PER_PIXEL);
	if (image32 == nullptr)
	{
		std::cout << "Image32 malloc fatal error!" << std::endl;
		exit(1);
	}
	set_screen(image32, width, height);
	return XCreateImage(display, visual, 24, ZPixmap, 0, (char*) image32, width, height, 32, 0);
}

void GWindow::set_screen(unsigned char* rgb_out, int w, int h)
{
	std::strncpy((char*) rgb_out, (const char*) screenbuffer, w * h * BYTES_PER_PIXEL);
	return;
}

int get_buffer_index(Vector2 pos, int WINDOW_WIDTH) { return (pos.y * WINDOW_WIDTH + pos.x) * 4; }

Vector2 get_buffer_pixel(int index, int WINDOW_WIDTH)
{
	int		pixelIndex = floor((float) index / 4);
	int		y		   = floor((float) pixelIndex / WINDOW_WIDTH);
	int		x		   = pixelIndex % WINDOW_WIDTH;
	Vector2 r(x, y);
	return r;
}

Color get_buffer_pixel_color(Vector2 pos, int WINDOW_WIDTH, unsigned char* screenbuffer)
{
	int	  index = get_buffer_index(pos, WINDOW_WIDTH);
	Color color(screenbuffer[index + 0],
				screenbuffer[index + 1],
				screenbuffer[index + 2],
				screenbuffer[index + 3]);

	return color;
}

Color lerpRGB(Color c1, Color c2, float time)
{
	Color r(lerp(c1.r, c2.r, time, false),
			lerp(c1.g, c2.g, time, false),
			lerp(c1.b, c2.b, time, false),
			lerp(c1.a, c2.a, time, false));
	return r;
}

float lerp(float a, float b, float time, bool looping)
{
	return (looping) ? ((a * (1.0 - fmod(time, 1.0f))) + (b * fmod(time, 1.0f)))
					 : (a * (1.0 - time)) + b * time;
}

float smoothstep(float t) { return t * t * (3 - t * 2); }

static int triangle_area(Vector2 t1, Vector2 t2, Vector2 t3)
{
	return abs((t1.x * (t2.y - t3.y) + t2.x * (t3.y - t1.y) + t3.x * (t1.y - t2.y)) / 2);
}

bool point_in_triangle(Vector2 aPoint, Vector2 t1, Vector2 t2, Vector2 t3)
{
	int a  = triangle_area(t1, t2, t3);
	int a1 = triangle_area(aPoint, t2, t3);
	int a2 = triangle_area(t1, aPoint, t3);
	int a3 = triangle_area(t1, t2, aPoint);
	return (a == a1 + a2 + a3);
}

void GWindow::fill_pixel(Vector2 aPos, Color aColor)
{
	if ((aPos.x < 0 || aPos.x > WINDOW_WIDTH) || (aPos.y < 0 || aPos.y > WINDOW_HEIGHT))
		return;
	int index				= get_buffer_index(aPos, WINDOW_WIDTH);
	screenbuffer[index + 0] = aColor.b;	 // B
	screenbuffer[index + 1] = aColor.g;	 // G
	screenbuffer[index + 2] = aColor.r;	 // R
	screenbuffer[index + 3] = aColor.a;	 // A
	return;
}

void GWindow::blend_pixel(Vector2 aPos, Color aColor)
{
	Color dst	  = get_buffer_pixel_color(aPos, WINDOW_WIDTH, screenbuffer);
	Color blended = lerpRGB(dst, aColor, (float) aColor.a / 255);
	fill_pixel(aPos, blended);
	return;
}

void GWindow::fill_rectangle(Vector2 aPos, int width, int height, const FillStyle& fillStyle)
{
	for (int y1 = aPos.y; y1 < aPos.y + height; ++y1)
	{
		for (int x1 = aPos.x; x1 < aPos.x + width; x1++)
		{
			Color color = fillStyle({ x1, y1 });
			blend_pixel({ x1, y1 }, color);
		}
	}
	return;
}

void GWindow::fill_line(Vector2 aPos1, Vector2 aPos2, const FillStyle& fillStyle)
{
	bool yLonger = false;
	int	 incrementVal, endVal;
	int	 shortLen = aPos2.y - aPos1.y;
	int	 longLen  = aPos2.x - aPos1.x;
	if (abs(shortLen) > abs(longLen))
	{
		int swap = shortLen;
		shortLen = longLen;
		longLen	 = swap;
		yLonger	 = true;
	}
	endVal = longLen;
	if (longLen < 0)
	{
		incrementVal = -1;
		longLen		 = -longLen;
	}
	else
		incrementVal = 1;
	int decInc;
	if (longLen == 0)
		decInc = 0;
	else
		decInc = (shortLen << 16) / longLen;
	int j = 0;
	if (yLonger)
	{
		for (int i = 0; i != endVal; i += incrementVal)
		{
			Color color = fillStyle({ aPos1.x + (j >> 16), aPos1.y + i });
			blend_pixel({ aPos1.x + (j >> 16), aPos1.y + i }, color);
			j += decInc;
		}
	}
	else
	{
		for (int i = 0; i != endVal; i += incrementVal)
		{
			Color color = fillStyle({ aPos1.x + i, aPos1.y + (j >> 16) });
			fill_pixel({ aPos1.x + i, aPos1.y + (j >> 16) }, color);

			j += decInc;
		}
	}
	return;
}

void GWindow::fill_circle(Vector2 center, int radius, const FillStyle& fillStyle)
{
	float x1 = float(center.x) - radius, y1 = float(center.y) - radius;
	float x2 = float(center.x) + radius, y2 = float(center.y) + radius;
	for (int y = y1; y < y2; ++y)
	{
		for (int x = x1; x < x2; ++x)
		{
			float distX = (x - center.x + 0.5), distY = (y - center.y + 0.5);
			float distance = sqrt(distX * distX + distY * distY);
			if (distance <= radius)
			{
				Color color = fillStyle({ x, y });
				blend_pixel({ x, y }, color);
			}
		}
	}
	return;
}

void GWindow::fill_triangle(Vector2 p1, Vector2 p2, Vector2 p3, const FillStyle& fillStyle)
{
	int maxX = std::max(p1.x, std::max(p2.x, p3.x));
	int minX = std::min(p1.x, std::min(p2.x, p3.x));
	int maxY = std::max(p1.y, std::max(p2.y, p3.y));
	int minY = std::min(p1.y, std::min(p2.y, p3.y));

	for (int x = minX; x <= maxX; ++x)
	{
		for (int y = minY; y <= maxY; ++y)
		{
			if (point_in_triangle({ x, y }, p1, p2, p3))
			{
				Color color = fillStyle({ x, y });
				blend_pixel({ x, y }, color);
			}
		}
	}
	return;
}

};
