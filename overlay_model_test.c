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

#include "teapot.h"

// Include OpenVR header so we can interact with VR stuff.
#undef EXTERN_C
#include "openvr_capi.h"


// The width/height of the overlay.
#define WIDTH  1024
#define HEIGHT 512
float overlayscale = 0.25; // In meters.

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

HmdMatrix34_t InvertHmdMatrix34( HmdMatrix34_t mtoinv )
{
	int i, j;
	HmdMatrix34_t out = { 0 };
	for( i = 0; i < 3; i++ )
		for( j = 0; j < 3; j++ )
			out.m[j][i] = mtoinv.m[i][j];

	for ( i = 0; i < 3; i++ )
	{
		out.m[i][3] = 0;
		for( j = 0; j < 3; j++ )
			out.m[i][3] += out.m[i][j] * -mtoinv.m[j][3];
	}
	return out;
}

HmdMatrix34_t MatrixMultiply( HmdMatrix34_t fin1, HmdMatrix34_t fin2 )
{
	HmdMatrix34_t fotmp;
	fotmp.m[0][0] = fin1.m[0][0] * fin2.m[0][0] + fin1.m[0][1] * fin2.m[1][0] + fin1.m[0][2] * fin2.m[2][0] + fin1.m[0][3] * 0;
	fotmp.m[0][1] = fin1.m[0][0] * fin2.m[0][1] + fin1.m[0][1] * fin2.m[1][1] + fin1.m[0][2] * fin2.m[2][1] + fin1.m[0][3] * 0;
	fotmp.m[0][2] = fin1.m[0][0] * fin2.m[0][2] + fin1.m[0][1] * fin2.m[1][2] + fin1.m[0][2] * fin2.m[2][2] + fin1.m[0][3] * 0;
	fotmp.m[0][3] = fin1.m[0][0] * fin2.m[0][3] + fin1.m[0][1] * fin2.m[1][3] + fin1.m[0][2] * fin2.m[2][3] + fin1.m[0][3] * 1;
	fotmp.m[1][0] = fin1.m[1][0] * fin2.m[0][0] + fin1.m[1][1] * fin2.m[1][0] + fin1.m[1][2] * fin2.m[2][0] + fin1.m[1][3] * 0;
	fotmp.m[1][1] = fin1.m[1][0] * fin2.m[0][1] + fin1.m[1][1] * fin2.m[1][1] + fin1.m[1][2] * fin2.m[2][1] + fin1.m[1][3] * 0;
	fotmp.m[1][2] = fin1.m[1][0] * fin2.m[0][2] + fin1.m[1][1] * fin2.m[1][2] + fin1.m[1][2] * fin2.m[2][2] + fin1.m[1][3] * 0;
	fotmp.m[1][3] = fin1.m[1][0] * fin2.m[0][3] + fin1.m[1][1] * fin2.m[1][3] + fin1.m[1][2] * fin2.m[2][3] + fin1.m[1][3] * 1;
	fotmp.m[2][0] = fin1.m[2][0] * fin2.m[0][0] + fin1.m[2][1] * fin2.m[1][0] + fin1.m[2][2] * fin2.m[2][0] + fin1.m[2][3] * 0;
	fotmp.m[2][1] = fin1.m[2][0] * fin2.m[0][1] + fin1.m[2][1] * fin2.m[1][1] + fin1.m[2][2] * fin2.m[2][1] + fin1.m[2][3] * 0;
	fotmp.m[2][2] = fin1.m[2][0] * fin2.m[0][2] + fin1.m[2][1] * fin2.m[1][2] + fin1.m[2][2] * fin2.m[2][2] + fin1.m[2][3] * 0;
	fotmp.m[2][3] = fin1.m[2][0] * fin2.m[0][3] + fin1.m[2][1] * fin2.m[1][3] + fin1.m[2][2] * fin2.m[2][3] + fin1.m[2][3] * 1;
	return fotmp;
}




#define m00 0
#define m01 1
#define m02 2
#define m03 3
#define m10 4
#define m11 5
#define m12 6
#define m13 7
#define m20 8
#define m21 9
#define m22 10
#define m23 11
#define m30 12
#define m31 13
#define m32 14
#define m33 15

void ApplyMatrixToPoint444( float * kout, const float * pin, const float * f )
{
	float ptmp[3];
	ptmp[0] = pin[0] * f[m00] + pin[1] * f[m01] + pin[2] * f[m02] + pin[3] * f[m03];
	ptmp[1] = pin[0] * f[m10] + pin[1] * f[m11] + pin[2] * f[m12] + pin[3] * f[m13];
	ptmp[2] = pin[0] * f[m20] + pin[1] * f[m21] + pin[2] * f[m22] + pin[3] * f[m23];
	kout[3] = pin[0] * f[m30] + pin[1] * f[m31] + pin[2] * f[m32] + pin[3] * f[m33];
	kout[0] = ptmp[0];
	kout[1] = ptmp[1];
	kout[2] = ptmp[2];
}

void ApplyMatrixToPoint( float * pout, const float * pin, const HmdMatrix34_t matt )
{
	float ptmp[2];
	ptmp[0] = pin[0] * matt.m[0][0] + pin[1] * matt.m[0][1] + pin[2] * matt.m[0][2] + matt.m[0][3];
	ptmp[1] = pin[0] * matt.m[1][0] + pin[1] * matt.m[1][1] + pin[2] * matt.m[1][2] + matt.m[1][3];
	pout[2] = pin[0] * matt.m[2][0] + pin[1] * matt.m[2][1] + pin[2] * matt.m[2][2] + matt.m[2][3];
	pout[0] = ptmp[0];
	pout[1] = ptmp[1];
}
void ApplyMatrixToPointRotateOnly( float * pout, const float * pin, const HmdMatrix34_t matt )
{
	float ptmp[2];
	ptmp[0] = pin[0] * matt.m[0][0] + pin[1] * matt.m[0][1] + pin[2] * matt.m[0][2];
	ptmp[1] = pin[0] * matt.m[1][0] + pin[1] * matt.m[1][1] + pin[2] * matt.m[1][2];
	pout[2] = pin[0] * matt.m[2][0] + pin[1] * matt.m[2][1] + pin[2] * matt.m[2][2];
	pout[0] = ptmp[0];
	pout[1] = ptmp[1];
}


void matrix44perspective( float * out, float fovyK, float aspect, float zNear, float zFar )
{
	//float f = 1./tan(fovy * 3.1415926 / 360.0);
	float f = fovyK;
	out[m00] = f/aspect; out[m01] = 0; out[m02] = 0; out[m03] = 0;
	out[m10] = 0; out[m11] = f; out[m12] = 0; out[m13] = 0;
	out[m20] = 0; out[m21] = 0;
	out[m22] = (zFar + zNear)/(zNear - zFar);
	out[m23] = 2*zFar*zNear  /(zNear - zFar);
	out[m30] = 0; out[m31] = 0; out[m32] = -1; out[m33] = 0;
}


void cross3d(float *out, const float *a, const float *b) {
	float o0 = a[1] * b[2] - a[2] * b[1];
	float o1 = a[2] * b[0] - a[0] * b[2];
	out[2] = a[0] * b[1] - a[1] * b[0];
	out[1] = o1;
	out[0] = o0;
}

void sub3d(float *out, const float *a, const float *b) {
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
	out[2] = a[2] - b[2];
}

float mag3d( const float *in )
{
	return sqrt(in[0] * in[0] + in[1] * in[1] + in[2] * in[2]);
}

void normalize3d(float *out, const float *in) {
	float r = ((float)1.) / mag3d(in);
	out[0] = in[0] * r;
	out[1] = in[1] * r;
	out[2] = in[2] * r;
}

double OGGetAbsoluteTime()
{
	static LARGE_INTEGER lpf;
	LARGE_INTEGER li;

	if( !lpf.QuadPart )
	{
		QueryPerformanceFrequency( &lpf );
	}

	QueryPerformanceCounter( &li );
	return (double)li.QuadPart / (double)lpf.QuadPart;
}

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
		oOverlay->CreateOverlay( "model-overlay", "Model Overlay", &overlayID );
		oOverlay->SetOverlayWidthInMeters( overlayID, overlayscale );
		oOverlay->SetOverlayColor( overlayID, 1., .8, .7 );

		// Control texture bounds to control the way the texture is mapped to the overlay.
		VRTextureBounds_t bounds;
		bounds.uMin = 1;
		bounds.uMax = 0;
		bounds.vMin = 0;
		bounds.vMax = 1;
		oOverlay->SetOverlayTextureBounds( overlayID, &bounds );
	}

	oOverlay->SetOverlayFlag( overlayID, VROverlayFlags_SideBySide_Parallel, 1 );

	// Actually show the overlay.
	oOverlay->ShowOverlay( overlayID );

	GLuint overlaytexture;
	{
		// Initialize the texture with junk data.
		uint8_t * myjunkdata = malloc( WIDTH * HEIGHT * 4 );
		int x, y;
		for( y = 0; y < HEIGHT; y++ )
		for( x = 0; x < WIDTH; x++ )
		{
			myjunkdata[ ( x + y * WIDTH ) * 4 + 0 ] = x * 2;
			myjunkdata[ ( x + y * WIDTH ) * 4 + 1 ] = y * 2;
			myjunkdata[ ( x + y * WIDTH ) * 4 + 2 ] = 0;
			myjunkdata[ ( x + y * WIDTH ) * 4 + 3 ] = 255;
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

#if 1
		int eye;
		
		for( eye = 0; eye < 2; eye++ )
		{
			// Setup where "CNFGDrawText" will draw.
			CNFGPenX = 1+WIDTH/2*eye;
			CNFGPenY = 1;
			CNFGTackSegment( CNFGPenX-1, CNFGPenY-1, CNFGPenX + WIDTH/2-2, CNFGPenY-1 );
			CNFGTackSegment( CNFGPenX + WIDTH/2-2, CNFGPenY-1, CNFGPenX + WIDTH/2-2, CNFGPenY+HEIGHT-2  );
			CNFGTackSegment( CNFGPenX + WIDTH/2-2, CNFGPenY+HEIGHT-2, CNFGPenX -1, CNFGPenY+HEIGHT-2 );
			CNFGTackSegment( CNFGPenX-1, CNFGPenY+HEIGHT-2, CNFGPenX -1, CNFGPenY-1 );

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
						CNFGPenX = 1+WIDTH/2*eye;
						CNFGPenY = devices * 12 + 13;
						CNFGDrawText( str, 2 );
						devices++;
					}
				}			
			}
		}
#endif


		int overlayAssociated = false;
		TrackedDeviceIndex_t index;
		index = oSystem->GetTrackedDeviceIndexForControllerRole( ETrackedControllerRole_TrackedControllerRole_LeftHand );

		if( index == k_unTrackedDeviceIndexInvalid || index == k_unTrackedDeviceIndex_Hmd )
		{
			printf( "Couldn't find your controller to attach our overlay to (%d)\n", index );
		}
		else
		{
			float RWIDTH = WIDTH/2;
			float RHEIGHT = HEIGHT;
			overlayAssociated = true;

			EVROverlayError err;
			struct HmdMatrix34_t eyeLoc[2] = {
				oSystem->GetEyeToHeadTransform( EVREye_Eye_Left ), 
				oSystem->GetEyeToHeadTransform( EVREye_Eye_Right )
			};

			// Get correct time offset for poses.
			float last_vsync_time;
			oSystem->GetTimeSinceLastVsync( &last_vsync_time, 0 );
			const float display_frequency = oSystem->GetFloatTrackedDeviceProperty( k_unTrackedDeviceIndex_Hmd, ETrackedDeviceProperty_Prop_DisplayFrequency_Float, 0 );
			const float frame_period = 1.f / display_frequency;
			const float vsync_to_photons = oSystem->GetFloatTrackedDeviceProperty( k_unTrackedDeviceIndex_Hmd, ETrackedDeviceProperty_Prop_SecondsFromVsyncToPhotons_Float, 0 );
			const float predicted_time = frame_period * 3 - last_vsync_time + vsync_to_photons;

			// Get all tracked devices for when we think our frame is likely to land.
			TrackedDevicePose_t poses[64];
			oSystem->GetDeviceToAbsoluteTrackingPose( ETrackingUniverseOrigin_TrackingUniverseRawAndUncalibrated, predicted_time, poses, 64 );
			struct HmdMatrix34_t ctrl = poses[index].mDeviceToAbsoluteTracking;
			struct HmdMatrix34_t head = poses[0].mDeviceToAbsoluteTracking;
			float put_thing_at[3] =  { ctrl.m[0][3], ctrl.m[1][3], ctrl.m[2][3] };
			float head_location[3] = { head.m[0][3], head.m[1][3], head.m[2][3] };
			float object_to_head[3];
			float object_to_head_full_mag[3];
			sub3d( object_to_head_full_mag, head_location, put_thing_at );
			normalize3d( object_to_head, object_to_head_full_mag );

//			printf( "%f %f %f\n", put_thing_at[0], put_thing_at[1], put_thing_at[2] );

			// Point square at head - Make HMD up real up, to keep eyes parallel in view space.
			float up[3] = { 0, 1, 0 };
			ApplyMatrixToPointRotateOnly( up, up, head );
			float object_vector_up[3];
			float object_vector_right[3];
			cross3d( object_vector_right, object_to_head, up );
			cross3d( object_vector_up, object_vector_right, object_to_head );
			normalize3d( object_vector_up, object_vector_up );
			normalize3d( object_vector_right, object_vector_right );
			//Makes a coordinate frame.
			struct HmdMatrix34_t absXForm;

			absXForm.m[0][3] = ctrl.m[0][3];
			absXForm.m[1][3] = ctrl.m[1][3];
			absXForm.m[2][3] = ctrl.m[2][3];
			absXForm.m[0][0] = object_vector_right[0];
			absXForm.m[1][0] = object_vector_right[1];
			absXForm.m[2][0] = object_vector_right[2];
			absXForm.m[0][1] = object_vector_up[0];
			absXForm.m[1][1] = object_vector_up[1];
			absXForm.m[2][1] = object_vector_up[2];
			absXForm.m[0][2] = object_to_head[0];
			absXForm.m[1][2] = object_to_head[1];
			absXForm.m[2][2] = object_to_head[2];
			
			//This now has a plane pointing at the center of your eyes.
#if 0
			printf( "%f %f %f / %f %f %f / %f %f %f / %f %f %f\n",
				object_vector_right[0], object_vector_right[1], object_vector_right[2],
				object_vector_up[0], object_vector_up[1], object_vector_up[2],
				object_to_head[0], object_to_head[1], object_to_head[2],
				ctrl.m[0][3], ctrl.m[1][3], ctrl.m[2][3] );
#endif

			int eye;
			for( eye = 0; eye < 2; eye++ )
			{
				HmdMatrix34_t worldEyeLoc = MatrixMultiply( head, eyeLoc[eye] );
				float eye_location[3] = { worldEyeLoc.m[0][3], worldEyeLoc.m[1][3], worldEyeLoc.m[2][3] };
				float put_thing_at[3] =  { ctrl.m[0][3], ctrl.m[1][3], ctrl.m[2][3] };
				float object_to_head[3];
				float object_to_head_full_mag[3];
				sub3d( object_to_head_full_mag, eye_location, put_thing_at );
				normalize3d( object_to_head, object_to_head_full_mag );

				float fovyK = mag3d( object_to_head_full_mag ) / overlayscale;
				//1./tan(fovy * 3.1415926 / 360.0)
				float aspect = 1;
				float zNear = .01;
				float zFar = 200;
				float perspective[16];
				matrix44perspective( perspective, fovyK, aspect, zNear, zFar );

				// Point square at head - Make HMD up real up, to keep eyes parallel in view space.
				float up[3] = { 0, 1, 0 };
				ApplyMatrixToPointRotateOnly( up, up, head );
				float object_vector_up[3];
				float object_vector_right[3];
				cross3d( object_vector_right, object_to_head, up );
				cross3d( object_vector_up, object_vector_right, object_to_head );
				normalize3d( object_vector_up, object_vector_up );
				normalize3d( object_vector_right, object_vector_right );
				//Makes a coordinate frame.
				struct HmdMatrix34_t absXFormTE;

				absXFormTE.m[0][3] = eye_location[0];
				absXFormTE.m[1][3] = eye_location[1];
				absXFormTE.m[2][3] = eye_location[2];
				absXFormTE.m[0][0] = object_vector_right[0];
				absXFormTE.m[1][0] = object_vector_right[1];
				absXFormTE.m[2][0] = object_vector_right[2];
				absXFormTE.m[0][1] = object_vector_up[0];
				absXFormTE.m[1][1] = object_vector_up[1];
				absXFormTE.m[2][1] = object_vector_up[2];
				absXFormTE.m[0][2] = object_to_head[0];
				absXFormTE.m[1][2] = object_to_head[1];
				absXFormTE.m[2][2] = object_to_head[2];
			
			
				// Need to make a matrix to invert the object to eye in worldspace.
				HmdMatrix34_t mat_to_inv = absXFormTE;
				HmdMatrix34_t modelview = InvertHmdMatrix34( mat_to_inv );
//				modelview.m[0][3] += -eye_location[0];
//				modelview.m[1][3] += -eye_location[1];
//				modelview.m[2][3] += -eye_location[2];
			//	mat_to_inv.m[0][0] = 1; mat_to_inv.m[1][0] = 0; mat_to_inv.m[2][0] = 0;
			//	mat_to_inv.m[0][1] = 0; mat_to_inv.m[1][1] = 1; mat_to_inv.m[2][1] = 0;
			//	mat_to_inv.m[0][2] = 0; mat_to_inv.m[1][2] = 0; mat_to_inv.m[2][2] = 1;
//				modelview.m[0][3] = -eye_location[0];
//				modelview.m[1][3] = -eye_location[1];
//				modelview.m[2][3] = -eye_location[2];
				//printf( "%f %f %f -> %f %f %f\n", mat_to_inv.m[0][3], mat_to_inv.m[1][3], mat_to_inv.m[2][3], modelview.m[0][3], modelview.m[1][3], modelview.m[2][3] );
				
				
				//HmdMatrix34_t modelview = ( mat_to_inv );
				
				
				int i;
				// Skip over a bunch of vertices.
				for( i = 0; i < sizeof( teapotIndices ) / sizeof( unsigned int); i+=3 )
				{
					int i1 = teapotIndices[((i+0)%3)+(i/3)*3];
					int i2 = teapotIndices[((i+1)%3)+(i/3)*3];
					const float * pt1 = &teapotPositions[i1*3];
					const float * pt2 = &teapotPositions[i2*3];

					float applied_pt1[4];
					float output_pt1[4];
					float applied_pt2[4];
					float output_pt2[4];
					float scale = 0.5;
					float enpt = 1;
					
					float ptloc1[3];
					float ptlocal1[3] = { pt1[0]*scale, pt1[1]*scale, pt1[2]*scale };
					ApplyMatrixToPoint( ptloc1, ptlocal1, ctrl );

					float ptloc2[3];
					float ptlocal2[3] = { pt2[0]*scale, pt2[1]*scale, pt2[2]*scale };
					ApplyMatrixToPoint( ptloc2, ptlocal2, ctrl );

					ApplyMatrixToPoint( applied_pt1, ptloc1, modelview );
					ApplyMatrixToPoint( applied_pt2, ptloc2, modelview );
					applied_pt1[3] = 1;
					applied_pt2[3] = 1;
					ApplyMatrixToPoint444( output_pt1, applied_pt1, perspective );
					ApplyMatrixToPoint444( output_pt2, applied_pt2, perspective );
					
					output_pt1[3] *= -1; // HMMM
					output_pt2[3] *= -1; // HMMM
					
					output_pt1[0] /= output_pt1[3];
					output_pt1[1] /= output_pt1[3];
					output_pt1[2] /= output_pt1[3];
					output_pt1[3] /= output_pt1[3];
					output_pt2[0] /= output_pt2[3];
					output_pt2[1] /= output_pt2[3];
					output_pt2[2] /= output_pt2[3];
					output_pt2[3] /= output_pt2[3];

					float clip_space1[2];
					float clip_space2[2];
					clip_space1[0] = ( output_pt1[0] + 1 ) * ( RWIDTH / 2.0 );
					clip_space1[1] = ( output_pt1[1] + 1 ) * ( RHEIGHT / 2.0 );
					clip_space2[0] = ( output_pt2[0] + 1 ) * ( RWIDTH / 2.0 );
					clip_space2[1] = ( output_pt2[1] + 1 ) * ( RHEIGHT / 2.0 );
					if( i < 0 )
					{
						printf( "%d  %f %f %f -> %f %f %f -> %f %f %f %f -> %f %f\n", i, ptloc1[0], ptloc1[1], ptloc1[2], applied_pt1[0], applied_pt1[1], applied_pt1[2], 
						output_pt1[0], output_pt1[1], output_pt1[2], output_pt1[3],  clip_space1[0], clip_space1[1] );
					}
					if( clip_space1[0] < 0 || clip_space1[0] >= RWIDTH ) continue;
					if( clip_space2[0] < 0 || clip_space2[0] >= RWIDTH ) continue;
					CNFGTackSegment( clip_space1[0] + (!eye)*RWIDTH, clip_space1[1], clip_space2[0] + (!eye)*RWIDTH, clip_space2[1] );
				}
			}

//			printf( "%f %f %f - ", head.m[0][3], head.m[1][3], head.m[2][3] );
//			printf( "%f %f %f\n", headeL.m[0][3], headeL.m[1][3], headeL.m[2][3] );

			err = oOverlay->SetOverlayTransformAbsolute( overlayID, index, &absXForm );
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

		// Do not flip frames, otherwise we will lock to 2D FPS.
		//CNFGSwapBuffers();

		framenumber++;
		
		// Don't go at 1,000+ FPS, wait til next frame sync from scene app.
		oOverlay->WaitFrameSync( 100 );
	}

	return 0;
}