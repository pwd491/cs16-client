#pragma once

// Servo web view glue for the CS16Client HUD. Compiled only with _CS16CLIENT_ENABLE_SERVO_HTML.
// All functions must be called from the engine's main (GL) thread.

void ServoUI_Init();
bool ServoUI_Create( int w, int h, int posX, int posY, const char *url );
void ServoUI_Destroy();
void ServoUI_Shutdown();
bool ServoUI_Active();

// Call every frame from HUD_Frame: pumps Servo and uploads a new frame if there is one.
void ServoUI_Frame();

// Call from HUD_Redraw: draws the page into the given screen rectangle (and the cursor in focus mode).
void ServoUI_Draw( float x1, float y1, float x2, float y2 );

// --- Input -----------------------------------------------------------------
// Focus mode: the page gets the mouse and keyboard, the game does not.
// Toggled by the console command `servo_focus`; Esc leaves it.
bool ServoUI_HasFocus();
void ServoUI_SetFocus( bool focus );

// HUD_Key_Event hook. Returns true if the key was consumed (HUD_Key_Event must then return 0).
bool ServoUI_Key( int down, int keynum );

// IN_ClientLookEvent hook: relative mouse movement (engine units) moves the virtual cursor.
void ServoUI_LookEvent( float relyaw, float relpitch );

// IN_MouseEvent hook: button index 0..4, uses the virtual cursor position.
void ServoUI_MouseButton( int button, bool down );

// Load URL by the calling function name
void ServoUI_LoadURL( char *url );