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
	gl::FboRef				mFbo[2];
	gl::PboRef				mPbo[2];
	ci::gl::Texture2dRef	mTexture;
};

VideodrommSpoutInNdiOutApp::VideodrommSpoutInNdiOutApp()
	: mSender("test-cinder-video")
	, mIndexNew{ 0 }
	, mIndexOld{ 1 }
{
	mFbo[0] = gl::Fbo::create(getWindowWidth(), getWindowHeight(), false);
	mFbo[1] = gl::Fbo::create(getWindowWidth(), getWindowHeight(), false);

	mSurface = ci::Surface::create(getWindowWidth(), getWindowHeight(), true, SurfaceChannelOrder::BGRA);

	mPbo[0] = gl::Pbo::create(GL_PIXEL_PACK_BUFFER, getWindowWidth() * getWindowHeight() * 4, 0, GL_STREAM_READ);
	mPbo[1] = gl::Pbo::create(GL_PIXEL_PACK_BUFFER, getWindowWidth() * getWindowHeight() * 4, 0, GL_STREAM_READ);

	mTexture = ci::gl::Texture::create(getWindowWidth(), getWindowHeight());
}

void VideodrommSpoutInNdiOutApp::mouseDown(MouseEvent event)
{
	if (event.isRightDown()) { // Select a sender
							   // SpoutPanel.exe must be in the executable path
		mSpoutIn.getSpoutReceiver().SelectSenderPanel(); // DirectX 11 by default
	}
}

void VideodrommSpoutInNdiOutApp::update()
{
	if (mSpoutIn.getSize() != app::getWindowSize()) {
		app::setWindowSize(mSpoutIn.getSize());
		mFbo[0] = gl::Fbo::create(getWindowWidth(), getWindowHeight(), false);
		mFbo[1] = gl::Fbo::create(getWindowWidth(), getWindowHeight(), false);

		mSurface = ci::Surface::create(getWindowWidth(), getWindowHeight(), true, SurfaceChannelOrder::BGRA);
		mPbo[0] = gl::Pbo::create(GL_PIXEL_PACK_BUFFER, getWindowWidth() * getWindowHeight() * 4, 0, GL_STREAM_READ);
		mPbo[1] = gl::Pbo::create(GL_PIXEL_PACK_BUFFER, getWindowWidth() * getWindowHeight() * 4, 0, GL_STREAM_READ);
		mTexture = ci::gl::Texture::create(getWindowWidth(), getWindowHeight(), ci::gl::Texture::Format().loadTopDown(false));		
	}
	getWindow()->setTitle("Videodromm Spout Receiver to NDI Out - " + std::to_string((int)getAverageFps()) + " FPS");

	if (mSurface)
	{
		gl::ScopedFramebuffer sFbo(mFbo[mIndexOld]);
		gl::ScopedBuffer scopedPbo(mPbo[mIndexNew]);

		gl::readBuffer(GL_COLOR_ATTACHMENT0);
		gl::readPixels(0, 0, mFbo[mIndexOld]->getWidth(), mFbo[mIndexOld]->getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, 0);
		mPbo[mIndexOld]->getBufferSubData(0, mFbo[mIndexOld]->getWidth() * mFbo[mIndexOld]->getHeight() * 4, mSurface->getData());
	}

	{
		gl::ScopedFramebuffer sFbo(mFbo[mIndexNew]);
		gl::ScopedViewport sVp(0, 0, mFbo[mIndexNew]->getWidth(), mFbo[mIndexNew]->getHeight());
		mTexture = mSpoutIn.receiveTexture();
		if (mTexture) {
			mSurface = Surface::create( mTexture->createSource());			
			//mSurface = ci::Surface::create(mTexture->createSource(), true, SurfaceChannelOrder::BGRA);
		}
	}

	long long timecode = app::getElapsedFrames();

	XmlTree msg{ "ci_meta", "test string" };
	mSender.sendMetadata(msg, timecode);
	mSender.sendSurface(*mSurface, timecode);
}

void VideodrommSpoutInNdiOutApp::draw()
{
	gl::clear();

	gl::draw(mTexture, Rectf{ 0.5f * float(app::getWindowWidth()),0, float(app::getWindowWidth()), 0.5f * float(app::getWindowHeight()) });
	std::swap(mIndexNew, mIndexOld);
}

void prepareSettings(App::Settings *settings)
{
	settings->setWindowSize(640, 480);
}
CINDER_APP(VideodrommSpoutInNdiOutApp, RendererGl, prepareSettings)

