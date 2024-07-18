// Battery Watcher for OpenVR (make an overlay with the battery left for all connected devices on your left hand)
//
// Compile with:
//  tcc battery_monitor_overlay.c -o bmo.exe -luser32 -lgdi32 -lkernel32 -lopengl32 C:\windows\system32\msvcrt.dll openvr_api.dll

// System headers for any extra stuff we need.
#include <stdbool.h>

// Include CNFG (rawdraw) for generating a window and/or OpenGL context.
#define CNFG_IMPLEMENTATION
#define CNFGOGL
#include "rawdraw_sf.h"

// Include OpenVR header so we can interact with VR stuff.
#undef EXTERN_C
#include "openvr_capi.h"


// OpenVR Doesn't define these for some reason (I don't remmeber why) so we define the functions here. They are copy-pasted from the bottom of openvr_capi.h
intptr_t VR_InitInternal( EVRInitError *peError, EVRApplicationType eType );
void VR_ShutdownInternal();
bool VR_IsHmdPresent();
intptr_t VR_GetGenericInterface( const char *pchInterfaceVersion, EVRInitError *peError );
bool VR_IsRuntimeInstalled();
const char * VR_GetVRInitErrorAsSymbol( EVRInitError error );
const char * VR_GetVRInitErrorAsEnglishDescription( EVRInitError error );

// These are functions that rawdraw calls back into.
void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { }
void HandleMotion( int x, int y, int mask ) { }
void HandleDestroy() { }

// This function was copy-pasted from cnovr.
void * CNOVRGetOpenVRFunctionTable( const char * interfacename )
{
	EVRInitError e;
	char fnTableName[128];
	int result1 = snprintf( fnTableName, 128, "FnTable:%s", interfacename );
	void * ret = (void *)VR_GetGenericInterface( fnTableName, &e );
	printf( "Getting System FnTable: %s = %p (%d)\n", fnTableName, ret, e );
	if( !ret )
	{
		exit( 1 );
	}
	return ret;
}

// These are interfaces into OpenVR, they are basically function call tables.
struct VR_IVRSystem_FnTable * oSystem;
struct VR_IVROverlay_FnTable * oOverlay;

// The OpenVR Overlay handle.
VROverlayHandle_t overlayID;

// Was the overlay assocated or not?
int overlayAssociated;

// The width/height of the overlay.
#define WIDTH 128
#define HEIGHT 128

int main()
{
	// Create the window, needed for making an OpenGL context, but also
	// gives us a framebuffer we can draw into.  Minus signs in front of 
	// width/height hint to rawdrawthat we want a hidden window.
	CNFGSetup( "Example App", -WIDTH, -HEIGHT );

	// We put this in a codeblock because it's logically together.
	// no reason to keep the token around.
	{
		EVRInitError ierr;
		uint32_t token = VR_InitInternal( &ierr, EVRApplicationType_VRApplication_Overlay );
		if( !token )
		{
			printf( "Error!!!! Could not initialize OpenVR\n" );
			return -5;
		}

		// Get the system and overlay interfaces.  We pass in the version of these
		// interfaces that we wish to use, in case the runtime is newer, we can still
		// get the interfaces we expect.
		oSystem = CNOVRGetOpenVRFunctionTable( IVRSystem_Version );
		oOverlay = CNOVRGetOpenVRFunctionTable( IVROverlay_Version );
	}

	{
		// Generate the overlay.
		oOverlay->CreateOverlay( "batterymonitoroverlay-overlay", "Battery Monitor Overlay", &overlayID );
		oOverlay->SetOverlayWidthInMeters( overlayID, .1 );
		oOverlay->SetOverlayColor( overlayID, 1., .8, .7 );

		// Control texture bounds to control the way the texture is mapped to the overlay.
		VRTextureBounds_t bounds;
		bounds.uMin = 1;
		bounds.uMax = 0;
		bounds.vMin = 0;
		bounds.vMax = 1;
		oOverlay->SetOverlayTextureBounds( overlayID, &bounds );
	}

	// Actually show the overlay.
	oOverlay->ShowOverlay( overlayID );

	GLuint overlaytexture;
	{
		// Initialize the texture with junk data.
		uint8_t * myjunkdata = malloc( 128 * 128 * 4 );
		int x, y;
		for( y = 0; y < 128; y++ )
		for( x = 0; x < 128; x++ )
		{
			myjunkdata[ ( x + y * 128 ) * 4 + 0 ] = x * 2;
			myjunkdata[ ( x + y * 128 ) * 4 + 1 ] = y * 2;
			myjunkdata[ ( x + y * 128 ) * 4 + 2 ] = 0;
			myjunkdata[ ( x + y * 128 ) * 4 + 3 ] = 255;
		}
		
		// We aren't doing it, but we could write directly into the overlay.
		//err = oOverlay->SetOverlayRaw( overlayID, myjunkdata, 128, 128, 4 );
		
		// Generate the texture.
		glGenTextures( 1, &overlaytexture );
		glBindTexture( GL_TEXTURE_2D, overlaytexture );

		// It is required to setup the min and mag filter of the texture.
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		
		// Load the texture with our dummy data.  Optionally we could pass 0 in where we are
		// passing in myjunkdata. That would allocate the RAM on the GPU but not do anything with it.
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, WIDTH, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, myjunkdata );
	}

	int framenumber;

	while( true )
	{
		CNFGBGColor = 0x00000000; //Black Transparent Background
		CNFGClearFrame();
		
		// Process any window events and call callbacks.
		CNFGHandleInput();

		// Setup draw color.
		CNFGColor( 0xffffffff ); 
		
		// Setup where "CNFGDrawText" will draw.
		CNFGPenX = 1;
		CNFGPenY = 1;

		// Scratch buffer for us to write text into.
		char str[256];
		sprintf( str, "%d\n", framenumber );
		
		// Actually draw the string.
		CNFGDrawText( str, 2 );

		// Iterate over the list of all devices.
		int i;
		int devices = 0;
		for( i = 0; i < k_unMaxTrackedDeviceCount; i++ )
		{
			// See if this device has a battery charge.
			ETrackedDeviceProperty prop;
			ETrackedPropertyError err;
			float battery = oSystem->GetFloatTrackedDeviceProperty( i, ETrackedDeviceProperty_Prop_DeviceBatteryPercentage_Float, &err );
			
			// No error? Proceed.
			if( err == 0 )
			{
				// Get the device name
				char ctrlname[128];
				oSystem->GetStringTrackedDeviceProperty( i, ETrackedDeviceProperty_Prop_ModelNumber_String, ctrlname, 128, &err );
				
				// If the device name is nonempty...
				if( strlen( ctrlname ) )
				{
					// Write into the scratch string.
					// Set the draw location, and draw that string.
					sprintf( str, "%4.0f %s", battery * 100., ctrlname );
					CNFGPenX = 1;
					CNFGPenY = devices * 12 + 13;
					CNFGDrawText( str, 2 );
					devices++;
				}
			}			
		}

		// If the overlay is unassociated, associate it with the left controller.
		if( !overlayAssociated )
		{
			TrackedDeviceIndex_t index;
			index = oSystem->GetTrackedDeviceIndexForControllerRole( ETrackedControllerRole_TrackedControllerRole_LeftHand );
			if( index == k_unTrackedDeviceIndexInvalid || index == k_unTrackedDeviceIndex_Hmd )
			{
				printf( "Couldn't find your controller to attach our overlay to (%d)\n", index );
			}
			else
			{
				// We have a ETrackedControllerRole_TrackedControllerRole_LeftHand.  Associate it.
				EVROverlayError err;

				// Transform that puts the text somewhere reasonable.
				HmdMatrix34_t transform = { 0 };
				transform.m[1][1] = 1;
				transform.m[0][2] = 1;
				transform.m[2][0] = 1;

				// Apply the transform and attach the overlay to that tracked device object.
				err = oOverlay->SetOverlayTransformTrackedDeviceRelative( overlayID, index, &transform );

				// Notify the terminal that this was associated.
				printf( "Successfully associated your battery status window to the tracked device (%d %d %08x).\n",
					 err, index, overlayID );

				overlayAssociated = true;
			}
		}

		// Finish rendering any pending draw operations.
		CNFGFlushRender();

		// Bind the texture we will be sending to OpenVR.
		glBindTexture( GL_TEXTURE_2D, overlaytexture );
		
		// Copy the current framebuffer into that texture.
		glCopyTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, WIDTH, HEIGHT, 0 );

		// Setup a Texture_t object to send in the texture.
		struct Texture_t tex;
		tex.eColorSpace = EColorSpace_ColorSpace_Auto;
		tex.eType = ETextureType_TextureType_OpenGL;
		tex.handle = (void*)(intptr_t)overlaytexture;

		// Send texture into OpenVR as the overlay.
		oOverlay->SetOverlayTexture( overlayID, &tex );

		// We have to process through texture events.
		struct VREvent_t nEvent;
		if( overlayAssociated )
		{
			while( oOverlay->PollNextOverlayEvent( overlayID, &nEvent, sizeof( nEvent ) ) );
		}

		// Don't swap buffers, otherwise we will lock to host FPS.
		//CNFGSwapBuffers();

		framenumber++;
		
		// Don't go at 1,000+ FPS, wait til next frame sync from scene app.
		oOverlay->WaitFrameSync( 100 );
	}

	return 0;
}