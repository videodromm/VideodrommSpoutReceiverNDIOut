#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/GeomIo.h"
#include "cinder/CameraUi.h"
#include "cinder/Camera.h"

#include "CiSpoutIn.h"
#include "CinderNDISender.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class VideodrommSpoutInNdiOutApp : public App {
public:
	VideodrommSpoutInNdiOutApp();
	void mouseDown(MouseEvent event) override;
	void update() override;
	void draw() override;

	SpoutIn	mSpoutIn;
private:
	CinderNDISender			mSender;
	ci::SurfaceRef 			mSurface;
	uint8_t					mIndexNew, mIndexOld;
	ci::gl::Texture2dRef	mTexture;
};

VideodrommSpoutInNdiOutApp::VideodrommSpoutInNdiOutApp()
	: mSender("VideodrommSpoutIn")
	, mIndexNew{ 0 }
	, mIndexOld{ 1 }
{
	mSurface = ci::Surface::create(getWindowWidth(), getWindowHeight(), true, SurfaceChannelOrder::BGRA);
	mTexture = ci::gl::Texture::create(getWindowWidth(), getWindowHeight());
}

void VideodrommSpoutInNdiOutApp::mouseDown(MouseEvent event)
{
	if (event.isRightDown()) { // Select a sender
							   // SpoutPanel.exe must be in the executable folder
		mSpoutIn.getSpoutReceiver().SelectSenderPanel(); // DirectX 11 by default
	}
}

void VideodrommSpoutInNdiOutApp::update()
{
	if (mSpoutIn.getSize() != app::getWindowSize()) {
		app::setWindowSize(mSpoutIn.getSize());
		mTexture = ci::gl::Texture::create(getWindowWidth(), getWindowHeight(), ci::gl::Texture::Format().loadTopDown(true));
	}
	getWindow()->setTitle("Videodromm Spout Receiver to NDI Out - " + std::to_string((int)getAverageFps()) + " FPS");

	mTexture = mSpoutIn.receiveTexture();
	if (mTexture) {
		mSurface = Surface::create(mTexture->createSource());
	}

	long long timecode = app::getElapsedFrames();

	XmlTree msg{ "ci_meta", mSpoutIn.getSenderName() };
	mSender.sendMetadata(msg, timecode);
	mSender.sendSurface(*mSurface, timecode);
}

void VideodrommSpoutInNdiOutApp::draw()
{
	gl::clear();

	gl::draw(mTexture, getWindowBounds());
	std::swap(mIndexNew, mIndexOld);
}

void prepareSettings(App::Settings *settings)
{
	settings->setWindowSize(640, 480);
}
CINDER_APP(VideodrommSpoutInNdiOutApp, RendererGl, prepareSettings)

