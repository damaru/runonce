#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <cargs.h>
#include <sys/wait.h>

Display* dpy;
char* argv0;

static void
window_property(Window window, char* name, char** values)
{
	Atom prop;
	Atom realType;
	unsigned long n;
	unsigned long extra;
	int format;
	int status;
	char* value;
	int i, j = 0;

	prop = XInternAtom(dpy, name, True);
	if (prop == None) {
		fprintf(stderr, "%s: no such property '%s'\n", argv0, name);
		return;
	}

	status = XGetWindowProperty(dpy, window, prop, 0L, 512L, 0, AnyPropertyType,
	    &realType, &format, &n, &extra, (unsigned char**)&value);
	if (status != Success || n == 0) {
		values[0] = 0;
		return;
	}

	values[0] = NULL;
	if (format == 8) {
		values[0] = strdup(value);
		for (j = 0, i = 0; i < n; i++) {
			if (value[i] == 0) {
				values[++j] = strdup(&value[i + 1]);
			}
		}
		j++;
	} else if (format == 32) {
		for (j = 0, i = 0; i < n; i++) {
#if __x86_64__
			values[j++] = (char*)((uint64_t*)value)[i];
#else
			values[j++] = (char*)((int*)value)[i];
#endif
		}
	}

	values[j] = NULL;
	XFree(value);
}

static int
list_windows(Window w, Window** ws)
{
	unsigned nkids;
	Window dumb;

	XQueryTree(dpy, w, &dumb, &dumb, ws, &nkids);
	return nkids;
}

void send_client_message(Window win, long type, long l0, long l1, long l2)
{
	XClientMessageEvent xev;

	Window root = DefaultRootWindow(dpy);
	xev.type = ClientMessage;
	xev.window = win;
	xev.message_type = type;
	xev.format = 32;
	xev.data.l[0] = l0;
	xev.data.l[1] = l1;
	xev.data.l[2] = l2;
	xev.data.l[3] = 0;
	xev.data.l[4] = 0;
	XSendEvent(dpy, root, False,
	    (SubstructureNotifyMask | SubstructureRedirectMask),
	    (XEvent*)&xev);
}

static Window
find_window(char* class, Window* rets, int count)
{
	Window* wins;
	Window dumb;
	Atom prop;
	int i;
	unsigned nkids;
	int j;
	int r = 0;

	for (i = 0; i < count; i++) {
		rets[i] = 0;
	}

	nkids = list_windows(DefaultRootWindow(dpy), &wins);

	prop = XInternAtom(dpy, "WM_CLASS", True);

	for (i = nkids - 1; i >= 0; i--) {
		XWindowAttributes attr;
		Window* kids2;
		unsigned nkids2;
		int i2;
		char* classes[122];

		if (XGetWindowAttributes(dpy, wins[i], &attr)) {
			if (attr.override_redirect == 0
			    && attr.map_state != IsUnmapped
			    && !XGetTransientForHint(dpy, wins[i], &dumb)) {

				window_property(wins[i], "WM_CLASS", classes);
				for (j = 0; classes[j]; j++) {
					if (strstr(classes[j], class)) {
						rets[r++] = wins[i];
						break;
					}
				}
			}
			nkids2 = list_windows(wins[i], &kids2);
			for (i2 = 0; i2 < nkids2; i2++) {
				XGetWindowAttributes(dpy, kids2[i2], &attr);
				if (attr.override_redirect == 0 && attr.map_state == IsViewable) {
					classes[0] = 0;
					window_property(kids2[i2], "WM_CLASS", classes);
					for (j = 0; classes[j]; j++) {
						if (strstr(classes[j], class)) {
							rets[r++] = kids2[i2];
							break;
						}
					}
				}
			}
			if (kids2)
				XFree(kids2);
		}
	}

	if (wins)
		XFree(wins);
	return r;
}

static int
handler(Display* disp, XErrorEvent* err)
{
	fprintf(stderr, "%s: no window with id %#x\n", argv0, (int)err->resourceid);
	exit(EXIT_FAILURE);
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s\n", argv0);
	fprintf(stderr, "    -c application's WM_CLASS\n");
	fprintf(stderr, "    -s server command\n");
	fprintf(stderr, "    -e client command\n");
}

CargsDesc args[4] = {
	{ "-c", CargsVal },
	{ "-s", CargsVal },
	{ "-e", CargsVal },
	{ NULL }
};

void spawn(const char* cmd)
{
	static const char* shell = NULL;
	if (!(shell = getenv("SHELL")))
		shell = "/bin/sh";
	if (!cmd)
		return;
	if (fork() == 0) {
		if (fork() == 0) {
			if (dpy)
				close(ConnectionNumber(dpy));
			setsid();
			execl(shell, shell, "-c", cmd, (char*)NULL);
			fprintf(stderr, "runonce: execl '%s -c %s' failed (%s)", shell, cmd, strerror(errno));
		}
		exit(0);
	}
	wait(0);
}

void x_switch(Window w)
{
	XMapRaised(dpy, w);
	//XRaiseWindow(dpy, w);
	send_client_message(w, XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False), 2, CurrentTime, 0);
	XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
	XWarpPointer(dpy, None, w, 0, 0, 0, 0, 10, 10);
	XSync(dpy, True);
}

extern int
main(int argc, char* argv[])
{

	char* class = NULL;
	char* server = NULL;
	char* client = NULL;
	Window wins[64];
	Window w = 0;

	argv0 = argv[0];

	cargs_process(&argc, &argv, args);

	CARGS_GET(args, 0, class);
	CARGS_GET(args, 1, server);
	CARGS_GET(args, 2, client);

	dpy = XOpenDisplay("");
	if (dpy == 0) {
		fprintf(stderr, "%s: can't open display.\n", argv0);
		exit(EXIT_FAILURE);
	}

	XSetErrorHandler(handler);

	if (class == NULL) {
		if (client != NULL) {
			class = client;
		} else {
			class = server;
		}
	}

	if (class == NULL) {
		fprintf(stderr, "class or command not specified\n");
		usage();
		return EXIT_FAILURE;
	}

	if (find_window(class, wins, 64)) {
		w = wins[0];
	}

	if (w) {
		x_switch(w);
	} else if (server) {
		spawn(server);
		return EXIT_SUCCESS;
	}

	if (client) {
		spawn(client);
		if (find_window(class, wins, 64)) {
			w = wins[0];
			x_switch(w);
		}
	}
	return EXIT_SUCCESS;
}
