
/*

    Window Management Library for Ruby
    SegPhault (Ryan Paul) - 02/13/05

    NOTES
    ------

    I used the source code of Tomas Styblo's wmctrl utility as a starting point
    to help me figure out some of the X stuff. You can find his excellent command
    line utility here: http://www.sweb.cz/tripie/utils/wmctrl/
    
    XGrabKey Resources:
    
    * http://mail.gnome.org/archives/gtk-app-devel-list/2005-August/msg00280.html
    * http://sunsolve.sun.com/search/document.do?assetkey=1-9-15043-1

    LICENSE
    -------
    
    This program is free software which I release under the GNU General Public
    License. You may redistribute and/or modify this program under the terms
    of that license as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    To get a copy of the GNU General Puplic License,  write to the
    Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
    
*/

#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Xmu/WinUtil.h>
#include <glib.h>

#include "ruby.h"

// Window management stuff

#define wm_root     DefaultRootWindow(disp)
#define open_disp   Display *disp = XOpenDisplay(NULL)
#define close_disp  XCloseDisplay(disp);

#define WM_TOP      0
#define WM_BOTTOM   1
#define WM_LEFT     2
#define WM_RIGHT    3
#define WM_WIDTH    4
#define WM_HEIGHT   5

char* xwm_get_win_title(Display *disp, Window win);
VALUE new_window(Window win);
VALUE window_desktop(VALUE self);
VALUE desktop_index(VALUE self);

static int xwm_long_msg(Display *disp, Window win, char *msg,
    long d0, long d1, long d2, long d3, long d4)
{
    XEvent e;
    long mask = SubstructureRedirectMask | SubstructureNotifyMask;

    e.xclient.type = ClientMessage;
    e.xclient.serial = 0;
    e.xclient.send_event = True;
    e.xclient.message_type = XInternAtom(disp, msg, False);
    e.xclient.window = win;
    e.xclient.format = 32;
    
    e.xclient.data.l[0] = d0;
    e.xclient.data.l[1] = d1;
    e.xclient.data.l[2] = d2;
    e.xclient.data.l[3] = d3;
    e.xclient.data.l[4] = d4;
    
    return XSendEvent(disp, DefaultRootWindow(disp),
        False, mask, &e) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int xwm_msg(Display *disp, Window win, char *msg, long d0)
{
    return xwm_long_msg(disp,win,msg,d0,0,0,0,0);
}

static gchar *get_property(Display *disp, Window win,
        Atom xa_prop_type, gchar *prop_name, unsigned long *size)
{
    unsigned long ret_nitems, ret_bytes_after, tmp_size;
    Atom xa_prop_name, xa_ret_type;
    unsigned char *ret_prop;
    int ret_format;
    gchar *ret;
    int size_in_byte;
    
    xa_prop_name = XInternAtom(disp, prop_name, False);
    
    if (XGetWindowProperty(disp, win, xa_prop_name, 0, 4096 / 4, False,
            xa_prop_type, &xa_ret_type, &ret_format, &ret_nitems,
            &ret_bytes_after, &ret_prop) != Success)
        return NULL;
  
    if (xa_ret_type != xa_prop_type)
    {
        XFree(ret_prop);
        return NULL;
    }

    switch(ret_format) {
	case 8:	size_in_byte = sizeof(char); break;
	case 16:	size_in_byte = sizeof(short); break;
	case 32:	size_in_byte = sizeof(long); break;
	}

    tmp_size = size_in_byte * ret_nitems;
    ret = g_malloc(tmp_size + 1);
    memcpy(ret, ret_prop, tmp_size);
    ret[tmp_size] = '\0';

    if (size) *size = tmp_size;
    
    XFree(ret_prop);
    return ret;
}

int xwm_has_state(Display* disp, Window win, gchar* state_name)
{
    long size=0;
    Atom state = XInternAtom(disp,state_name,False);
    Atom* list = (Atom*)get_property(disp,win,XA_CARDINAL,"_NET_WM_STATE",&size);
    int r = 0;
    int i;
    for (i=0; i<size/sizeof(Atom); i++) {
        if (list[i]==state)
            r = 1;
    }
    g_free(list);
    return r;
}

int xwm_get_win_desk(Display *disp, Window win)
{
    long *wdesk = (long*)get_property(disp,win, XA_CARDINAL,"_NET_WM_DESKTOP", NULL);
    long *nwdesk = (long*)get_property(disp, win, XA_CARDINAL,"_WIN_WORKSPACE", NULL);

    return nwdesk ? (int)*nwdesk : (wdesk ? (int)*wdesk : -1);
}

int xwm_get_win_pid(Display *disp, Window win)
{
    long *pid = (long*)get_property(disp,win, XA_CARDINAL,"_NET_WM_PID", NULL);
    if (pid==NULL)  return -1;

    return *pid;
}


void xwm_set_win_desk(Display *disp, Window win, int d)
{
    xwm_msg(disp, win, "_NET_WM_DESKTOP", d);
}

char *xwm_get_win_class(Display *disp, Window win)
{
    unsigned long size;
    return get_property(disp,win,XA_STRING, "WM_CLASS", &size);
}

char *xwm_get_win_title(Display *disp, Window win)
{
    char *wname = (char*)get_property(disp,win, XA_STRING, "WM_NAME", NULL);
    char *nwname = (char*)get_property(disp,win, XInternAtom(disp,
        "UTF8_STRING", False), "_NET_WM_NAME", NULL);

    return nwname ? nwname : (wname ? wname : NULL);
}

Window xwm_get_win_active(Display *disp)
{
    char *prop;
    unsigned long s;
    
    prop = get_property(disp, wm_root, XA_WINDOW,
            "_NET_ACTIVE_WINDOW", &s);

    return prop ? *((Window*)prop) : ((Window)0);
}

Window *xwm_window_list(Display *disp, gchar* prop1, gchar* prop2, long *size)
{
    Window *nlst = (Window*)get_property(disp, wm_root, XA_WINDOW,
            prop1 /*"_NET_CLIENT_LIST"*/, size);
    Window *lst = (Window *)get_property(disp, wm_root, XA_CARDINAL,
            prop2 /*"_WIN_CLIENT_LIST"*/, size);

    return nlst ? nlst : (lst ? lst : NULL);
}

Window* xwm_window_list_byage(Display* disp, long* size) {
	return xwm_window_list(disp,"_NET_CLIENT_LIST","_WIN_CLIENT_LIST",size);
}
Window* xwm_window_list_bystack(Display* disp, long* size) {
	return xwm_window_list(disp,"_NET_CLIENT_LIST_STACKING","_WIN_CLIENT_LIST_STACKING",size);
}

void xwm_desk_activate(Display *disp, int desk)
{
    xwm_msg(disp, wm_root, "_NET_CURRENT_DESKTOP", desk);
}

int xwm_desk_count(Display *disp)
{
    long *ndcount = (long*)get_property(disp, wm_root, XA_CARDINAL,
            "_NET_NUMBER_OF_DESKTOPS", NULL);
    long *dcount = (long*)get_property(disp, wm_root, XA_CARDINAL,
            "_WIN_WORKSPACE_COUNT", NULL);

    return ndcount ? (int)*ndcount : (dcount ? (int)*dcount : -1);
}

void xwm_win_close(Display *disp, Window win)
{
    xwm_msg(disp, win, "_NET_CLOSE_WINDOW",0);
}

void xwm_win_activate(Display *disp, Window win)
{
    int d = (int)xwm_get_win_desk(disp, win);
    if (d != -1) xwm_desk_activate(disp, d);
    xwm_msg(disp, win, "_NET_ACTIVE_WINDOW",0);
}

int xwm_get_win_size(Display *disp, Window win, int stype)
{
    XWindowAttributes a; int rx,ry; Window junkwin;

    if (!XGetWindowAttributes(disp, win, &a))
        return -1;

    int bw = a.border_width;

    // translate the top-left boundary of this windo into the coordinates of the root widnow
    XTranslateCoordinates (disp, win, a.root, 
            -bw,-bw,
            &rx, &ry, &junkwin);

    if (stype == WM_TOP)      return ry;
    if (stype == WM_BOTTOM)   return ry + a.height+bw*2;
    if (stype == WM_LEFT)     return rx;
    if (stype == WM_RIGHT)    return rx + a.width+bw*2;
    if (stype == WM_WIDTH)    return a.width;
    if (stype == WM_HEIGHT)   return a.height;
}

static gboolean xwm_supports(Display *disp, const char *prop)
{
    unsigned long size;
    Atom p = XInternAtom(disp, prop, False);
    Atom *list;
    int i;

    if (! (list = (Atom *)get_property(disp, wm_root, XA_ATOM,
            "_NET_SUPPORTED", &size)))
        return FALSE;

    for (i = 0; i < size / sizeof(Atom); i++)
        if (list[i] == p)
            return TRUE;
}

void xwm_win_move(Display *disp, Window win, int x, int y)
{
    XMoveWindow(disp, win, x, y);
}

void xwm_win_resize(Display *disp, Window win, int w, int h)
{
    XResizeWindow(disp, win, w, h);
}

void xwm_win_remove_decor(Display *disp, Window win)
{
   xwm_msg(disp, win, "_OB_WM_STATE_UNDECORATED", 0);
}

// --- Ruby Stuff ---

#define rb_cmdef          rb_define_singleton_method
#define rb_imdef          rb_define_method
#define rb_wmclass(n)     rb_define_class_under(mWM, n, rb_cObject)

#define RSTR              rb_str_new2
#define ISA               rb_obj_is_instance_of

#define rb_struct(n)      n *data; Data_Get_Struct(self, n, data)
#define rbwm_init(n)      open_disp; rb_struct(n)

#define WM_FILTER_CLASS   1
#define WM_FILTER_TITLE   2

#define same_win(w1,w2)  RTEST(window_op_eq(w1,w2))
#define same_desk(w1,w2) xwm_get_win_desk(disp,w1) == xwm_get_win_desk(disp,w2)

VALUE Obj_filter(VALUE self)
{
    open_disp;
    VALUE lst = rb_ary_new();
    
    VALUE iter(VALUE x)
    {
        if (RTEST(rb_yield(x))) rb_ary_push(lst,x);
        return Qnil;
    }

    rb_iterate(rb_each, self, iter, 0);
    return lst;
}

VALUE Obj_find(VALUE self)
{
    open_disp;
    VALUE wnd = Qnil;
    
    VALUE iter(VALUE x)
    {
        if (RTEST(rb_yield(x)))
        {
            wnd = x;
            rb_iter_break();
        }
        return Qnil;
    }

    rb_iterate(rb_each, self, iter, 0);
    return wnd;
}


VALUE mWM;
VALUE cWindow;
VALUE cDesktop;

typedef struct desktop
{
    int index;

} DESKTOP;

typedef struct window
{
    Window win;

} WINDOW;

VALUE Desktop_new(VALUE self, VALUE ind)
{
    DESKTOP *data;
    VALUE d = Data_Make_Struct(self, DESKTOP, 0, free, data);
    data->index = FIX2INT(ind);

    rb_obj_call_init(d, 0, 0);
    return d;
}

VALUE new_desktop(int ind)
{
    return Desktop_new(cDesktop, INT2FIX(ind));
}

VALUE Desktop_current(VALUE self)
{
    open_disp;
    Window w = xwm_get_win_active(disp);

    close_disp;
    return window_desktop(new_window(w));
}

VALUE Desktop_set_current(VALUE self, VALUE d)
{
    open_disp;
    
    if (TYPE(d) == T_FIXNUM)
        xwm_desk_activate(disp, FIX2INT(d));
    
    if (ISA(d, cDesktop))
        xwm_desk_activate(disp, FIX2INT(desktop_index(d)));

    close_disp;
    return self;
}

VALUE desktop_op_add(VALUE self, VALUE n)
{
    open_disp;
    int dval = -1;

    if (TYPE(n) == T_FIXNUM)
        dval = FIX2INT(desktop_index(self)) + FIX2INT(n);
    
    if (ISA(n, cDesktop))
        dval = FIX2INT(desktop_index(self)) + FIX2INT(desktop_index(n));

    close_disp;
    
    return new_desktop(dval != -1 ? dval : 0);
}

VALUE desktop_op_sub(VALUE self, VALUE n)
{
    open_disp;
    int dval = -1;

    if (TYPE(n) == T_FIXNUM)
        dval = FIX2INT(desktop_index(self)) - FIX2INT(n);
    
    if (ISA(n, cDesktop))
        dval = FIX2INT(desktop_index(self)) - FIX2INT(desktop_index(n));

    close_disp;
    
    return new_desktop(dval != -1 ? dval : 0);
}

VALUE desktop_op_eq(VALUE self, VALUE d)
{
    open_disp;

    if (ISA(d,cDesktop))
        return rb_equal(desktop_index(self), desktop_index(d));

    if (TYPE(d) == T_FIXNUM)
        return rb_equal(desktop_index(self), d);
    
    close_disp;

    return Qfalse;
}

VALUE Desktop_last(VALUE self)
{
    open_disp;

    int cnt = xwm_desk_count(disp);

    close_disp;
    return new_desktop(cnt - 1);
}

VALUE Desktop_first(VALUE self)
{
    return new_desktop(0);
}

VALUE Desktop_each(VALUE self)
{
    open_disp;
    int cnt = xwm_desk_count(disp);
    int i;

    for (i = 0; i <= cnt; i++)
        rb_yield(new_desktop(i));

    close_disp;
    return self;
}

VALUE Desktop_count(VALUE self)
{
    open_disp;
    int cnt = xwm_desk_count(disp);

    close_disp;
    return INT2FIX(cnt);
}

VALUE Desktop_root_window(VALUE self)
{
    open_disp;
    VALUE v = new_window(DefaultRootWindow(disp));

    close_disp;
    return v;
}

VALUE desktop_index(VALUE self)
{
    rb_struct(DESKTOP);
    return INT2FIX(data->index);
}

VALUE desktop_to_s(VALUE self)
{
    rb_struct(DESKTOP);
    char *txt;

    sprintf(txt, "%d", data->index);
    return RSTR(txt);
}

VALUE desktop_each(VALUE self)
{
    rbwm_init(DESKTOP);
    long cnt;
    int i;

    Window *lst = xwm_window_list_byage(disp, &cnt);

    for (i = 0; i < cnt / sizeof(Window); i++)
        if (xwm_get_win_desk(disp, lst[i]) == data->index)
            rb_yield(new_window(lst[i]));
    
    close_disp;
    return Qnil;
}

VALUE Window_new(VALUE self)
{
    WINDOW *data;
    VALUE w = Data_Make_Struct(self, WINDOW, 0, free, data);

    rb_obj_call_init(w, 0, 0);
    return w;
}

VALUE new_window(Window win)
{
    VALUE w = Window_new(cWindow);
    WINDOW *data;

    Data_Get_Struct(w, WINDOW, data);
    data->win = win;

    return w;
}

VALUE Window_current(VALUE self)
{
    open_disp;
    Window w = xwm_get_win_active(disp);
 
    close_disp;
    return w ? new_window(w) : Qnil;
}

VALUE Window_on_press(VALUE self, VALUE keys)
{
    open_disp;
    
    KeyCode mykey = 0;
    KeySym mysym;

    Window w = xwm_get_win_active(disp);
    mysym = XStringToKeysym(STR2CSTR(keys));
    mykey = XKeysymToKeycode(disp, mysym);

    XGrabKey(disp, mykey, AnyModifier, w, True, GrabModeAsync, GrabModeAsync);
    
    close_disp;
    return Qnil;
}

VALUE Window_key_loop(VALUE self)
{
    open_disp;

    XEvent ev;
    KeySym grabbed_key;

    for (;;)
    {
        XNextEvent(disp, &ev);

        switch (ev.type)
        {
            case KeyPress:
                grabbed_key = XKeycodeToKeysym(disp, ev.xkey.keycode, 0);
                printf("Test: %s", grabbed_key);

            default:
                break;
        }
    }
    
    return Qnil;
}

VALUE Window_each(VALUE self)
{
    open_disp;
    long cnt;
    int i;

    Window *lst = xwm_window_list_bystack(disp, &cnt);
    // printf("Found %d\n",cnt/sizeof(Window));

    for (i = 0; i < cnt / sizeof(Window); i++)
        rb_yield(new_window(lst[i]));

    close_disp;
    return Qnil;
}

VALUE window_root(VALUE self)
{
	rbwm_init(WINDOW);
	Window root,parent;
	int i; Window* children;
	XQueryTree(disp,data->win,&root,&parent,&children,&i);
	if (children!=NULL)	XFree(children);
	close_disp;
	return new_window(root);
}

VALUE Window_eachChild(VALUE self)
{
	rbwm_init(WINDOW);
	Window root,parent;
	int i;
	int nChild=0;
	Window* children=NULL;
	XQueryTree(disp,data->win,&root,&parent,&children,&nChild);
	for (i=0; i<nChild; i++)
		rb_yield(new_window(children[i]));
	if (children!=NULL)
		XFree(children);
	close_disp;
	return Qnil;
}

VALUE window_title(VALUE self)
{
    rbwm_init(WINDOW);
    char *t = xwm_get_win_title(disp, data->win);
 
    close_disp;
    return t ? RSTR(t) : Qnil;
}

VALUE window_class(VALUE self)
{
    rbwm_init(WINDOW);
    char *t = xwm_get_win_class(disp, data->win);

    close_disp;
    return t ? RSTR(t) : Qnil;
}

VALUE windim(VALUE self, int dt)
{
    rbwm_init(WINDOW);
    int s = xwm_get_win_size(disp, data->win, dt);

    close_disp;
    return INT2FIX(s);
}

VALUE window_top(VALUE self)      { return windim(self, WM_TOP); }
VALUE window_bottom(VALUE self)   { return windim(self, WM_BOTTOM); }
VALUE window_left(VALUE self)     { return windim(self, WM_LEFT); }
VALUE window_right(VALUE self)    { return windim(self, WM_RIGHT); }
VALUE window_width(VALUE self)    { return windim(self, WM_WIDTH); }
VALUE window_height(VALUE self)   { return windim(self, WM_HEIGHT); }

/*
 * Is the window visible?
 *
 * This excludes minimized windows. Not sure exactly what else would fit
 * in this category.
 */
VALUE window_visible(VALUE self)
{
    XWindowAttributes a;
    rbwm_init(WINDOW);

    if (!XGetWindowAttributes(disp, data->win, &a))
        return -1;

    close_disp;
    return a.map_state==IsViewable ? Qtrue : Qfalse;
}

VALUE window_id(VALUE self)
{
    rbwm_init(WINDOW);
    VALUE v = LONG2FIX(data->win);

    close_disp;
    return v; 
}

VALUE window_set_desktop(VALUE self, VALUE dsk)
{
    rbwm_init(WINDOW);
    xwm_set_win_desk(disp, data->win, FIX2INT(dsk));
    close_disp;

    return Qnil;
}

VALUE window_set_left(VALUE self, VALUE x)
{
    rbwm_init(WINDOW);
    xwm_win_move(disp, data->win, FIX2INT(x), FIX2INT(window_top(self)));
    
    close_disp;
    return Qnil;
}

VALUE window_set_top(VALUE self, VALUE x)
{
    rbwm_init(WINDOW);
    xwm_win_move(disp, data->win, FIX2INT(window_left(self)), FIX2INT(x));
    
    close_disp;
    return Qnil;
}

VALUE window_set_width(VALUE self, VALUE x)
{
    rbwm_init(WINDOW);
    xwm_win_resize(disp, data->win, FIX2INT(x), FIX2INT(window_height(self)));
    
    close_disp;
    return Qnil;
}

VALUE window_set_height(VALUE self, VALUE x)
{
    rbwm_init(WINDOW);
    xwm_win_resize(disp, data->win, FIX2INT(window_width(self)), FIX2INT(x));
    
    close_disp;
    return Qnil;
}

VALUE window_op_eq(VALUE self, VALUE other)
{
    // Match window title
    if (ISA(other,cWindow))
        return rb_equal(window_id(self), window_id(other));

    if (TYPE(other) == T_REGEXP)
        return rb_reg_match(other, window_title(self));

    if (TYPE(other) == T_STRING)
        return rb_equal(window_title(self), other);

    if (TYPE(other) == T_FIXNUM)
        return rb_equal(desktop_index(window_desktop(self)), other);

    return Qfalse;
}

VALUE window_op_teq(VALUE self, VALUE other)
{
    // Match window class rather than title
    if (ISA(other,cWindow))
        return rb_equal(window_class(self), window_class(other));

    if (TYPE(other) == T_REGEXP)
        return rb_reg_match(other, window_class(self));

    if (TYPE(other) == T_STRING)
        return rb_equal(window_class(self), other);
    
    if (TYPE(other) == T_FIXNUM)
        return rb_equal(desktop_index(window_desktop(self)), other);

    return Qfalse;    
}

VALUE wm_filter(VALUE self, VALUE other, int ftype)
{
    open_disp;
    VALUE lst = rb_ary_new();
    
    VALUE iter(VALUE x)
    {
        if (ftype == WM_FILTER_CLASS)
            if (RTEST(window_op_teq(x, other))) rb_ary_push(lst, x);

        if (ftype == WM_FILTER_TITLE)
            if (RTEST(window_op_eq(x, other))) rb_ary_push(lst, x);
        
        return Qnil;
    }
    
    rb_iterate(rb_each, self, iter, 0);

    close_disp;
    return lst;
}

VALUE wm_find(VALUE self, VALUE other, int ftype)
{
    open_disp;
    VALUE fnd = Qnil;
    
    VALUE iter(VALUE x)
    {
        if (ftype == WM_FILTER_CLASS)
            if (RTEST(window_op_teq(x, other)))
            {
                fnd = x;
                rb_iter_break();
            }

        if (ftype == WM_FILTER_TITLE)
            if (RTEST(window_op_eq(x, other)))
            {
                fnd = x;
                rb_iter_break();
            }
        
        return Qnil;
    }
    
    rb_iterate(rb_each, cWindow, iter, 0);

    close_disp;
    return fnd;
}

VALUE Window_op_div(VALUE self, VALUE other)
{
    return wm_filter(self,other,WM_FILTER_TITLE);
}

VALUE Window_op_gi(VALUE self, VALUE other)
{
    return wm_filter(self,other,WM_FILTER_CLASS);
}

VALUE Window_window(VALUE self, VALUE x)
{
    return wm_find(self, x, WM_FILTER_CLASS);
}

VALUE Window_op_mul(VALUE self, VALUE x)
{
    return wm_find(self, x, WM_FILTER_TITLE);
}

VALUE window_desktop(VALUE self)
{
    rbwm_init(WINDOW);
    int d = xwm_get_win_desk(disp, data->win);
 
    close_disp;
    return new_desktop(d);
}

VALUE window_pid(VALUE self)
{
    rbwm_init(WINDOW);
    int pid = xwm_get_win_pid(disp, data->win);
 
    close_disp;
    return INT2FIX(pid);
}

VALUE window_activate(VALUE self)
{
    rbwm_init(WINDOW);
    xwm_win_activate(disp, data->win);

    close_disp;
    return self;
}

VALUE window_remove_decor(VALUE self)
{
    rbwm_init(WINDOW);
    xwm_win_remove_decor(disp, data->win);
    
    close_disp;
    return Qnil;
}

// This is where it starts to get ugly
// I'm still debugging/reworking all this. It doesnt really work yet.

VALUE window_dir(VALUE self, int dir)
{
    rbwm_init(WINDOW);
    
    int i,junkx,junky,bw,depth;
    int wwidth,wheight,wleft,wtop,wright,wbottom;
    int owidth,oheight,oleft,otop,oright,obottom;
    Window junkroot;
    long cnt;
    
    XGetGeometry(disp, data->win, &junkroot, &junkx, &junky,
            &wwidth, &wheight, &bw, &depth);

    XTranslateCoordinates(disp, data->win, junkroot, junkx, junky,
            &wleft, &wtop, &junkroot);

    wright = wleft + wwidth;
    wbottom = wtop + wheight;

    
    VALUE ret = self;
    Window *lst = xwm_window_list_byage(disp, &cnt);

    for (i = 0; i < cnt / sizeof(Window); i++)
    {
        XGetGeometry(disp, lst[i], &junkroot, &junkx, &junky,
            &owidth, &oheight, &bw, &depth);

        XTranslateCoordinates(disp, lst[i], junkroot, junkx, junky,
            &oleft, &otop, &junkroot);

        oright = oleft + owidth;
        obottom = otop + oheight;

        if (same_desk(data->win, lst[i]))
        {
            if (dir == WM_LEFT)
                if (oright < wleft && (obottom > wtop && otop < wbottom))
                    if (!same_win(ret,self) && (oright > FIX2INT(window_right(ret))))
                        ret = new_window(lst[i]);
                    else
                        ret = new_window(lst[i]);


            if (dir == WM_RIGHT)
                if (oleft > wright && (obottom > wtop && otop < wbottom))
                    if (!same_win(ret,self) && (oleft < FIX2INT(window_left(ret))))
                        ret = new_window(lst[i]);
                    else
                    {
                        printf("%s - %d - %d\n", xwm_get_win_title(disp, lst[i]), oleft, FIX2INT(window_left(ret)));
                        ret = new_window(lst[i]);
                    }
        }
    }
    
    close_disp;
    return RTEST(window_op_eq(ret,self)) ? Qnil : ret;
}

// this is where the ugliness stops
// ugliness will eventually be purged

VALUE window_dir_left(VALUE self)   { return window_dir(self, WM_LEFT); }
VALUE window_dir_right(VALUE self)  { return window_dir(self, WM_RIGHT); }
VALUE window_dir_top(VALUE self)    { return window_dir(self, WM_TOP); }
VALUE window_dir_bottom(VALUE self) { return window_dir(self, WM_BOTTOM); }

void Init_wmlib()
{
    mWM = rb_define_module("WM");
    rb_imdef(mWM, "Window", Window_window, 1);

    cDesktop = rb_wmclass("Desktop");
    rb_cmdef(cDesktop, "new", Desktop_new, 1);
    rb_cmdef(cDesktop, "current", Desktop_current, 0);
    rb_cmdef(cDesktop, "current=", Desktop_set_current, 1);
    rb_cmdef(cDesktop, "each", Desktop_each, 0);
    rb_cmdef(cDesktop, "count", Desktop_count, 0);
    rb_cmdef(cDesktop, "last", Desktop_last, 0);
    rb_cmdef(cDesktop, "first", Desktop_first, 0);
    rb_imdef(cDesktop, "index", desktop_index, 0);
    rb_imdef(cDesktop, "each", desktop_each, 0);
    rb_imdef(cDesktop, "filter", Obj_filter, 0);
    rb_imdef(cDesktop, "find", Obj_find, 0);
    rb_imdef(cDesktop, "to_s", desktop_to_s, 0);
    rb_imdef(cDesktop, "+", desktop_op_add, 1);
    rb_imdef(cDesktop, "-", desktop_op_sub, 1);
    rb_imdef(cDesktop, "==", desktop_op_eq, 1);
    rb_imdef(cDesktop, "root_window", Desktop_root_window,0);

    cWindow = rb_wmclass("Window");
    rb_cmdef(cWindow, "new", Window_new, 0);
    rb_cmdef(cWindow, "current", Window_current, 0);
    rb_cmdef(cWindow, "on_press", Window_on_press, 1);
    rb_cmdef(cWindow, "key_loop", Window_key_loop, 0);

    rb_cmdef(cWindow, "each", Window_each, 0);
    rb_imdef(cWindow, "eachChild", Window_eachChild, 0);
    rb_cmdef(cWindow, "filter", Obj_filter, 0);
    rb_cmdef(cWindow, "find", Obj_find, 0);
    rb_cmdef(cWindow, "*", Window_op_mul, 1);
    rb_cmdef(cWindow, "/", Window_op_div, 1);
    rb_cmdef(cWindow, "[]", Window_op_gi, 1);
    rb_imdef(cWindow, "==", window_op_eq, 1);
    rb_imdef(cWindow, "=~", window_op_teq, 1);
    rb_imdef(cWindow, "to_s", window_title, 0);
    rb_imdef(cWindow, "title", window_title, 0);
    rb_imdef(cWindow, "id", window_id, 0);
    rb_imdef(cWindow, "visible?", window_visible, 0);
    rb_imdef(cWindow, "winclass", window_class, 0);
    rb_imdef(cWindow, "desktop", window_desktop, 0);
    rb_imdef(cWindow, "pid", window_pid, 0);
    rb_imdef(cWindow, "desktop=", window_set_desktop, 1);
    rb_imdef(cWindow, "activate", window_activate, 0);

    rb_imdef(cWindow, "root", window_root, 0);
    rb_imdef(cWindow, "height", window_height, 0);
    rb_imdef(cWindow, "width", window_width, 0);
    rb_imdef(cWindow, "top", window_top, 0);
    rb_imdef(cWindow, "bottom", window_bottom, 0);
    rb_imdef(cWindow, "left", window_left, 0);
    rb_imdef(cWindow, "right", window_right, 0);

    rb_imdef(cWindow, "height=", window_set_height, 1);
    rb_imdef(cWindow, "width=", window_set_width, 1);
    rb_imdef(cWindow, "top=", window_set_top, 1);
    rb_imdef(cWindow, "left=", window_set_left, 1);

    rb_imdef(cWindow, "dir_left", window_dir_left, 0);
    rb_imdef(cWindow, "dir_right", window_dir_right, 0);
    rb_imdef(cWindow, "dir_top", window_dir_top, 0);
    rb_imdef(cWindow, "dir_bottom", window_dir_bottom, 0);

    rb_imdef(cWindow, "remove_decor", window_remove_decor, 0);
}
