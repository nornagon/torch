#include "datastream.h"
#include <stdio.h>
#include "assert.h"

/* DataStream */

DataStream::DataStream(): fd(0), ownfd(false)
{}

DataStream::DataStream(FILE *f): fd(f), ownfd(false)
{}

DataStream::DataStream(const char *filename, const char *mode)
{
	fd = fopen(filename, mode);
	ownfd = true;
}

DataStream::~DataStream()
{
	if (ownfd && fd)
		fclose(fd);
}

size_t DataStream::read(void *ptr, size_t size, size_t nmemb)
{
	assert(fd);
	return fread(ptr, size, nmemb, fd);
}

size_t DataStream::write(const void *ptr, size_t size, size_t nmemb)
{
	assert(fd);
	return fwrite(ptr, size, nmemb, fd);
}

long DataStream::tell() const
{
	assert(fd);
	return ftell(fd);
}

bool DataStream::eof() const
{
	assert(fd);
	return feof(fd);
}


/* ZDataStream */

ZDataStream::ZDataStream(const char *filename, const char *mode)
{
	gz = gzopen(filename, mode);
	assert(gz);
}

ZDataStream::~ZDataStream()
{
	gzclose(gz);
}

size_t ZDataStream::read(void *ptr, size_t size, size_t nmemb)
{
	assert(gz);
	return gzread(gz, ptr, size*nmemb);
}

size_t ZDataStream::write(const void *ptr, size_t size, size_t nmemb)
{
	assert(gz);
	return gzwrite(gz, ptr, size*nmemb);
}

long ZDataStream::tell() const
{
	assert(gz);
	return gztell(gz);
}

bool ZDataStream::eof() const
{
	assert(gz);
	return gzeof(gz);
}


/* inserters/extractors */

DataStream& DataStream::operator <<(s8 i)
{
	write(&i, 1, 1);
	return *this;
}

DataStream& DataStream::operator <<(s16 i)
{
	write(&i, 2, 1);
	return *this;
}

DataStream& DataStream::operator <<(s32 i)
{
	write(&i, 4, 1);
	return *this;
}

DataStream& DataStream::operator >>(s8 &i)
{
	read(&i, 1, 1);
	return *this;
}

DataStream& DataStream::operator >>(s16 &i)
{
	read(&i, 2, 1);
	return *this;
}

DataStream& DataStream::operator >>(s32 &i)
{
	read(&i, 4, 1);
	return *this;
}
