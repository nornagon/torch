#include "datastream.h"
#include <stdio.h>
#include "assert.h"

#define CHECK_STREAM_PRECOND assert(fd)

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
	if (ownfd)
		fclose(fd);
}

bool DataStream::eof() const
{
	CHECK_STREAM_PRECOND;
	return feof(fd);
}

DataStream& DataStream::operator <<(s8 i)
{
	CHECK_STREAM_PRECOND;
	fwrite(&i, 1, 1, fd);
	return *this;
}

DataStream& DataStream::operator <<(s16 i)
{
	CHECK_STREAM_PRECOND;
	fwrite(&i, 2, 1, fd);
	return *this;
}

DataStream& DataStream::operator <<(s32 i)
{
	CHECK_STREAM_PRECOND;
	fwrite(&i, 4, 1, fd);
	return *this;
}

DataStream& DataStream::operator >>(s8 &i)
{
	CHECK_STREAM_PRECOND;
	fread(&i, 1, 1, fd);
	return *this;
}

DataStream& DataStream::operator >>(s16 &i)
{
	CHECK_STREAM_PRECOND;
	fread(&i, 2, 1, fd);
	return *this;
}

DataStream& DataStream::operator >>(s32 &i)
{
	CHECK_STREAM_PRECOND;
	fread(&i, 4, 1, fd);
	return *this;
}
