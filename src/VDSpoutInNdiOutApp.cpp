/*
 Copyright (c) 2013-2022, Bruce Lane - All rights reserved.
 This code is intended for use with the Cinder C++ library: http://libcinder.org

 Using Cinder-Warping from Paul Houx.

 Cinder-Warping is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Cinder-Warping is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Cinder-Warping.  If not, see <http://www.gnu.org/licenses/>.
 */
 
#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

// Animation
#include "VDAnimation.h"
// Session Facade
#include "VDSessionFacade.h"
// Spout (Windows only)
// TODO: Mac equivalent (Syphon server output) not implemented yet
#if defined( CINDER_MSW )
#include "CiSpoutIn.h"
#endif
// NDI (Windows only, see Cinder-NDI/cinderblock.xml's <supports os="msw"/>)
// TODO: Mac equivalent not implemented yet (same gap as Syphon server output above)
#if defined( CINDER_MSW )
#include "CinderNDISender.h"
#endif
// Uniforms
#include "VDUniforms.h"
// Params
#include "VDParams.h"
// Mix
#include "VDMix.h"

// UI
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS 1
#include "VDUI.h"
// once per exe: dedicated GPU exports, process priority, 1 ms timer
#include "VDLivePriority.h"
// imgui.h already defines this (pulled in transitively) - redefining it unconditionally
// triggers "warning C4005: redéfinition de macro" on every build
#ifndef IM_ARRAYSIZE
#define IM_ARRAYSIZE(_ARR)			((int)(sizeof(_ARR)/sizeof(*_ARR)))
#endif
using namespace ci;
using namespace ci::app;
using namespace videodromm;

class VDSpoutInNdiOutApp : public App {
public:
	VDSpoutInNdiOutApp();
	void cleanup() override;
	void update() override;
	void draw() override;
	void resize() override;
	void mouseMove(MouseEvent event) override;
	void mouseDown(MouseEvent event) override;
	void mouseDrag(MouseEvent event) override;
	void mouseUp(MouseEvent event) override;
	void keyDown(KeyEvent event) override;
	void keyUp(KeyEvent event) override;
	void fileDrop(FileDropEvent event) override;
	
	#if defined( CINDER_MSW )
		SpoutIn	mSpoutIn;
	#endif
private:
	// Settings
	VDSettingsRef					mVDSettings;
	// Animation
	VDAnimationRef					mVDAnimation;
	// Session
	VDSessionFacadeRef				mVDSessionFacade;
	// Mix
	VDMixRef						mVDMix;
	// Uniforms
	VDUniformsRef					mVDUniforms;
	// Params
	VDParamsRef						mVDParams;
	// UI
	VDUIRef							mVDUI;

	bool							mFadeInDelay = true;
	void							toggleCursorVisibility(bool visible);
#if defined( CINDER_MSW )
	// off by default, toggled with 'n': only created when needed (an existing sender is
	// advertised on the network and every send is a GPU->CPU readback)
	std::unique_ptr<CinderNDISender>	mNdiOut;
#endif
	void							toggleNdi();
	ci::SurfaceRef 			mSurface;
	ci::gl::Texture2dRef	mTexture;
	int								mTrack = 0;
};


VDSpoutInNdiOutApp::VDSpoutInNdiOutApp()
{


	// Settings
	mVDSettings = VDSettings::create("VDUI");
	// Uniform
	mVDUniforms = VDUniforms::create();
	// Params
	mVDParams = VDParams::create();
	// Animation
	mVDAnimation = VDAnimation::create(mVDSettings, mVDUniforms);
	// Mix
	mVDMix = VDMix::create(mVDSettings, mVDAnimation, mVDUniforms);
	// Session
	mVDSessionFacade = VDSessionFacade::createVDSession(mVDSettings, mVDAnimation, mVDUniforms, mVDMix)
		->setUniformValue(mVDUniforms->IDISPLAYMODE, VDDisplayMode::POST)
		->setupSession()
		->setupWSClient()
		->wsConnect()
		//->setupOSCReceiver()
		//->addOSCObserver(mVDSettings->mOSCDestinationHost, mVDSettings->mOSCDestinationPort)
		->addUIObserver(mVDSettings, mVDUniforms)
		->toggleUI()
		->setUniformValue(mVDUniforms->IBPM, 160.0f)
		->setUniformValue(mVDUniforms->IMOUSEY, 0.5648f) // sos only
		->setUniformValue(mVDUniforms->IEXPOSURE, 0.965f)
		//->setUniformValue(mVDUniforms->ISOBEL, 0.999f) // mtrlvr  only 
		//->setUniformValue(mVDUniforms->ITOGGLE, 1.0f) // sos only
		->setUniformValue(mVDUniforms->IWEIGHT0, 1.0f)
		->setUniformValue(mVDUniforms->IWEIGHT1, 1.0f) // yeah tests only0.1
		->setUniformValue(mVDUniforms->IWEIGHT2, 1.0f)
		->setUniformValue(mVDUniforms->IWEIGHT3, 0.0f) // yeah tests only0.2
		->setUniformValue(mVDUniforms->IWEIGHT4, 0.0f);

	mFadeInDelay = true;
	// UI
	mVDUI = VDUI::create(mVDSettings, mVDSessionFacade, mVDUniforms);
	// Initialize ImGui context
	ImGui::Initialize();
	// VDUIParams positions/sizes are authored in logical (1x) units; scale ImGui to match
	// on Retina displays now that the window reports real device pixels.
#if defined( CINDER_MAC )
	float contentScale = getWindow()->getContentScale();
	ImGui::GetStyle().ScaleAllSizes(contentScale);
	ImGui::GetStyle().FontScaleMain = contentScale;
#endif
}

void VDSpoutInNdiOutApp::toggleCursorVisibility(bool visible)
{
	if (visible)
	{
		showCursor();
	}
	else
	{
		hideCursor();
	}
}

void VDSpoutInNdiOutApp::toggleNdi()
{
#if defined( CINDER_MSW )
	if (mNdiOut) mNdiOut.reset();
	else mNdiOut = std::make_unique<CinderNDISender>("VDUI");
	CI_LOG_I("NDI sender " << (mNdiOut ? "on" : "off"));
#endif
}

void VDSpoutInNdiOutApp::fileDrop(FileDropEvent event)
{
	mVDSessionFacade->fileDrop(event);
}

void VDSpoutInNdiOutApp::mouseMove(MouseEvent event)
{
	if (mVDUI->getOutputWindow()->isOutputWindow(event.getWindow())) {
		// only the warp editor listens on the projector output (drag control points on the wall)
		if (mVDUI->getOutputWindow()->getContent() == VDOutputWindow::WARPS) {
			MouseEvent warpEvent = mVDUI->getOutputWindow()->toWarpSpace(event);
			mVDSessionFacade->handleMouseMove(warpEvent);
		}
		return;
	}
	if (!mVDSessionFacade->handleMouseMove(event)) {

	}
}

void VDSpoutInNdiOutApp::mouseDown(MouseEvent event)
{
	if (mVDUI->getOutputWindow()->isOutputWindow(event.getWindow())) {
		// only the warp editor listens on the projector output (drag control points on the wall)
		if (mVDUI->getOutputWindow()->getContent() == VDOutputWindow::WARPS) {
			MouseEvent warpEvent = mVDUI->getOutputWindow()->toWarpSpace(event);
			mVDSessionFacade->handleMouseDown(warpEvent);
		}
		return;
	}
#if defined( CINDER_MSW )
	// right-click outside the UI panels picks the Spout sender (any click used to open it,
	// including clicks meant for the ImGui panels or the warp editor)
	if (event.isRight() && !ImGui::GetIO().WantCaptureMouse) {
		// SpoutPanel.exe must be in the executable folder
		mSpoutIn.getSpoutReceiver().SelectSenderPanel(); // DirectX 11 by default
		return;
	}
#endif
	if (!mVDSessionFacade->handleMouseDown(event)) {
		
	}
}

void VDSpoutInNdiOutApp::mouseDrag(MouseEvent event)
{
	if (mVDUI->getOutputWindow()->isOutputWindow(event.getWindow())) {
		// only the warp editor listens on the projector output (drag control points on the wall)
		if (mVDUI->getOutputWindow()->getContent() == VDOutputWindow::WARPS) {
			MouseEvent warpEvent = mVDUI->getOutputWindow()->toWarpSpace(event);
			mVDSessionFacade->handleMouseDrag(warpEvent);
		}
		return;
	}

	if (!mVDSessionFacade->handleMouseDrag(event)) {

	}
}

void VDSpoutInNdiOutApp::mouseUp(MouseEvent event)
{
	if (mVDUI->getOutputWindow()->isOutputWindow(event.getWindow())) {
		// only the warp editor listens on the projector output (drag control points on the wall)
		if (mVDUI->getOutputWindow()->getContent() == VDOutputWindow::WARPS) {
			MouseEvent warpEvent = mVDUI->getOutputWindow()->toWarpSpace(event);
			mVDSessionFacade->handleMouseUp(warpEvent);
		}
		return;
	}

	if (!mVDSessionFacade->handleMouseUp(event)) {

	}
}

void VDSpoutInNdiOutApp::keyDown(KeyEvent event)
{
	if (mVDUI->getOutputWindow()->isOutputWindow(event.getWindow())) {
		// Esc closes the projector output; 'f' would toggle the output window's own fullscreen
		if (event.getCode() == KeyEvent::KEY_ESCAPE) {
			mVDUI->getOutputWindow()->requestClose();
			return;
		}
		if (event.getCode() == KeyEvent::KEY_f) return;
	}

	// warp editor did not handle the key, so handle it here
	if (!mVDSessionFacade->handleKeyDown(event)) {
		switch (event.getCode()) {
		case KeyEvent::KEY_F12:
			// quit the application
			quit();
			break;
		case KeyEvent::KEY_f:
			// toggle full screen
			setFullScreen(!isFullScreen());
			break;

		case KeyEvent::KEY_l:
			mVDSessionFacade->createWarp();
			break;
		case KeyEvent::KEY_n:
			// NDI sender on/off
			toggleNdi();
			break;
		}
	}
}

void VDSpoutInNdiOutApp::keyUp(KeyEvent event)
{

	// let your application perform its keyUp handling here
	if (!mVDSessionFacade->handleKeyUp(event)) {
		
	}
}
void VDSpoutInNdiOutApp::cleanup()
{
	CI_LOG_V("cleanup and save");
	mVDSessionFacade->saveWarps();
	mVDSettings->save();
	CI_LOG_V("quit");
}

void VDSpoutInNdiOutApp::update()
{
#if defined( CINDER_MSW )
	// latest frame from the selected Spout sender (null until one is received); NDI is sent
	// from draw(), only when enabled and someone is connected
	mTexture = mSpoutIn.receiveTexture();
#endif
	if (mTrack != (int)mVDSessionFacade->getUniformValue(mVDUniforms->ITRACK)) {
		//mVDSessionFacade->setUniformValue(mVDUniforms->ITIME, (float)getElapsedSeconds());
		//mVDSessionFacade->setUniformValue(mVDUniforms->ISTART, mVDUniforms->getUniformValue(mVDUniforms->ITIME));
		mVDSessionFacade->useTimeWithTempo();
		mTrack = (int)mVDSessionFacade->getUniformValue(mVDUniforms->ITRACK);
		switch (mTrack)
		{
		case 110:
			// Zoom D7
			// mVDSessionFacade->loadFolder("zoom");
			mVDSessionFacade->loadFolder("boomer");
			break;
		case 111:
			// Binary D#7
			//mVDSessionFacade->loadFolder("onezero");
			// ntb
			mVDSessionFacade->loadFolder("dd");
			break;
		case 112:
			// SOS E7
			//mVDSessionFacade->loadFolder("sos");
			mVDSessionFacade->loadFolder("fear"); // k21
			break;
		case 113:
			// HumanET F7
			mVDSessionFacade->loadFolder("spidermoon");
			
			break;
		case 114:
			// HumanET F#7
			
			mVDSessionFacade->loadFolder("julien");
			break;
		case 115:
			// HumanET G#7
			//mVDSessionFacade->loadFolder("love");
			mVDSessionFacade->loadFolder("evolution");
			break;
		case 116:
			// HumanET G7
			//mVDSessionFacade->loadFolder("green");
			mVDSessionFacade->loadFolder("sky");
			break;
		case 117:
			// HumanET A7
			//mVDSessionFacade->loadFolder("purple");
			//mVDSessionFacade->loadFolder("gali");
			mVDSessionFacade->loadFolder("lovehate");
			break;
		case 118:
			// HumanET A#7 
			//mVDSessionFacade->loadFolder("logo");
			mVDSessionFacade->loadFolder("polo");
			break;
		case 119:
			// Yeah Beauso Disco Zombification 130 B7
			//mVDSessionFacade->loadFolder("yf");
			mVDSessionFacade->loadFolder("onezero");
			break;
		case 120:
			// welcometo C8
			mVDSessionFacade->loadFolder("welcometo");
			mVDSessionFacade->setUniformValue(mVDUniforms->IWEIGHT0, 1.0f);
			mVDSessionFacade->setUniformValue(mVDUniforms->IWEIGHT1, 0.0f);
			mVDSessionFacade->setUniformValue(mVDUniforms->IWEIGHT2, 0.0f);
			mVDSessionFacade->setUniformValue(mVDUniforms->IDISPLAYMODE, VDDisplayMode::FX);
			break;
		// champo
		case 108:
			// galeriens C7
			mVDSessionFacade->loadFolder("galeriens");
			break;
		case 109:
			// guns C#7
			mVDSessionFacade->loadFolder("guns");
			break;
		case 107:
			// mission B6
			mVDSessionFacade->loadFolder("mission");
			break;
		case 106:
			// fous A#6
			mVDSessionFacade->loadFolder("fous");
			break;
		case 105:
			// google A6
			mVDSessionFacade->loadFolder("google");
			break;
		case 121:
			// C#8 glasses
			mVDSessionFacade->loadFolder("glasses");
			break;
		case 122:
			// D8 bombe
			mVDSessionFacade->loadFolder("bombe");
			break;
		case 123:
			// D#8 egyptien
			mVDSessionFacade->loadFolder("egyptien");
			break;
		case 124:
			// E8 house
			mVDSessionFacade->loadFolder("house");
			break;
		case 125:
			// F8 hejoe
			mVDSessionFacade->loadFolder("hejoe");
			break;
		case 126:
			// F#8 dream
			mVDSessionFacade->loadFolder("dream");
			break;
		case 127:
			// G8 heaven
			mVDSessionFacade->loadFolder("heaven");
			break;

		default:
			break;
		}
	}
	mVDSessionFacade->setUniformValue(mVDUniforms->IFPS, getAverageFps());
	mVDSessionFacade->update();
	// open/close requests + warps rendered at the output's resolution (main GL context)
	mVDUI->getOutputWindow()->update();
}


void VDSpoutInNdiOutApp::resize()
{
	// resize() fires for every window: only the main one hosts ImGui
	if (getWindow() == getWindowIndex(0)) mVDUI->resize();
}
void VDSpoutInNdiOutApp::draw()
{
	// the optional projector output window (VDUI "Output" panel) goes through here too
	if (mVDUI->getOutputWindow()->isCurrent()) {
		mVDUI->getOutputWindow()->draw();
		return;
	}
	mVDUI->getOutputWindow()->applyMainWindowPacing();
	// clear the window and set the drawing color to white
	gl::clear();
	gl::color(Color::white());
	gl::setMatricesWindow(getWindowSize());
	// the received Spout frame when there is one (backup player), otherwise this app's own render
	ci::gl::TextureRef tex = mTexture;
	if (!tex) {
		int m = mVDSessionFacade->getUniformValue(mVDUniforms->IDISPLAYMODE);
		if (m == VDDisplayMode::POST) {
			tex = mVDSessionFacade->buildPostFboTexture();
		}
		else if (m == VDDisplayMode::FX) {
			tex = mVDSessionFacade->buildFxFboTexture();
		}
		else if (m == VDDisplayMode::WARP) {
			// the geometrically warped result (resize handles when warp edit mode - 'W' - is on)
			tex = mVDSessionFacade->buildRenderedWarpFboTexture();
		}
		else if (m < mVDSessionFacade->getFboShaderListSize()) {
			tex = mVDSessionFacade->getFboShaderTexture(m);
		}
	}
	if (tex) {
		gl::draw(tex, getWindowBounds());
		// the output window's "Main view" shows this same texture, drawn after this window
		mVDUI->getOutputWindow()->setMainViewTexture(tex);
#if defined( CINDER_MSW )
		// building the ci::Surface is a GPU->CPU readback: only when enabled ('n') and received
		if (mNdiOut && mNdiOut->hasConnections()) {
			long long timecode = app::getElapsedFrames();
			if (mTexture) mNdiOut->sendMetadata(XmlTree{ "ci_meta", mSpoutIn.getSenderName() }, timecode);
			ci::Surface surface(tex->createSource());
			mNdiOut->sendSurface(surface, timecode);
		}
#endif
	}
#if defined( CINDER_MSW )
	// live code view: the WebApp editor's text on a transparent background, as its own Spout
	// sender so Resolume can layer it over the visuals (cleared when the editor closes)
	//mCodeSpoutOut.sendTexture(mVDSessionFacade->buildCodeViewTexture(ivec2(mVDParams->getFboWidth(), mVDParams->getFboHeight())));
#endif
	// imgui
	if (mVDSessionFacade->showUI()) {
		mVDUI->Run("UI", (int)getAverageFps());
		if (mVDUI->isReady()) {
		}
	}
#if defined( CINDER_MSW )
	getWindow()->setTitle("Videodromm Spout In - " + (mTexture ? mSpoutIn.getSenderName() : std::string("no sender"))
		+ " - NDI " + (mNdiOut ? "on" : "off (n)") + " - " + toString((int)getAverageFps()) + " fps");
#else
	getWindow()->setTitle(toString((int)getAverageFps()) + " fps");
#endif
}
void prepareSettings(App::Settings *settings)
{
	// live performance: this app first, the others are helpers (VDLivePriority.h)
	videodromm::applyLivePriority();
	settings->setWindowSize(1280, 720);
	settings->setHighDensityDisplayEnabled(true);
}
CINDER_APP(VDSpoutInNdiOutApp, RendererGl(RendererGl::Options().msaa(8)),  prepareSettings)
