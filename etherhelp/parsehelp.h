
//Returns the termating string.
int ReadBlock( FILE * f, const char * delim, char * buffer, int bufflen )
{
	int i = 0;
	int c;

	//Wait for data.
	while( c = fgetc( f ) )
	{
		int j;
		if( c == EOF ) goto stop;
		for( j = 0; delim[j]; j++ )
		{
			if( delim[j] == c )
			{
				break;
			}
		}
		if( delim[j] == 0 )
		{
			ungetc( c, f );
			//Non-whitespace.
			break;
		}
	}

	while( c = fgetc( f ) )
	{
		int j;
		if( c == EOF ) break;
		for( j = 0; delim[j]; j++ )
		{
			if( delim[j] == c )
			{
				goto stop;
			}
		}
		if( i < bufflen-1 )
			buffer[i++] = c;
	}
stop:
	buffer[i] = 0;
	return c;
}

