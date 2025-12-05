/*
**  Copyright 1998-2003 University of Illinois Board of Trustees
**  Copyright 1998-2003 Mark D. Roth
**  All rights reserved.
**
**  block.c - libtar code to handle tar archive header blocks
**
**  Mark D. Roth <roth@uiuc.edu>
**  Campus Information Technologies and Educational Services
**  University of Illinois at Urbana-Champaign
*/

#include <internal.h>

#include <errno.h>

#ifdef STDC_HEADERS
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
#endif


#define BIT_ISSET(bitmask, bit) ((bitmask) & (bit))


/*
** Parse PAX extended header data.
** PAX format: "len key=value\n" where len includes the length field itself.
** Returns 0 on success, -1 on error.
*/
static int
pax_parse_header(TAR *t, const char *data, size_t datalen)
{
	const char *p = data;
	const char *end = data + datalen;

#ifdef DEBUG
	printf("    pax_parse_header(): parsing %zu bytes\n", datalen);
#endif

	while (p < end && *p != '\0')
	{
		size_t len;
		const char *key_start, *key_end, *value_start, *value_end;
		char *endptr;

		/* Parse length field */
		len = strtoul(p, &endptr, 10);
		if (endptr == p || *endptr != ' ' || len == 0)
		{
#ifdef DEBUG
			printf("    pax_parse_header(): invalid length field\n");
#endif
			break;
		}

		/* Validate length doesn't exceed remaining data */
		if (len > (size_t)(end - p))
		{
#ifdef DEBUG
			printf("    pax_parse_header(): length %zu exceeds remaining data\n", len);
#endif
			break;
		}

		/* Find key=value pair */
		key_start = endptr + 1;
		key_end = memchr(key_start, '=', (p + len) - key_start);
		if (key_end == NULL)
		{
#ifdef DEBUG
			printf("    pax_parse_header(): no '=' found in record\n");
#endif
			p += len;
			continue;
		}

		value_start = key_end + 1;
		value_end = p + len - 1; /* -1 for newline */

		/* Skip if value is empty or malformed */
		if (value_start >= value_end)
		{
			p += len;
			continue;
		}

#ifdef DEBUG
		{
			size_t keylen = key_end - key_start;
			size_t vallen = value_end - value_start;
			printf("    pax_parse_header(): key='%.*s' value='%.*s'\n",
			       (int)keylen, key_start, (int)vallen, value_start);
		}
#endif

		/* Check for 'path' keyword */
		if ((size_t)(key_end - key_start) == 4 &&
		    strncmp(key_start, "path", 4) == 0)
		{
			size_t vallen = value_end - value_start;
			if (t->th_buf.pax_path != NULL)
				free(t->th_buf.pax_path);
			t->th_buf.pax_path = (char *)malloc(vallen + 1);
			if (t->th_buf.pax_path == NULL)
				return -1;
			memcpy(t->th_buf.pax_path, value_start, vallen);
			t->th_buf.pax_path[vallen] = '\0';
#ifdef DEBUG
			printf("    pax_parse_header(): set pax_path='%s'\n",
			       t->th_buf.pax_path);
#endif
		}
		/* Check for 'linkpath' keyword */
		else if ((size_t)(key_end - key_start) == 8 &&
		         strncmp(key_start, "linkpath", 8) == 0)
		{
			size_t vallen = value_end - value_start;
			if (t->th_buf.pax_linkpath != NULL)
				free(t->th_buf.pax_linkpath);
			t->th_buf.pax_linkpath = (char *)malloc(vallen + 1);
			if (t->th_buf.pax_linkpath == NULL)
				return -1;
			memcpy(t->th_buf.pax_linkpath, value_start, vallen);
			t->th_buf.pax_linkpath[vallen] = '\0';
#ifdef DEBUG
			printf("    pax_parse_header(): set pax_linkpath='%s'\n",
			       t->th_buf.pax_linkpath);
#endif
		}
		/* Other keywords (size, mtime, uid, gid, etc.) can be added here */

		p += len;
	}

	return 0;
}


/* read a header block */
int
th_read_internal(TAR *t)
{
	int i;
	int num_zero_blocks = 0;

#ifdef DEBUG
	printf("==> th_read_internal(TAR=\"%s\")\n", t->pathname);
#endif

	while ((i = tar_block_read(t, &(t->th_buf))) == T_BLOCKSIZE)
	{
		/* two all-zero blocks mark EOF */
		if (t->th_buf.name[0] == '\0')
		{
			num_zero_blocks++;
			if (!BIT_ISSET(t->options, TAR_IGNORE_EOT)
			    && num_zero_blocks >= 2)
				return 0;	/* EOF */
			else
				continue;
		}

		/* verify magic and version */
		if (BIT_ISSET(t->options, TAR_CHECK_MAGIC)
		    && strncmp(t->th_buf.magic, TMAGIC, TMAGLEN - 1) != 0)
		{
#ifdef DEBUG
			puts("!!! unknown magic value in tar header");
#endif
			return -2;
		}

		if (BIT_ISSET(t->options, TAR_CHECK_VERSION)
		    && strncmp(t->th_buf.version, TVERSION, TVERSLEN) != 0)
		{
#ifdef DEBUG
			puts("!!! unknown version value in tar header");
#endif
			return -2;
		}

		/* check chksum */
		if (!BIT_ISSET(t->options, TAR_IGNORE_CRC)
		    && !th_crc_ok(t))
		{
#ifdef DEBUG
			puts("!!! tar header checksum error");
#endif
			return -2;
		}

		break;
	}

#ifdef DEBUG
	printf("<== th_read_internal(): returning %d\n", i);
#endif
	return i;
}


/* wrapper function for th_read_internal() to handle GNU extensions */
int
th_read(TAR *t)
{
	int i;
	size_t sz, j, blocks;
	char *ptr;

#ifdef DEBUG
	printf("==> th_read(t=0x%lx)\n", t);
#endif

	if (t->th_buf.gnu_longname != NULL)
		free(t->th_buf.gnu_longname);
	if (t->th_buf.gnu_longlink != NULL)
		free(t->th_buf.gnu_longlink);
	if (t->th_buf.pax_path != NULL)
		free(t->th_buf.pax_path);
	if (t->th_buf.pax_linkpath != NULL)
		free(t->th_buf.pax_linkpath);
	memset(&(t->th_buf), 0, sizeof(struct tar_header));

	i = th_read_internal(t);
	if (i == 0)
		return 1;
	else if (i != T_BLOCKSIZE)
	{
		if (i != -1)
			errno = EINVAL;
		return -1;
	}

	/* check for GNU long link extention */
	if (TH_ISLONGLINK(t))
	{
		sz = th_get_size(t);
		blocks = (sz / T_BLOCKSIZE) + (sz % T_BLOCKSIZE ? 1 : 0);
		if (blocks > ((size_t)-1 / T_BLOCKSIZE))
		{
			errno = E2BIG;
			return -1;
		}
#ifdef DEBUG
		printf("    th_read(): GNU long linkname detected "
		       "(%ld bytes, %d blocks)\n", sz, blocks);
#endif
		t->th_buf.gnu_longlink = (char *)malloc(blocks * T_BLOCKSIZE);
		if (t->th_buf.gnu_longlink == NULL)
			return -1;

		for (j = 0, ptr = t->th_buf.gnu_longlink; j < blocks;
		     j++, ptr += T_BLOCKSIZE)
		{
#ifdef DEBUG
			printf("    th_read(): reading long linkname "
			       "(%d blocks left, ptr == %ld)\n", blocks-j, ptr);
#endif
			i = tar_block_read(t, ptr);
			if (i != T_BLOCKSIZE)
			{
				if (i != -1)
					errno = EINVAL;
				return -1;
			}
#ifdef DEBUG
			printf("    th_read(): read block == \"%s\"\n", ptr);
#endif
		}
#ifdef DEBUG
		printf("    th_read(): t->th_buf.gnu_longlink == \"%s\"\n",
		       t->th_buf.gnu_longlink);
#endif

		i = th_read_internal(t);
		if (i != T_BLOCKSIZE)
		{
			if (i != -1)
				errno = EINVAL;
			return -1;
		}
	}

	/* check for GNU long name extention */
	if (TH_ISLONGNAME(t))
	{
		sz = th_get_size(t);
		blocks = (sz / T_BLOCKSIZE) + (sz % T_BLOCKSIZE ? 1 : 0);
		if (blocks > ((size_t)-1 / T_BLOCKSIZE))
		{
			errno = E2BIG;
			return -1;
		}
#ifdef DEBUG
		printf("    th_read(): GNU long filename detected "
		       "(%ld bytes, %d blocks)\n", sz, blocks);
#endif
		t->th_buf.gnu_longname = (char *)malloc(blocks * T_BLOCKSIZE);
		if (t->th_buf.gnu_longname == NULL)
			return -1;

		for (j = 0, ptr = t->th_buf.gnu_longname; j < blocks;
		     j++, ptr += T_BLOCKSIZE)
		{
#ifdef DEBUG
			printf("    th_read(): reading long filename "
			       "(%d blocks left, ptr == %ld)\n", blocks-j, ptr);
#endif
			i = tar_block_read(t, ptr);
			if (i != T_BLOCKSIZE)
			{
				if (i != -1)
					errno = EINVAL;
				return -1;
			}
#ifdef DEBUG
			printf("    th_read(): read block == \"%s\"\n", ptr);
#endif
		}
#ifdef DEBUG
		printf("    th_read(): t->th_buf.gnu_longname == \"%s\"\n",
		       t->th_buf.gnu_longname);
#endif

		i = th_read_internal(t);
		if (i != T_BLOCKSIZE)
		{
			if (i != -1)
				errno = EINVAL;
			return -1;
		}
	}

	/* check for PAX extended header */
	if (TH_ISPAX(t) || TH_ISPAXGLOBAL(t))
	{
		char *pax_path_saved = NULL;
		char *pax_linkpath_saved = NULL;

		sz = th_get_size(t);
		blocks = (sz / T_BLOCKSIZE) + (sz % T_BLOCKSIZE ? 1 : 0);
		if (blocks > ((size_t)-1 / T_BLOCKSIZE))
		{
			errno = E2BIG;
			return -1;
		}
#ifdef DEBUG
		printf("    th_read(): PAX extended header detected "
		       "(%zu bytes, %zu blocks), typeflag='%c'\n",
		       sz, blocks, t->th_buf.typeflag);
#endif
		ptr = (char *)malloc(blocks * T_BLOCKSIZE);
		if (ptr == NULL)
			return -1;

		for (j = 0; j < blocks; j++)
		{
#ifdef DEBUG
			printf("    th_read(): reading PAX block %zu of %zu\n",
			       j + 1, blocks);
#endif
			i = tar_block_read(t, ptr + (j * T_BLOCKSIZE));
			if (i != T_BLOCKSIZE)
			{
				free(ptr);
				if (i != -1)
					errno = EINVAL;
				return -1;
			}
		}

		/* Parse PAX data */
		if (pax_parse_header(t, ptr, sz) != 0)
		{
			free(ptr);
			return -1;
		}

		/* Save pax values before reading next header */
		if (t->th_buf.pax_path != NULL)
		{
			pax_path_saved = t->th_buf.pax_path;
			t->th_buf.pax_path = NULL;
		}
		if (t->th_buf.pax_linkpath != NULL)
		{
			pax_linkpath_saved = t->th_buf.pax_linkpath;
			t->th_buf.pax_linkpath = NULL;
		}

		free(ptr);

		/* Read the actual file header that follows */
		i = th_read_internal(t);
		if (i != T_BLOCKSIZE)
		{
			if (pax_path_saved != NULL)
				free(pax_path_saved);
			if (pax_linkpath_saved != NULL)
				free(pax_linkpath_saved);
			if (i != -1)
				errno = EINVAL;
			return -1;
		}

		/* Restore saved pax values to the new header */
		t->th_buf.pax_path = pax_path_saved;
		t->th_buf.pax_linkpath = pax_linkpath_saved;

#ifdef DEBUG
		if (t->th_buf.pax_path != NULL)
			printf("    th_read(): PAX path restored: '%s'\n",
			       t->th_buf.pax_path);
		if (t->th_buf.pax_linkpath != NULL)
			printf("    th_read(): PAX linkpath restored: '%s'\n",
			       t->th_buf.pax_linkpath);
#endif
	}

#if 0
	/*
	** work-around for old archive files with broken typeflag fields
	** NOTE: I fixed this in the TH_IS*() macros instead
	*/

	/*
	** (directories are signified with a trailing '/')
	*/
	if (t->th_buf.typeflag == AREGTYPE
	    && t->th_buf.name[strlen(t->th_buf.name) - 1] == '/')
		t->th_buf.typeflag = DIRTYPE;

	/*
	** fallback to using mode bits
	*/
	if (t->th_buf.typeflag == AREGTYPE)
	{
		mode = (mode_t)oct_to_int(t->th_buf.mode);

		if (S_ISREG(mode))
			t->th_buf.typeflag = REGTYPE;
		else if (S_ISDIR(mode))
			t->th_buf.typeflag = DIRTYPE;
		else if (S_ISFIFO(mode))
			t->th_buf.typeflag = FIFOTYPE;
		else if (S_ISCHR(mode))
			t->th_buf.typeflag = CHRTYPE;
		else if (S_ISBLK(mode))
			t->th_buf.typeflag = BLKTYPE;
		else if (S_ISLNK(mode))
			t->th_buf.typeflag = SYMTYPE;
	}
#endif

	return 0;
}


/* write a header block */
int
th_write(TAR *t)
{
	int i, j;
	char type2;
	size_t sz, sz2;
	char *ptr;
	char buf[T_BLOCKSIZE];

#ifdef DEBUG
	printf("==> th_write(TAR=\"%s\")\n", t->pathname);
	th_print(t);
#endif

	if ((t->options & TAR_GNU) && t->th_buf.gnu_longlink != NULL)
	{
#ifdef DEBUG
		printf("th_write(): using gnu_longlink (\"%s\")\n",
		       t->th_buf.gnu_longlink);
#endif
		/* save old size and type */
		type2 = t->th_buf.typeflag;
		sz2 = th_get_size(t);

		/* write out initial header block with fake size and type */
		t->th_buf.typeflag = GNU_LONGLINK_TYPE;
		sz = strlen(t->th_buf.gnu_longlink);
		th_set_size(t, sz);
		th_finish(t);
		i = tar_block_write(t, &(t->th_buf));
		if (i != T_BLOCKSIZE)
		{
			if (i != -1)
				errno = EINVAL;
			return -1;
		}

		/* write out extra blocks containing long name */
		for (j = (sz / T_BLOCKSIZE) + (sz % T_BLOCKSIZE ? 1 : 0),
		     ptr = t->th_buf.gnu_longlink; j > 1;
		     j--, ptr += T_BLOCKSIZE)
		{
			i = tar_block_write(t, ptr);
			if (i != T_BLOCKSIZE)
			{
				if (i != -1)
					errno = EINVAL;
				return -1;
			}
		}
		memset(buf, 0, T_BLOCKSIZE);
		strncpy(buf, ptr, T_BLOCKSIZE);
		i = tar_block_write(t, &buf);
		if (i != T_BLOCKSIZE)
		{
			if (i != -1)
				errno = EINVAL;
			return -1;
		}

		/* reset type and size to original values */
		t->th_buf.typeflag = type2;
		th_set_size(t, sz2);
	}

	if ((t->options & TAR_GNU) && t->th_buf.gnu_longname != NULL)
	{
#ifdef DEBUG
		printf("th_write(): using gnu_longname (\"%s\")\n",
		       t->th_buf.gnu_longname);
#endif
		/* save old size and type */
		type2 = t->th_buf.typeflag;
		sz2 = th_get_size(t);

		/* write out initial header block with fake size and type */
		t->th_buf.typeflag = GNU_LONGNAME_TYPE;
		sz = strlen(t->th_buf.gnu_longname);
		th_set_size(t, sz);
		th_finish(t);
		i = tar_block_write(t, &(t->th_buf));
		if (i != T_BLOCKSIZE)
		{
			if (i != -1)
				errno = EINVAL;
			return -1;
		}

		/* write out extra blocks containing long name */
		for (j = (sz / T_BLOCKSIZE) + (sz % T_BLOCKSIZE ? 1 : 0),
		     ptr = t->th_buf.gnu_longname; j > 1;
		     j--, ptr += T_BLOCKSIZE)
		{
			i = tar_block_write(t, ptr);
			if (i != T_BLOCKSIZE)
			{
				if (i != -1)
					errno = EINVAL;
				return -1;
			}
		}
		memset(buf, 0, T_BLOCKSIZE);
		strncpy(buf, ptr, T_BLOCKSIZE);
		i = tar_block_write(t, &buf);
		if (i != T_BLOCKSIZE)
		{
			if (i != -1)
				errno = EINVAL;
			return -1;
		}

		/* reset type and size to original values */
		t->th_buf.typeflag = type2;
		th_set_size(t, sz2);
	}

	th_finish(t);

#ifdef DEBUG
	/* print tar header */
	th_print(t);
#endif

	i = tar_block_write(t, &(t->th_buf));
	if (i != T_BLOCKSIZE)
	{
		if (i != -1)
			errno = EINVAL;
		return -1;
	}

#ifdef DEBUG
	puts("th_write(): returning 0");
#endif
	return 0;
}


