////
// File: dyn_buffer.c
/////


struct dyn_buffer {
	char *data;
	int allocsize;
	int datasize;
	int windex; // write position
	int rindex; // read position
};
/*
void dynbuf_print( struct dyn_buffer *dbf )
{
	printf("DYNAMIC BUFFER\n Allocated size = %d\n Data size = %d\n Write Index = %d\n Read Index = %d\n", dbf->allocsize, dbf->datasize, dbf->windex, dbf->rindex);
}
*/
void *dynbuf_init( struct dyn_buffer *dbf, int size )
{
	char *data = malloc( size );
	dbf->data = data;
	dbf->windex = 0;
	dbf->rindex = 0;
	dbf->datasize = 0;
	dbf->allocsize = size;
	return data;
}


void *dynbuf_realloc( struct dyn_buffer *dbf, int newsize )
{
	newsize = (newsize+1023)/1024;
	newsize *= 1024; // en kilos
	char *newdata = malloc( newsize );
	if (!newdata) return NULL;
	if (newsize>dbf->allocsize) memcpy( newdata, dbf->data, dbf->allocsize ); else memcpy( newdata, dbf->data, newsize );
	free( dbf->data );
	dbf->data = newdata;
	dbf->allocsize = newsize;
	return newdata;
}


void dynbuf_write( struct dyn_buffer *dbf, unsigned char *data, int size )
{
	if ( (dbf->windex+size)>dbf->allocsize ) dynbuf_realloc( dbf, dbf->windex+size );
	memcpy( dbf->data+dbf->windex, data, size);
	dbf->windex += size;
	if (dbf->windex>dbf->datasize) dbf->datasize = dbf->windex;
	//dynbuf_print( dbf );
}

void dynbuf_free( struct dyn_buffer *dbf )
{
	if (!dbf->data) return;
	free( dbf->data );
	dbf->data = NULL;
	dbf->windex = 0;
	dbf->rindex = 0;
	dbf->datasize = 0;
	dbf->allocsize = 0;
}


