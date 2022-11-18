/*
 * Copyright (C) Marek Peteraj 2009 <marek.peteraj@pm.me>
 *
 * main.c is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * main.c is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <sndfile.h>
#include <sys/stat.h>
#include <fcntl.h>


#define BUFFER_SIZE 32768
#define CHANNELS	2
#define	LEFT		0
#define	RIGHT		1
#define PCM_24		3

int fp[CHANNELS];

SF_INFO	sfp_info;
SNDFILE *sfp;

struct stat statbuf[CHANNELS];

float audio_sample;

float peak[CHANNELS];

char *suffix[] = {"_01", "_02"};


inline void cvt_ilv_pcm24(float *non_interleaved_l, float *non_interleaved_r, void *interleaved) 
{
	
	int mask[4] = {0x04020100, 0x09080605, 0x0E0D0C0A, 0x0F0B0703};

	float ceiling = (float)0x7FFFFF;
	float *cl_ptr = &ceiling; 

		__asm__ __volatile__(
		"movaps 	(%%rdi),		%%xmm0		\n\t"
		
		"movq	 	(%4),			%%xmm2		\n\t"
		"shufps		$0x00, %%xmm2, 		%%xmm2		\n\t"

		"mulps		%%xmm2, 		%%xmm0		\n\t"
		"movaps 	(%%rsi),		%%xmm1		\n\t"
		"mulps		%%xmm2, 		%%xmm1		\n\t"
		"cvtps2dq 	%%xmm0,			%%xmm0		\n\t"
		"cvtps2dq 	%%xmm1,			%%xmm1		\n\t"
		"movdqa		%%xmm0, 		%%xmm2		\n\t"
		"punpckldq	%%xmm1,			%%xmm2		\n\t"

		"movdqa 	(%3),			%%xmm3		\n\t"	

		"pshufb		%%xmm3,			%%xmm2		\n\t"
		"movdqu		%%xmm2, 		(%%rdx)		\n\t"
		"punpckhdq	%%xmm1,			%%xmm0		\n\t"
		"pshufb		%%xmm3,			%%xmm0		\n\t"
		"movdqu		%%xmm0, 		0x0C(%%rdx) 	\n\t"
		:
		:"D"	(non_interleaved_l), "S" (non_interleaved_r), "d" (interleaved), "r" (mask), "r" (cl_ptr)
		:"xmm0", "xmm1", "xmm2", "xmm3"
		);
}



int main(int argc, char* argv[])
{
	int i, j, channels, chunk_size;

	float 	**non_interleaved_float			= (float **)malloc(2     	* sizeof(float *));

	for(channels = 0; channels < CHANNELS; channels++)
		  non_interleaved_float[channels]	= (float  *)malloc(BUFFER_SIZE * sizeof(float)); 
	
	char	*interleaved_pcm24			= malloc((BUFFER_SIZE << 1) * PCM_24);

	char name[32];

	long	filesize, remainder;

	if(argc < 2)
	{
			printf("No signature given.\n");
			exit(1);
	};
	
	for(channels = 0; channels < CHANNELS; channels++)
	{
		memset(name, 0, sizeof(name));

		strcpy(name, argv[1]);
		strcat(name, suffix[channels]);

		if ((fp[channels] = open(name, O_RDONLY)) == 0) 
		{
			printf("No file opened.\n");
			exit(1);
		};

		fstat (fp[channels], &statbuf[channels]);
	};

	memset(name, 0, sizeof(name));

	strcpy(name, argv[1]);
	strcat(name, ".wav");

	sfp_info.frames		= 0;

	for(channels = 0; channels < CHANNELS; channels++)
		sfp_info.frames += (statbuf[channels].st_size / sizeof(float));

		/* modulo doesn't work? */

		remainder = (sfp_info.frames / (BUFFER_SIZE * CHANNELS));
		remainder *= BUFFER_SIZE;
		remainder = sfp_info.frames / CHANNELS - remainder; 

	//printf("%ld %ld\n", sfp_info.frames, remainder);

        sfp_info.samplerate 	= 96000;
        sfp_info.channels 	= 2;
        sfp_info.format 	= SF_FORMAT_WAV | SF_FORMAT_PCM_24;

	filesize = (unsigned int) sfp_info.frames;
	
	if ((sfp = sf_open(name, SFM_WRITE, &sfp_info)) == NULL)
	{
		printf("No sound file opened.\n");
		exit(1);
	}; 

	chunk_size = BUFFER_SIZE;

	for (i = 0; i < (filesize >> 1); i += BUFFER_SIZE) /**/
	{
		if(((filesize >> 1) - i) < BUFFER_SIZE) chunk_size = (int)remainder;

		for(channels = 0; channels < CHANNELS; channels++)
		{
			read(fp[channels], non_interleaved_float[channels], sizeof(float) * chunk_size);

			calculate_peak(non_interleaved_float[channels], &peak[channels], chunk_size);
		}

		int index = 0;

//				 cvt_ilv_pcm24(	&(non_interleaved_float[ LEFT][0]), 
//			 			&(non_interleaved_float[RIGHT][0]), 
//			 			&(interleaved_pcm24[index]));

		for(j = 0; j < chunk_size; j += 1) /* 1 for SISD, 4 for SIMD */
		{
//				 cvt_ilv_pcm24(	&(non_interleaved_float[ LEFT][j]), 
//			 			&(non_interleaved_float[RIGHT][j]), 
//			 			&(interleaved_pcm24[index]));
			 

			interleaved_pcm24[index    ] = (char)((int)( non_interleaved_float[ LEFT][j] * 0x7FFFFF) & 0x000000FF);
			interleaved_pcm24[index + 1] = (char)(((int)(non_interleaved_float[ LEFT][j] * 0x7FFFFF) & 0x0000FF00) >>  8);
			interleaved_pcm24[index + 2] = (char)(((int)(non_interleaved_float[ LEFT][j] * 0x7FFFFF) & 0x00FF0000) >> 16);
			interleaved_pcm24[index + 3] = (char)((int)( non_interleaved_float[RIGHT][j] * 0x7FFFFF) & 0x000000FF);
			interleaved_pcm24[index + 4] = (char)(((int)(non_interleaved_float[RIGHT][j] * 0x7FFFFF) & 0x0000FF00) >>  8);
			interleaved_pcm24[index + 5] = (char)(((int)(non_interleaved_float[RIGHT][j] * 0x7FFFFF) & 0x00FF0000) >> 16);

			index += 6; /* 6 for SISD, 24 for SIMD */
		}

		sf_write_raw(sfp, interleaved_pcm24, chunk_size * CHANNELS * PCM_24); /* 196608 */

		printf("Converting file of size %ldMb: %ld % written.\r", 
		(filesize >> 18),
		(long)((((float)(i) + chunk_size) / (filesize >> 1)) * 100));
		fflush(stdout);
	};

	printf("\nPeaks L: %.3f (%.3fdB) R: %.3f (%.3fdB)\nFinishing...\n", 
			peak[LEFT ], peak[LEFT ] == 0.0f ? -90.0f : (20.0f * log10f(peak[LEFT ])), 
			peak[RIGHT], peak[RIGHT] == 0.0f ? -90.0f : (20.0f * log10f(peak[RIGHT])));
	fflush(stdout);

	//free(non_interleaved_float);
	for(channels = 0; channels < CHANNELS; channels++)
		free(non_interleaved_float[channels]);
	
	for(i = 0; i < CHANNELS; i++)
		close(fp[i]);

	sf_close(sfp);

	return 0;
}
