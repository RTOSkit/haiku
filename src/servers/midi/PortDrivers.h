/*
 * Copyright (c) 2003 Matthijs Hollemans
 * Copyright (c) 2003 Jerome Leveque
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _PORT_DRIVERS_H
#define _PORT_DRIVERS_H

#include <MidiProducer.h>
#include <MidiConsumer.h>

class MidiPortConsumer : public BMidiLocalConsumer
{
public:
	MidiPortConsumer(int fd, const char* name);

	virtual void Data(uchar* data, size_t length, bool atomic, bigtime_t time);

private:
	int fd;
};

class MidiPortProducer : public BMidiLocalProducer
{
public:
	MidiPortProducer(int fd, const char* name);
	~MidiPortProducer(void);

	int32 GetData(void);

private:
	static int32 SpawnThread(void* data);

	int fd;
	bool keepRunning;
};

#endif // _PORT_DRIVERS_H
