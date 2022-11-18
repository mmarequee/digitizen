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
#include "peak.h"


#define BUFFER_SIZE 32768
#define CHANNELS 	2
#define	LEFT	0
#define	RIGHT	1

#define __SSE_

int fp[CHANNELS];

SF_INFO	sfp_info;
SNDFILE *sfp;

struct stat statbuf[CHANNELS];

float audio_sample;

float peak[CHANNELS];

char *suffix[] = {"_01", "_02"};

inline void interleave_floats(float *non_interleaved_l, float *non_interleaved_r, float *interleaved)
{
		__asm__ __volatile__(
		"movaps 	(%%rdi),	%%xmm0		\n\t"
		"unpcklps	(%%rsi),	%%xmm0		\n\t"
		"movaps		%%xmm0,		(%%rdx)		\n\t"
		"movaps 	(%%rdi),	%%xmm0		\n\t"
		"unpckhps	(%%rsi),	%%xmm0		\n\t"
		"movaps		%%xmm0,		0x10(%%rdx)	\n\t"
		:
		:"D"	(non_interleaved_l), "S" (non_interleaved_r), "d" (interleaved) 
		:"xmm0"
		);
}

int main(int argc, char* argv[])
{
	int i, j, channels, chunk_size;

	float 	**non_interleaved_float			= (float **)malloc(2     	* sizeof(float *));

	for(channels = 0; channels < CHANNELS; channels++)
		  non_interleaved_float[channels]	= (float  *)malloc(BUFFER_SIZE * sizeof(float)); 
	
	float	*interleaved_float			= malloc((BUFFER_SIZE << 1) * sizeof(float));

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
	strcat(name, ".w64");

	sfp_info.frames		= 0;

	for(channels = 0; channels < CHANNELS; channels++)
		sfp_info.frames += (statbuf[channels].st_size / sizeof(float));  

	/* modulo doesn't work? */

		remainder = (sfp_info.frames / (BUFFER_SIZE * CHANNELS));
		remainder *= BUFFER_SIZE;
		remainder = sfp_info.frames / CHANNELS - remainder;

        sfp_info.samplerate 	= 96000;
        sfp_info.channels 	= 2;
        sfp_info.format 	= SF_FORMAT_W64 | SF_FORMAT_FLOAT;

	filesize = (unsigned int) sfp_info.frames;
	
	if ((sfp = sf_open(name, SFM_WRITE, &sfp_info)) == NULL)
	{
		printf("No sound file opened.\n");
		exit(1);
	}; 

	chunk_size = BUFFER_SIZE;

	for (i = 0; i < (filesize >> 1); i += BUFFER_SIZE)
	{
		if(((filesize >> 1) - i) < BUFFER_SIZE) chunk_size = (int)remainder;

		for(channels = 0; channels < CHANNELS; channels++)
		{
			read(fp[channels], non_interleaved_float[channels], sizeof(float) * chunk_size);

			calculate_peak(non_interleaved_float[channels], &peak[channels], chunk_size);
		}
#ifdef __SSE_
		for(j = 0; j < chunk_size; j += 4)
		{
			interleave_floats(	&(non_interleaved_float[ LEFT][j]), 
						&(non_interleaved_float[RIGHT][j]), 
						&(interleaved_float[(j << 1)]));
#else
		for(j = 0; j < BUFFER_SIZE; j++)
		{
			for(channels = 0; channels < CHANNELS; channels++)
			{	
				interleaved_float[(j << 1) + channels] = non_interleaved_float[channels][j];
			}
#endif
		}

		sf_write_float(sfp, interleaved_float, chunk_size << 1); 

		printf("Converting file of size %ldMb: %ld % written.\r", 
		(filesize >> 18),
		(long)((((float)(i) + chunk_size) / (filesize >> 1)) * 100));
		fflush(stdout);
	};

	printf("\nPeaks L: %.3f (%.3fdB) R: %.3f (%.3fdB)\nFinishing...\n", 
			peak[LEFT ], peak[LEFT ] == 0.0f ? -90.0f : (20.0f * log10f(peak[LEFT ])), 
			peak[RIGHT], peak[RIGHT] == 0.0f ? -90.0f : (20.0f * log10f(peak[RIGHT])));
	fflush(stdout);

	free(non_interleaved_float);
	for(channels = 0; channels < CHANNELS; channels++)
		free(non_interleaved_float[channels]);
	
	for(i = 0; i < CHANNELS; i++)
		close(fp[i]);

	sf_close(sfp);

	return 0;
}
