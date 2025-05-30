/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// cl_util.h
//
#pragma once
#include "cvardef.h"
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

extern cvar_t *hud_textmode;

#ifdef _MSC_VER
#pragma warning(disable : 4244) // 'argument': conversion from 'float' to 'int', possible loss of data
#pragma warning(disable : 4101) // unreferenced local variable
#endif

// Macros to hook function calls into the HUD object
#define HOOK_MESSAGE_FUNC(name, func) gEngfuncs.pfnHookUserMsg(name, [](const char *pszName, int iSize, void *pbuf) \
{\
	return func(pszName, iSize, pbuf);\
})
#define HOOK_MESSAGE(obj, name) HOOK_MESSAGE_FUNC( #name, obj.MsgFunc_##name )

#define HOOK_COMMAND_FUNC(str, func, ...) gEngfuncs.pfnAddCommand( str, [](void) { func(__VA_ARGS__); })
#define HOOK_COMMAND(obj, str, func) HOOK_COMMAND_FUNC( str, obj.UserCmd_##func, )

inline float CVAR_GET_FLOAT( const char *x ) {	return gEngfuncs.pfnGetCvarFloat( (char*)x ); }
inline char* CVAR_GET_STRING( const char *x ) {	return gEngfuncs.pfnGetCvarString( (char*)x ); }
inline struct cvar_s *CVAR_CREATE( const char *cv, const char *val, const int flags ) {	return gEngfuncs.pfnRegisterVariable( (char*)cv, (char*)val, flags ); }

#define SPR_Load (*gEngfuncs.pfnSPR_Load)
#define SPR_Set (*gEngfuncs.pfnSPR_Set)
#define SPR_Frames (*gEngfuncs.pfnSPR_Frames)
#define SPR_GetList (*gEngfuncs.pfnSPR_GetList)

// SPR_Draw  draws a the current sprite as solid
#define SPR_Draw (*gEngfuncs.pfnSPR_Draw)
// SPR_DrawHoles  draws the current sprites,  with color index255 not drawn (transparent)
#define SPR_DrawHoles (*gEngfuncs.pfnSPR_DrawHoles)
// SPR_DrawAdditive  adds the sprites RGB values to the background  (additive transulency)
#define SPR_DrawAdditive (*gEngfuncs.pfnSPR_DrawAdditive)

// SPR_EnableScissor  sets a clipping rect for HUD sprites.  (0,0) is the top-left hand corner of the screen.
#define SPR_EnableScissor (*gEngfuncs.pfnSPR_EnableScissor)
// SPR_DisableScissor  disables the clipping rect
#define SPR_DisableScissor (*gEngfuncs.pfnSPR_DisableScissor)
//
#define FillRGBA (*gEngfuncs.pfnFillRGBA)
#define FillRGBABlend (*gEngfuncs.pfnFillRGBABlend)


// ScreenHeight returns the height of the screen, in pixels
#define ScreenHeight (gHUD.m_scrinfo.iHeight)
// ScreenWidth returns the width of the screen, in pixels
#define ScreenWidth (gHUD.m_scrinfo.iWidth)

#define TrueHeight (gHUD.m_truescrinfo.iHeight)
#define TrueWidth (gHUD.m_truescrinfo.iWidth)

// Use this to set any co-ords in 640x480 space
#define XRES(x)		((int)(float(x)  * ((float)ScreenWidth / 640.0f) + 0.5f))
#define YRES(y)		((int)(float(y)  * ((float)ScreenHeight / 480.0f) + 0.5f))

// use this to project world coordinates to screen coordinates
#define XPROJECT(x)	( (1.0f+(x))*ScreenWidth*0.5f )
#define YPROJECT(y) ( (1.0f-(y))*ScreenHeight*0.5f )

#define GetScreenInfo (*gEngfuncs.pfnGetScreenInfo)
#define ServerCmd (*gEngfuncs.pfnServerCmd)
#define ClientCmd (*gEngfuncs.pfnClientCmd)
#define FilteredClientCmd (*gEngfuncs.pfnFilteredClientCmd)
#define AngleVectors (*gEngfuncs.pfnAngleVectors)
#define Com_RandomLong (*gEngfuncs.pfnRandomLong)
#define Com_RandomFloat (*gEngfuncs.pfnRandomFloat)

extern float color[3]; // hud.cpp


// Gets the height & width of a sprite,  at the specified frame
inline int SPR_Height( HSPRITE x, int f = 0 )
{
	return gEngfuncs.pfnSPR_Height(x, f);
}
inline int SPR_Width( HSPRITE x, int f = 0 )
{
	return gEngfuncs.pfnSPR_Width(x, f);
}

inline client_textmessage_t *TextMessageGet( const char *pName )
{
	return gEngfuncs.pfnTextMessageGet( pName );
}

inline void ConsolePrint( const char *string )
{
	gEngfuncs.pfnConsolePrint( string );
}

inline void CenterPrint( const char *string )
{
	gEngfuncs.pfnCenterPrint( string );
}

// returns the players name of entity no.
#define GetPlayerInfo (*gEngfuncs.pfnGetPlayerInfo)

// sound functions
inline void PlaySound( const char *szSound, float vol ) { gEngfuncs.pfnPlaySoundByName( szSound, vol ); }
inline void PlaySound( int iSound, float vol ) { gEngfuncs.pfnPlaySoundByIndex( iSound, vol ); }

#define max(a, b)  (((a) > (b)) ? (a) : (b))
#define min(a, b)  (((a) < (b)) ? (a) : (b))
#if !defined(__APPLE__) && !defined(_WIN32)
#define fabs(x)	   ((x) > 0 ? (x) : 0 - (x))
#endif
#define DotProduct(x,y) ((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define VectorSubtract(a,b,c) {(c)[0]=(a)[0]-(b)[0];(c)[1]=(a)[1]-(b)[1];(c)[2]=(a)[2]-(b)[2];}
#define VectorAdd(a,b,c) {(c)[0]=(a)[0]+(b)[0];(c)[1]=(a)[1]+(b)[1];(c)[2]=(a)[2]+(b)[2];}
#define VectorCopy(a,b) {(b)[0]=(a)[0];(b)[1]=(a)[1];(b)[2]=(a)[2];}
inline void VectorClear(float *a) { a[0]=0.0;a[1]=0.0;a[2]=0.0;}
#define DotProduct(x,y) ((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define VectorLength(a) ( sqrt( DotProduct( a, a )))
#define VectorMA(a, scale, b, c) ((c)[0] = (a)[0] + (scale) * (b)[0],(c)[1] = (a)[1] + (scale) * (b)[1],(c)[2] = (a)[2] + (scale) * (b)[2])
#define VectorScale(in, scale, out) ((out)[0] = (in)[0] * (scale),(out)[1] = (in)[1] * (scale),(out)[2] = (in)[2] * (scale))
float VectorNormalize (float *v);
#define VectorInverse(x) ((x)[0] = -(x)[0], (x)[1] = -(x)[1], (x)[2] = -(x)[2])

extern float vec3_origin[3];

#ifdef _MSC_VER
// disable 'possible loss of data converting float to int' warning message
#pragma warning( disable: 4244 )
// disable 'truncation from 'const double' to 'float' warning message
#pragma warning( disable: 4305 )
#endif

float *GetClientColor( int clientIndex );
inline HSPRITE LoadSprite(const char *pszName)
{
	char sz[256];
	snprintf(sz, 256, pszName, 640);

	return SPR_Load(sz);
}

extern vec3_t g_ColorRed, g_ColorBlue, g_ColorYellow, g_ColorGrey, g_ColorGreen;

inline void GetTeamColor( int &r, int &g, int &b, int teamIndex )
{
	r = 255;
	g = 255;
	b = 255;
	switch( teamIndex )
	{
	case TEAM_TERRORIST:
		r *= g_ColorRed[0];
		g *= g_ColorRed[1];
		b *= g_ColorRed[2];
		break;
	case TEAM_CT:
		r *= g_ColorBlue[0];
		g *= g_ColorBlue[1];
		b *= g_ColorBlue[2];
		break;
	case TEAM_SPECTATOR:
	case TEAM_UNASSIGNED:
		r *= g_ColorYellow[0];
		g *= g_ColorYellow[1];
		b *= g_ColorYellow[2];
		break;
	default:
		r *= g_ColorGrey[0];
		g *= g_ColorGrey[1];
		b *= g_ColorGrey[2];
		break;
	}
}

#define bound( min, num, max ) ((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))
#define RAD2DEG( x )	((float)(x) * (float)(180.f / M_PI))
#define DEG2RAD( x )	((float)(x) * (float)(M_PI / 180.f))

enum
{
	GAME_CSTRIKE = 0, // 1.6
	GAME_CZERO = 1,   // czero
	GAME_CZERODS = 2  // ritual czero
};
