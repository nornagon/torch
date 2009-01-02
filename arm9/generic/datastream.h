#ifndef DATASTREAM_H
#define DATASTREAM_H 1

#include <nds/jtypes.h>
#include <stdio.h>

class DataStream {
	FILE *fd;
	bool ownfd;

	public:
	DataStream();
	DataStream(FILE*);
	DataStream(const char *filename); // opens filename in mode a+
	~DataStream();

	FILE *getfd();
	FILE *setfd(FILE*);

	bool eof() const;

	DataStream &operator <<(s8);
	DataStream &operator >>(s8&);
	DataStream &operator <<(s16);
	DataStream &operator >>(s16&);
	DataStream &operator <<(s32);
	DataStream &operator >>(s32&);

	DataStream &operator <<(u8);
	DataStream &operator >>(u8&);
	DataStream &operator <<(u16);
	DataStream &operator >>(u16&);
	DataStream &operator <<(u32);
	DataStream &operator >>(u32&);
};

inline DataStream& DataStream::operator <<(u8 i)
{ return *this << (s8)i; }

inline DataStream& DataStream::operator <<(u16 i)
{ return *this << (s16)i; }

inline DataStream& DataStream::operator <<(u32 i)
{ return *this << (s32)i; }

inline DataStream& DataStream::operator >>(u8 &i)
{ return *this >> (s8&)i; }

inline DataStream& DataStream::operator >>(u16 &i)
{ return *this >> (s16&)i; }

inline DataStream& DataStream::operator >>(u32 &i)
{ return *this >> (s32&)i; }

#endif /* DATASTREAM_H */
