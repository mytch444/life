#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#define LOCK_DELAY 100

void setupwindow();
void update();
void updateloop();
void setcolor(int color);
void render();

void lock();
void unlock();

int neighbours(int i);
int reproduce(int i);

void addblock(int i, int w);
void addrandom(int n);

void key(XEvent *event);
void buttonpress(XEvent *event);
void buttonrelease(XEvent *event);
void motionnotify(XEvent *event);
void configurenotify(XEvent *event);

int quit, paused;

int locked;

int delay;
int width, height;
int *points, *buffer;

int buttonpressed;

int boxwidth, boxheight;

Display *display;
Window root, win;
GC gc;
XIC xic;

void (*handlers[LASTEvent])(XEvent *e) = {
	[KeyPress] = key,
	[ButtonPress] = buttonpress,
	[ButtonRelease] = buttonrelease,
	[ConfigureNotify] = configurenotify,
	[MotionNotify] = motionnotify,
};

void lock() {
	while (locked) 
		usleep(LOCK_DELAY);
	locked = 1;
}

void unlock() {
	locked = 0;
}

void addrandom(int n) {
	int r;
	while (--n >= 0) {
		r = rand() % (width * height);	
		points[r] = 1000;
	}
}

void addblock(int i, int w) {
	int xx, yy;
	lock();

	for (xx = -w / 2; xx < w / 2; xx++) {
		for (yy = -w / 2; yy < w / 2; yy++) {
			if (i % width + xx < 0 || i % width + xx >= width
					|| i / width + yy < 0 || i / width + yy >= height)
				continue;
			buffer[i + xx + yy * width] = points[i + xx + yy * width] 
				= 1000;
		}
	}

	unlock();
}

int neighbours(int i) {
	int x, y, n = 0;
	for (x = -1; x <= 1; x++)
		for (y = -1; y <= 1; y += 1)
			if (!(x == 0 && y == 0) 
					&& !(i % width + x < 0 || i % width + x >= width)
					&& !(i / width + y < 0 || i / width + y >= height)
					&& points[i + x + y * width])
				n++;
	return n;
}

int reproduce(int i) {
	return 1000;
}

void update() {
	int i, n;
	for (i = 0; i < width * height; i++) {
		n = neighbours(i);
		if (points[i] && (n < 2 || n > 3))
			buffer[i] = 0;
		else if (!points[i] && n == 3)
			buffer[i] = reproduce(i); 
	}

	points = memcpy(points, buffer, sizeof(int) * width * height);
}

void updateloop() {
	while (!quit) {
		usleep(delay);
		if (paused) continue;

		lock();
		update();
		unlock();
	}
}

void setcolor(int color) {
	XGCValues values;
	values.foreground = color;
	XChangeGC(display, gc, GCForeground, &values);
}

void render() {
	int x, y;
	lock();
	for (x = 0; x < width; x++) {
		for (y = 0; y < height; y++) {
			setcolor(points[x + y * width]);
			XFillRectangle(display, win, gc, x * boxwidth, y * boxheight,
					boxwidth, boxheight);
		}
	}
	unlock();
}

void key(XEvent *event) {
	XKeyEvent ev = event->xkey;
	char buf[32];
	int len;
	Status status;
	KeySym keysym = NoSymbol;

	len = XmbLookupString(xic, &ev, buf, sizeof(buf), &keysym, &status);
	if (status == XBufferOverflow)
		return;

	switch (keysym) {
		case XK_p:
			paused = !paused;
			break;
		case XK_q:
			quit = 1;
			break;
	}
}

void buttonpress(XEvent *event) {
	XButtonEvent ev = event->xbutton;
	if (ev.button == Button1)
		buttonpressed = 1;
	else if (ev.button == Button3)
		buttonpressed = 10;

	if (buttonpressed)
		addblock(ev.x / boxwidth + ev.y / boxheight * width, buttonpressed);
}

void buttonrelease(XEvent *event) {
	buttonpressed = 0;
}

void motionnotify(XEvent *event) {
	XMotionEvent ev = event->xmotion;

	if (!buttonpressed) return;
	addblock(ev.x / boxwidth + ev.y / boxheight * width, buttonpressed);
}

void configurenotify(XEvent *event) {
	XConfigureEvent ev = event->xconfigure;
	printf("Resize\n");

	boxwidth = ev.width / width;
	boxheight = ev.height / height;
}

void setupwindow() {
	printf("Setting up window\n");

	XSetWindowAttributes attributes;

	display = XOpenDisplay(NULL);
	root = RootWindow(display, 0);

	attributes.event_mask = KeyPressMask | ButtonPressMask | ButtonReleaseMask 
		| PointerMotionMask | ExposureMask | StructureNotifyMask;

	printf("Creating window\n");
	win = XCreateWindow(display, root, 0, 0, 400, 400, 1,
			DefaultDepth(display, 0), CopyFromParent, CopyFromParent,
			CWEventMask,
			&attributes);

	xic = XCreateIC(XOpenIM(display, NULL, NULL, NULL), XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
			XNClientWindow, win, XNFocusWindow, win, NULL);

	gc = XCreateGC(display, win, 0, 0);

	printf("Mapping window\n");
	XMapWindow(display, win);
}

void usage(char *cmd) {
	printf("Usage %s\n", cmd);
	printf("	-r n	Adds n number of random points (default 100)\n");
	printf("	-w n	Sets number of horizontal boxs to n (default 25)\n");
	printf("	-h n	Sets number of vertical boxs to n (default 25)\n");
	printf("	-d n	Sets frame update delay to n milliseconds (default 1000)\n");
	printf("	-p	Will start paused\n");
	printf("	--help	Shows this\n");
}

int main(int argc, char *argv[]) {
	pthread_t pth;
	XEvent event;
	int pending, i, random;

	width = 25;
	height = 25;
	delay = 1000 * 1000;

	random = 100;

	buttonpressed = 0;
	paused = 0;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-w") == 0) {
			width = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-h") == 0) {
			height = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-d") == 0) {
			delay = atoi(argv[++i]) * 1000;
		} else if (strcmp(argv[i], "-r") == 0) {
			random = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-p") == 0) {
			paused = 1;
		} else if (strcmp(argv[i], "--help") == 0) {
			usage(argv[0]);
			exit(1);
		} else {
			printf("Unknown option '%s'\n", argv[i]);
			usage(argv[0]);
			exit(1);
		}
	}

	boxwidth = boxheight = 20;

	points = malloc(sizeof(int) * (width * height));
	if (random)
		addrandom(random);
	buffer = malloc(sizeof(int) * (width * height));
	buffer = memcpy(buffer, points, sizeof(int) * width * height);

	setupwindow();

	printf("Starting\n");

	locked = 0;
	quit = 0;
	pthread_create(&pth, NULL, updateloop, "updater");
	while (!quit) {
		render();

		pending = XPending(display);
		if (!pending) {
			usleep(delay / 100);
		} else {
			XNextEvent(display, &event);
			if (handlers[event.type])
				handlers[event.type](&event);
		}
	}

	printf("Exiting\n");

	pthread_cancel(pth);

	return 0;
}
