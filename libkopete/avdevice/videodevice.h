//
// C++ Interface: videodevicelistitem
//
// Description: 
//
//
// Author: Kopete Developers <kopete-devel@kde.org>, (C) 2005
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef KOPETE_AVVIDEODEVICELISTITEM_H
#define KOPETE_AVVIDEODEVICELISTITEM_H

#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#ifdef __linux__

#undef __STRICT_ANSI__
#include <asm/types.h>
#include <linux/fs.h>
#include <linux/kernel.h>

#ifndef __u64 //required by videodev.h
#define __u64 unsigned long long
#endif // __u64

#include <linux/videodev.h>
#define __STRICT_ANSI__

#endif // __linux__

#include <qstring.h>
#include <qfile.h>
#include <qimage.h>
#include <qvaluevector.h>
#include <kcombobox.h>

#include "videoinput.h"

namespace Kopete {

namespace AV {

/**
@author Kopete Developers
*/
typedef enum
{
	VIDEODEV_DRIVER_NONE,
#ifdef __linux__
	VIDEODEV_DRIVER_V4L,
#ifdef HAVE_V4L2
	VIDEODEV_DRIVER_V4L2,
#endif
#endif
} videodev_driver;

class VideoDevice{
protected:
	int xioctl(int request, void *arg);
	int errnoReturn(const char* s);
public:
	VideoDevice();
	~VideoDevice();
	int setFileName(QString filename);
	int open();
	bool isOpen();
	int checkDevice();
	int showDeviceCapabilities();
	int initDevice();
	int inputs();
	int width();
	int minWidth();
	int maxWidth();
	int height();
	int minHeight();
	int maxHeight();
	unsigned int currentInput();
	int selectInput(int input);
	int startCapturing();
	int readFrame();
	int processImage(const void *p);
	int getImage(QImage *qimage);
	int stopCapturing();
	int close();
	QString name;
	QString full_filename;
	videodev_driver m_driver;
	int descriptor;

//protected:
#ifdef __linux__
#ifdef HAVE_V4L2
	struct v4l2_capability V4L2_capabilities;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
#endif
	struct video_capability V4L_capabilities;
	struct video_window V4L_videowindow;
	struct video_buffer V4L_videobuffer;
#endif	
	QValueVector<Kopete::AV::VideoInput> input;
//	QFile file;
protected:
	int currentwidth, minwidth, maxwidth, currentheight, minheight, maxheight;

	typedef enum
	{
		IO_METHOD_NONE,
		IO_METHOD_READ,
		IO_METHOD_MMAP,
		IO_METHOD_USERPTR,
	} io_method;
	io_method m_io_method;

	struct buffer2
	{
		int height;
		int width;
		int pixfmt;
		size_t size;
		QValueVector <uchar> data;
	};
	struct buffer
	{
		uchar * start;
		size_t length;
	};
	QValueVector<buffer> buffers;
	unsigned int     n_buffers;
	buffer2 currentbuffer;
	int m_buffer_size;

	int m_current_input;

	int initRead();
	int initMmap();
	int initUserptr();

};

}

}

#endif
