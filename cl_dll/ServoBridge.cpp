#ifdef _CS16CLIENT_ENABLE_SERVO_HTML

#include "hud.h"
#include "cl_util.h"
#include "cl_dll.h"
#include "triangleapi.h"
#include "render_api.h"
#include "draw_util.h"
#include "cvardef.h"
#include "keydefs.h"

#include "srv_view.h" // 3rdparty/servohtml
#include "ServoBridge.h"

#include <cstdio>
#include <string>

#if defined( _WIN32 )
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
	#include <cstdarg>
#else
	#include <dlfcn.h>
	#if defined( __APPLE__ )
		#include <OpenGL/OpenGL.h> // CGLGetCurrentContext / CGLSetCurrentContext (link: -framework OpenGL)
	#endif
#endif

// ---------------------------------------------------------------------------
// GL context guard: Servo makes its own GL context current inside every call.
// Restore the engine's context afterwards or the engine draws into Servo's.
// ---------------------------------------------------------------------------
#if defined( __APPLE__ )

struct GlGuard
{
	CGLContextObj saved;
	GlGuard() : saved( CGLGetCurrentContext() ) {}
	~GlGuard() { CGLSetCurrentContext( saved ); }
};

#elif defined( _WIN32 )

struct WglFuncs
{
	HGLRC ( WINAPI *getContext )();
	HDC ( WINAPI *getDC )();
	BOOL ( WINAPI *makeCurrent )( HDC, HGLRC );
	WglFuncs()
	{
		HMODULE gl = LoadLibraryA( "opengl32.dll" );
		getContext = gl ? (decltype( getContext ))GetProcAddress( gl, "wglGetCurrentContext" ) : nullptr;
		getDC = gl ? (decltype( getDC ))GetProcAddress( gl, "wglGetCurrentDC" ) : nullptr;
		makeCurrent = gl ? (decltype( makeCurrent ))GetProcAddress( gl, "wglMakeCurrent" ) : nullptr;
	}
};

struct GlGuard
{
	HGLRC rc = nullptr;
	HDC dc = nullptr;
	static WglFuncs &F() { static WglFuncs f; return f; }
	GlGuard()
	{
		WglFuncs &f = F();
		if( f.getContext && f.getDC ) { rc = f.getContext(); dc = f.getDC(); }
	}
	~GlGuard()
	{
		WglFuncs &f = F();
		if( f.makeCurrent ) f.makeCurrent( dc, rc );
	}
};

#else // Linux (GLX or EGL, detected at runtime) and Android (EGL)

struct EglGlxFuncs
{
	void *( *eglGetCurrentDisplay )();
	void *( *eglGetCurrentContext )();
	void *( *eglGetCurrentSurface )( int );
	unsigned ( *eglMakeCurrent )( void *, void *, void *, void * );
	void *( *glxGetCurrentDisplay )();
	void *( *glxGetCurrentContext )();
	unsigned long ( *glxGetCurrentDrawable )();
	unsigned long ( *glxGetCurrentReadDrawable )();
	int ( *glxMakeContextCurrent )( void *, unsigned long, unsigned long, void * );

	EglGlxFuncs()
	{
#define L( f, n ) f = reinterpret_cast<decltype( f )>( dlsym( RTLD_DEFAULT, n ) )
		L( eglGetCurrentDisplay, "eglGetCurrentDisplay" );
		L( eglGetCurrentContext, "eglGetCurrentContext" );
		L( eglGetCurrentSurface, "eglGetCurrentSurface" );
		L( eglMakeCurrent, "eglMakeCurrent" );
		L( glxGetCurrentDisplay, "glXGetCurrentDisplay" );
		L( glxGetCurrentContext, "glXGetCurrentContext" );
		L( glxGetCurrentDrawable, "glXGetCurrentDrawable" );
		L( glxGetCurrentReadDrawable, "glXGetCurrentReadDrawable" );
		L( glxMakeContextCurrent, "glXMakeContextCurrent" );
#undef L
	}
};

struct GlGuard
{
	enum Kind { None, Egl, Glx } kind = None;
	void *dpy = nullptr, *ctx = nullptr;
	void *eDraw = nullptr, *eRead = nullptr; // EGL surfaces
	unsigned long xDraw = 0, xRead = 0;      // GLX drawables

	static EglGlxFuncs &F() { static EglGlxFuncs f; return f; }

	GlGuard()
	{
		EglGlxFuncs &f = F();
		// GLX first: on a GLX engine eglGetCurrentContext() is NULL anyway, and the reverse is also true.
		if( f.glxGetCurrentContext && ( ctx = f.glxGetCurrentContext() ) )
		{
			kind = Glx;
			dpy = f.glxGetCurrentDisplay();
			xDraw = f.glxGetCurrentDrawable();
			xRead = f.glxGetCurrentReadDrawable ? f.glxGetCurrentReadDrawable() : xDraw;
		}
		else if( f.eglGetCurrentContext && ( ctx = f.eglGetCurrentContext() ) )
		{
			kind = Egl;
			dpy = f.eglGetCurrentDisplay();
			eDraw = f.eglGetCurrentSurface( 0x3059 ); // EGL_DRAW
			eRead = f.eglGetCurrentSurface( 0x305A ); // EGL_READ
		}
	}
	~GlGuard()
	{
		EglGlxFuncs &f = F();
		if( kind == Glx && f.glxMakeContextCurrent )
			f.glxMakeContextCurrent( dpy, xDraw, xRead, ctx );
		else if( kind == Egl && f.eglMakeCurrent )
			f.eglMakeCurrent( dpy, eDraw, eRead, ctx );
	}
};

#endif

// ---------------------------------------------------------------------------
// Library loading: the Servo library sits next to this client library.
// ---------------------------------------------------------------------------
#ifndef SERVO_LIB_FILE
	#error "SERVO_LIB_FILE IS MISSING"
#endif

#if defined( _WIN32 )
	#define LIB_SYM( h, n ) ( (void *)GetProcAddress( (HMODULE)( h ), n ) )
#else
	#define LIB_SYM( h, n ) dlsym( h, n )
#endif

struct ServoApi
{
	ServoView *(*create)( const ServoViewConfig * );
	void ( *destroy )( ServoView * );
	void ( *resize )( ServoView *, uint32_t, uint32_t );
	void ( *load_url )( ServoView *, const char * );
	int ( *acquire )( ServoView *, ServoViewFrame * );
	void ( *mouse_move )( ServoView *, float, float );
	void ( *mouse_button )( ServoView *, int, int, float, float );
	void ( *wheel )( ServoView *, float, float, float, float );
	void ( *key )( ServoView *, int, uint32_t, int, uint32_t );
	void ( *text )( ServoView *, const char * );
	uint32_t ( *abi_version )();
};

static ServoApi   g_api;
static ServoView *g_view;
static void      *g_lib; // never unloaded: Servo threads outlive any view
static bool       g_apiReady = false; // true only after all symbols have been bound and ABI checked

static int      g_tex;
static uint32_t g_texW, g_texH;
static int      g_texSerial;
static float    g_rx1, g_ry1, g_rx2, g_ry2; // last drawn rect (screen coords)

// input state
static bool     g_focus;
static float    g_cx, g_cy;                 // virtual cursor, screen coords
static bool     g_shift, g_ctrl, g_alt, g_caps;
static cvar_t  *g_cursorScale;              // servo_cursor_scale
static bool     g_cmdsRegistered;
static void     ClampCursor();

static void ServoLog( int level, const char *fmt, ... )
{
	char message[2048];

	va_list args;
	va_start( args, fmt );
	vsnprintf( message, sizeof( message ), fmt, args );
	va_end( args );

	switch( level )
	{
	case 3: // ERROR
		gEngfuncs.Con_DPrintf( "ServoHTML: %s\n", message );
		break;
	case 0: // TRACE
	case 1: // INFO
	case 2: // WARN
	default:
		gEngfuncs.Con_Printf( "ServoHTML: %s\n", message );
		break;
	}
}

static std::string ModuleDir()
{
	std::string path;
#if defined( _WIN32 )
	HMODULE mod = nullptr;
	char buf[MAX_PATH] = {};
	if( GetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCSTR)&ModuleDir, &mod ) && GetModuleFileNameA( mod, buf, MAX_PATH ) )
		path = buf;
	size_t slash = path.find_last_of( "\\/" );
#else
	Dl_info info;
	if( dladdr( (void *)&ModuleDir, &info ) && info.dli_fname )
		path = info.dli_fname;
	size_t slash = path.rfind( '/' );
#endif
	return slash != std::string::npos ? path.substr( 0, slash ) : std::string( "." );
}

static void ResetApi()
{
	g_api = {};
	g_apiReady = false;
}

static bool LoadApi()
{
	if( g_apiReady )
		return true;

	if( g_lib )
		return false;

	std::string dir = ModuleDir();
	std::string path = dir + "/" SERVO_LIB_FILE;

#if defined( _WIN32 )
	SetDllDirectoryA( dir.c_str() );
	g_lib = (void*)LoadLibraryExA( path.c_str(), nullptr,
		LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS );

	if( !g_lib )
	{
		ServoLog( 3, "LoadLibrary(%s) failed, error %lu", path.c_str(), GetLastError() );
		return false;
	}
#else
	g_lib = dlopen( path.c_str(), RTLD_NOW | RTLD_LOCAL );

	if( !g_lib )
	{
		ServoLog(3, "^1dlopen(%s) failed:^7 %s", path.c_str(), dlerror());
		return false;
	}
#endif

#define BIND( field, sym ) \
	g_api.field = reinterpret_cast<decltype( g_api.field )>( LIB_SYM( g_lib, sym ) ); \
	if( !g_api.field ) \
	{ \
		ServoLog(3, "missing symbol %s", sym); \
		goto bind_failed; \
	}

	BIND( abi_version, "servo_html_abi_version" );
	BIND( create, "servo_html_create" );
	BIND( destroy, "servo_html_destroy" );
	BIND( resize, "servo_html_resize" );
	BIND( load_url, "servo_html_load_url" );
	BIND( acquire, "servo_html_acquire" );
	BIND( mouse_move, "servo_html_mouse_move" );
	BIND( mouse_button, "servo_html_mouse_button" );
	BIND( wheel, "servo_html_wheel" );
	BIND( key, "servo_html_key" );
	BIND( text, "servo_html_text" );

#undef BIND

	if( g_api.abi_version() != SERVO_HTML_ABI_VERSION )
	{
		ServoLog(3, "^1ABI mismatch:^7 library %u, client %u", g_api.abi_version(), (unsigned)SERVO_HTML_ABI_VERSION );
		goto bind_failed;
	}

	g_apiReady = true;

	ServoLog( 1, "^2library loaded^7 API ver. %u", (unsigned)SERVO_HTML_ABI_VERSION );
	return true;

bind_failed:

	ResetApi();

#if defined( _WIN32 )
	FreeLibrary( (HMODULE )g_lib );
#else
	dlclose( g_lib );
#endif
	g_lib = nullptr;

	return false;
}

// ---------------------------------------------------------------------------

void ServoUI_Init()
{
	if( g_apiReady )
		return;

	if( !LoadApi() )
		return;

	if( !g_cmdsRegistered )
	{
		g_cmdsRegistered = true;

		g_cursorScale = gEngfuncs.pfnRegisterVariable(
				"servo_cursor_scale", "1", 0 );

		gEngfuncs.pfnAddCommand(
				"servo_focus",
				[]() {
						ServoUI_SetFocus( !g_focus );
				} );

		gEngfuncs.pfnAddCommand(
				"servo_url",
				[]() {
					if( g_view && gEngfuncs.Cmd_Argc() > 1 )
					{
						GlGuard guard;
						g_api.load_url( g_view, gEngfuncs.Cmd_Argv( 1 ) );
					}
				} );
	}
}

bool ServoUI_Create( int w, int h, int posX, int posY, const char *url )
{
	if( !g_apiReady )
	{
		ServoLog( 3, "create failed: ^1g_apiReady not ready^7");
		return false;
	}

	if( g_view )
	{
			ServoLog(2, "create skipped: view already exists" );
			return true;
	}

	if( w <= 0 || h <= 0 )
	{
			ServoLog( 2, "invalid view size: %dx%d", w, h );
			return false;
	}

	ServoViewConfig cfg = {};
	cfg.abi_version = SERVO_HTML_ABI_VERSION;
	cfg.width = (uint32_t)w;
	cfg.height = (uint32_t)h;
	cfg.url = url;
	cfg.log = ServoLog;

	{
		GlGuard guard;
		g_view = g_api.create( &cfg );
	}

	ServoLog( 2, "view %s", g_view ? "^2created^7" : "^1FAILED^7" );
	return g_view != nullptr;
}

void ServoUI_Shutdown()
{
	g_focus = false;
	if( !g_view )
		return;
	{
		GlGuard guard;
		g_api.destroy( g_view );
	}
	g_view = nullptr;
	g_tex = 0; // the engine frees its textures on renderer shutdown
}

bool ServoUI_Active()
{
	return g_view != nullptr;
}

void ServoUI_Frame()
{
	if( !g_view )
		return;

	ServoViewFrame frame = {};
	int got;
	{
		GlGuard guard; // engine context is restored before any engine GL call below
		got = g_api.acquire( g_view, &frame );
	}

	if( !got || !frame.pixels )
		return; // nothing new: keep the previous texture

	const texFlags_t flags = (texFlags_t)( TF_NOMIPMAP | TF_CLAMP );

	if( !g_tex || frame.width != g_texW || frame.height != g_texH )
	{
		// New name per size: GL_CreateTexture(TF_UPDATE) needs an existing texture of the same size.
		char name[32];
		snprintf( name, sizeof( name ), "*srv_ui_%d", ++g_texSerial );
		g_tex = gRenderAPI.GL_CreateTexture( name, frame.width, frame.height, frame.pixels, flags );
		g_texW = frame.width;
		g_texH = frame.height;
	}
	else
	{
		char name[32];
		snprintf( name, sizeof( name ), "*srv_ui_%d", g_texSerial );
		gRenderAPI.GL_CreateTexture( name, frame.width, frame.height, frame.pixels, (texFlags_t)( flags | TF_UPDATE ));
	}
}

void ServoUI_Draw( float x1, float y1, float x2, float y2 )
{
	if( !g_tex )
		return;

	g_rx1 = x1; g_ry1 = y1; g_rx2 = x2; g_ry2 = y2;

	gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha ); // if colors look wrong, try kRenderTransColor as sniperscope.cpp does
	gEngfuncs.pTriAPI->Color4ub( 255, 255, 255, 255 );
	gRenderAPI.GL_Bind( 0, g_tex );
	DrawUtils::Draw2DQuad( x1, y1, x2, y2 );

	if( g_focus )
	{
		ClampCursor();
		// small white square with a dark outline; no texture needed
		gEngfuncs.pfnFillRGBABlend( (int)g_cx - 3, (int)g_cy - 3, 8, 8, 0, 0, 0, 255 );
		gEngfuncs.pfnFillRGBABlend( (int)g_cx - 2, (int)g_cy - 2, 6, 6, 255, 255, 255, 255 );
	}
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
static bool ToPage( float sx, float sy, float &px, float &py )
{
	if( !g_view || !g_texW || g_rx2 <= g_rx1 || g_ry2 <= g_ry1 )
		return false;
	px = ( sx - g_rx1 ) / ( g_rx2 - g_rx1 ) * (float)g_texW;
	py = ( sy - g_ry1 ) / ( g_ry2 - g_ry1 ) * (float)g_texH;
	return true;
}

static void ClampCursor()
{
	if( g_cx < g_rx1 ) g_cx = g_rx1;
	if( g_cx > g_rx2 ) g_cx = g_rx2;
	if( g_cy < g_ry1 ) g_cy = g_ry1;
	if( g_cy > g_ry2 ) g_cy = g_ry2;
}

bool ServoUI_HasFocus()
{
	return g_focus && g_view;
}

void ServoUI_SetFocus( bool focus )
{
	if( focus && !g_view )
		return;
	if( focus && !g_focus )
	{
		// start in the middle of the page
		g_cx = ( g_rx1 + g_rx2 ) * 0.5f;
		g_cy = ( g_ry1 + g_ry2 ) * 0.5f;
	}
	g_focus = focus;
	ServoLog( 1, "focus %s", focus ? "^2ON^7 ^5(Esc to leave)^7" : "^1OFF^7" );
}

void ServoUI_LookEvent( float relyaw, float relpitch )
{
	if( !g_view )
		return;

	// IN_CollectInput() in the engine turns pixels into angles: yaw -= x * m_yaw, pitch += y * m_pitch.
	float myaw = gEngfuncs.pfnGetCvarFloat( "m_yaw" );
	float mpitch = gEngfuncs.pfnGetCvarFloat( "m_pitch" );
	if( myaw == 0.0f ) myaw = 0.022f;
	if( mpitch == 0.0f ) mpitch = 0.022f;
	float scale = g_cursorScale && g_cursorScale->value > 0.0f ? g_cursorScale->value : 1.0f;

	g_cx += ( -relyaw / myaw ) * scale;
	g_cy += ( relpitch / mpitch ) * scale;
	ClampCursor();

	float px, py;
	if( ToPage( g_cx, g_cy, px, py ) )
	{
		GlGuard guard;
		g_api.mouse_move( g_view, px, py );
	}
}

void ServoUI_MouseButton( int button, bool down )
{
	float px, py;
	if( !ToPage( g_cx, g_cy, px, py ) )
		return;
	GlGuard guard;
	g_api.mouse_button( g_view, button, down ? 1 : 0, px, py );
}

void ServoUI_LoadURL( char *url )
{
	GlGuard guard;
	g_api.load_url( g_view, url);
}

static void Wheel( float dy )
{
	float px, py;
	if( !ToPage( g_cx, g_cy, px, py ) )
		return;
	GlGuard guard;
	g_api.wheel( g_view, 0.0f, dy, px, py );
}

static int SpecialKey( int key )
{
	switch( key )
	{
	case K_ENTER: case K_KP_ENTER: return 1;
	case K_BACKSPACE: return 2;
	case K_TAB: return 3;
	case K_UPARROW: return 5;
	case K_DOWNARROW: return 6;
	case K_LEFTARROW: return 7;
	case K_RIGHTARROW: return 8;
	case K_HOME: return 9;
	case K_END: return 10;
	case K_PGUP: return 11;
	case K_PGDN: return 12;
	case K_INS: return 13;
	case K_DEL: return 14;
	}
	return 0;
}

// Engine key numbers are lowercase ASCII for printable keys. US layout only.
static uint32_t PrintableKey( int key )
{
	if( key < 32 || key > 126 )
		return 0;

	if( key >= 'a' && key <= 'z' )
		return ( g_shift != g_caps ) ? (uint32_t)( key - 'a' + 'A' ) : (uint32_t)key;

	if( g_shift )
	{
		static const char from[] = "`1234567890-=[]\\;',./";
		static const char to[]   = "~!@#$%^&*()_+{}|:\"<>?";
		for( int i = 0; from[i]; i++ )
			if( from[i] == key )
				return (uint32_t)to[i];
	}
	return (uint32_t)key;
}

bool ServoUI_Key( int down, int key )
{
	// Modifier state is tracked even without focus, so it is right the moment focus starts.
	if( key == K_SHIFT ) g_shift = down != 0;
	else if( key == K_CTRL ) g_ctrl = down != 0;
	else if( key == K_ALT ) g_alt = down != 0;
	else if( key == K_CAPSLOCK && down ) g_caps = !g_caps;

	if( !ServoUI_HasFocus() )
		return false;

	if( key == K_ESCAPE )
	{
		if( down )
			ServoUI_SetFocus( false );
		return true; // swallow Esc so the game menu does not open
	}

	if( key == K_SHIFT || key == K_CTRL || key == K_ALT || key == K_CAPSLOCK )
		return true;

	if( key == K_MWHEELUP || key == K_MWHEELDOWN )
	{
		if( down )
			Wheel( key == K_MWHEELUP ? 3.0f : -3.0f ); // positive = scroll up
		return true;
	}

	uint32_t mods = ( g_shift ? 1u : 0u ) | ( g_ctrl ? 2u : 0u ) | ( g_alt ? 4u : 0u );
	uint32_t cp = PrintableKey( key );
	int special = cp ? 0 : SpecialKey( key );

	if( cp || special )
	{
		GlGuard guard;
		g_api.key( g_view, down ? 1 : 0, cp, special, mods );
	}
	return true; // everything is swallowed while the page has focus
}

#endif // _CS16CLIENT_ENABLE_SERVO_HTML