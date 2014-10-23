/////
// File: dcwfilter.c
/////

int checksumDCW(uint8 *data)
{
    if(data[3] != (byte)((data[0] + data[1] + data[2]) & 0xFF) || data[7] != (byte)((data[4] + data[5] + data[6]) & 0xFF)) {
      return 0;
    }
    if(data[11] != (byte)((data[8] + data[9] + data[10]) & 0xFF) || data[15] != (byte)((data[12] + data[13] + data[14]) & 0xFF)) {
      return 0;
    }
    return 1;
}


int isnullDCW(uint8 *data)
{
	int a1 = (!data[0] && !data[1] && !data[2]) || (!data[4] && !data[5] && !data[6]);
	int a2 = (!data[8] && !data[9] && !data[10]) || (!data[12] && !data[13] && !data[14]);
	return (a1 && a2);
}

int isbadDCW(uint8 *data)
{
	int a0 = ((data[0]&0xF0)==(data[1]&0xF0))&&((data[0]&0xF0)==(data[2]&0xF0));
	int a1 = ((data[4]&0xF0)==(data[5]&0xF0))&&((data[4]&0xF0)==(data[6]&0xF0));
	int a2 = ((data[8]&0xF0)==(data[9]&0xF0))&&((data[8]&0xF0)==(data[10]&0xF0));
	int a3 = ((data[12]&0xF0)==(data[13]&0xF0))&&((data[12]&0xF0)==(data[14]&0xF0));
	return ( (a0&&a1)&&(a2&&a3) );
}

int acceptDCW(uint8 *data)
{
	if (!checksumDCW(data)) return 0;
	if ( isnullDCW(data) ) return 0;
	if ( isbadDCW(data) ) return 0;

	struct dcw_data *dcw = cfg.bad_dcw;
	while (dcw) {
		if (!memcmp(dcw->dcw, data, 16)) return 0;
		dcw = dcw->next;
	}

	return 1;
}

