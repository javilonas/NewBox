#if 0
# 
# Copyright (c) 2014 - 2015 Javier Sayago <admin@lonasdigital.com>
# Contact: javilonas@esp-desarrolladores.com
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#endif

#define CR '\r'
#define LF '\n'
#define CRLF "\r\n"

#define CBASE64_BUFFSZ 32

char base64_chr_table(int index)
{
	char table[] = {
		'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
		'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
		'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
		'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/'};

	return table[index];
}

typedef struct b64_chunk
{
/*
** longeur de la chaine, comme on n'aura besoin d'aller de 0 a 3, on utilise un
** unsigned char plutot qu'un unsigned int pour gagner 3 octets sur un une
** plateforme 32 bits.
*/
	int len;
/*
** Buffers d'encodage et de décodage.
*/
	char enc[4];
	char dec[3];
}B64_CHUNK;


void base64_encoding_process(B64_CHUNK *ptr)
{
	ptr->enc[0] = base64_chr_table((ptr->dec[0] & 0xFC) >> 2);
	ptr->enc[1] = base64_chr_table((((ptr->dec[0] & 0x03) << 4) |
		((ptr->dec[1] & 0xF0) >> 4)));
	ptr->enc[2] = (ptr->len > 1) ?
		base64_chr_table(((ptr->dec[1] & 0x0F) << 2) |
			((ptr->dec[2] & 0xC0) >> 6)) :
		'=';
	ptr->enc[3] = (ptr->len > 2) ?
		base64_chr_table(ptr->dec[2] & 0x3F) :
		'=';
}

void base64_decoding_process(B64_CHUNK *ptr)
{
	ptr->dec[0] = ((ptr->enc[0] & 0x3F) << 2) | (((ptr->enc[1]) & 0x30) >> 4);
	ptr->dec[1] = ((ptr->enc[1] & 0x0F) << 4) | (((ptr->enc[2]) & 0x3C) >> 2);
	ptr->dec[2] = ((ptr->enc[2] & 0x0F) << 6) | ((ptr->enc[3]) & 0x3F);
}

/*
** Encodage d'une chaine de caracteres
*/
char * base64_pencode(const char *in, char *out, int linesz)
{
	int i, o, len;
	B64_CHUNK *chunk;
	chunk = malloc(sizeof(*chunk));

	for(i = 0, o = 0, len = 0; in[i];)
	{
		for (chunk->len = 0; chunk->len < 3 && in[i];)
		{
			chunk->dec[chunk->len++] = in[i++];
		}

		base64_encoding_process(chunk);
		if (len >= linesz)
		{
			out[o++] = '\r';
			out[o++] = '\n';
			len = 0;
		}
		for (chunk->len = 0; chunk->len < 4;)
		{
			out[o++] = chunk->enc[chunk->len++];
			len++;
		}
	}
	out[o] = '\0';

	free(chunk);
	return out;
}

/*
** Décodage d'une chaine de caracteres
*/


void base64_chr_real(char *chr)
{
	if (
		*chr >= 0x41 && *chr <= 0x5A)
	{
		*chr -= 0x41;
	}
	else if (*chr >= 0x61 && *chr <= 0x7A)
	{
		*chr -= 0x47;
	}
	else if (*chr >= 0x30 && *chr <= 0x39)
	{
		*chr += 0x04;
	}
	else if (*chr == 0x2B)
	{
		*chr = 0x3E;
	}
	else if (*chr == 0x2F)
	{
		*chr = 0x3F;
	}
	else if (*chr == 0x3D)
	{
		*chr = 64;
	}
	else
	{
		*chr = -1;
	}
}

char * base64_pdecode(const char *in, char *out)
{
	int i, o;
	B64_CHUNK *chunk;

	chunk = malloc(sizeof(*chunk));

	for (i = 0, o = 0; in[i];)
	{
		for (chunk->len = 0; chunk->len < 4 && in[i];)
		{
			chunk->enc[chunk->len] = in[i++];
			
			base64_chr_real(chunk->enc + chunk->len);

			if (chunk->enc[chunk->len] >= 0)
			{
				chunk->len++;
			}
		}

		base64_decoding_process(chunk);

		for (chunk->len = 0; chunk->len < 3 && chunk->dec[chunk->len];)
		{
			out[o++] = chunk->dec[chunk->len++];
		}
	}
	out[o] = '\0';

	free(chunk);
	return out;
}

