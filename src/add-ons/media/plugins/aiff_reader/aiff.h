#ifndef _AIFF_H
#define _AIFF_H

// The complete size of a chunk is chunk_size + sizeof(chunk_struct)
// If the complete size of a chunk is not even, the chunk is padded with one byte
// sample order for multi channel sound:
// 2: left, right
// 3: left, right, center
// 4: front left, front right, rear left, rear right
// 4: left, center, right, surround
// 6: left center, left, center, right center, right, surround

// Apple maps Microsoft ACM formats into a compression code of the form "ms??",
// where the second two characters are the Microsoft Audio Compression Manager
// (ACM) code. For instance, Microsoft ADPCM, with ACM value of 1, is mapped
// to the code "ms\x00\x01".

struct chunk_struct
{
	uint32	chunk_id;
	uint32	chunk_size;
} _PACKED;

struct aiff_chunk
{
	uint32	chunk_id;	// 'FORM'
	uint32	chunk_size;
	uint32	aiff_id;	// 'AIFF' or 'AIFC'
	// chunks[]
} _PACKED;

struct comm_chunk
{
//	uint32	chunk_id;	// 'COMM'
//	uint32	chunk_size;
	uint16	channel_count;
	uint32	frame_count;
	uint16	bits_per_sample;	// store as 8 bit entities
	char	sample_rate[10];	// 80 bit IEEE floating point, big endian
	uint32	compression_id;		// only present if chunk_size >= 22
} _PACKED;

struct ssnd_chunk
{
//	uint32	chunk_id;	// 'SSND'
//	uint32	chunk_size;
	uint32	offset;
	uint32	block_size;
	// samples[]
} _PACKED;

#if DEBUG
inline const char * string_for_compression(uint32 compression)
{
	static char s[64];
	switch (compression) {
		case 'NONE': return "not compressed";
		case 'ADP4': return "4:1 Intel/DVI ADPCM";
		case 'DWVW': return "Delta With Variable Word Width";
		case 'FL32': return "IEEE 32-bit floating point";
		case 'fl32': return "IEEE 32-bit floating point";
		case 'fl64': return "IEEE 64-bit floating point";
		case 'alaw': return "8-bit ITU-T G.711 A-law";
		case 'ALAW': return "8-bit ITU-T G.711 A-law";
		case 'ulaw': return "8-bit ITU-T G.711 µ-law";
		case 'ULAW': return "8-bit ITU-T G.711 µ-law";
		case 'ima4': return "IMA 4:1";
		case 'MAC3': return "MACE 3-to-1";
		case 'MAC6': return "MACE 6-to-1";
		case 'QDMC': return "QDesign Music";
		case 'Qclp': return "Qualcomm PureVoice";
		case 'rt24': return "RT24 50:1";
		case 'rt29': return "RT29 50:1";
		default:
			sprintf(s, "unknown compression %c%c%c%c",
				(int)((compression >> 24) & 0xff),
				(int)((compression >> 16) & 0xff),
				(int)((compression >> 8) & 0xff),
				(int)(compression & 0xff));
			return s;
	}
}
#endif

#endif
