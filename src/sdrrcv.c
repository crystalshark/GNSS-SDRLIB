/*------------------------------------------------------------------------------
* sdrrcv.c : SDR receiver functions
*
* Copyright (C) 2014 Taro Suzuki <gnsssdrlib@gmail.com>
*-----------------------------------------------------------------------------*/
#include "sdr.h"

#ifdef MAX2769_NET


SOCKET conn;
int max2769_net_init()
{
	struct sockaddr_in server;
	char buff[256];
	conn = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (conn == INVALID_SOCKET)
	{
		SDRPRINTF("socket creat failed\n");
		return -1;
	}
	int nNetTimeout = 3000;//1秒
	setsockopt(conn, SOL_SOCKET, SO_RCVTIMEO, (char *)&nNetTimeout, sizeof(int));
	SDRPRINTF("SOCKET created OK\n");
	server.sin_addr.s_addr = inet_addr("192.168.31.10");
	server.sin_family = AF_INET;
	server.sin_port = htons(7);
	if (connect(conn, (struct sockaddr*)&server, sizeof(server)))
	{
		closesocket(conn);
		SDRPRINTF("connect error\n");
		return -1;
	}
	SDRPRINTF("Connected to server 192.168.31.10:7\n");
	sprintf(buff, "GET C\r\n\r\n");
	send(conn, buff, strlen(buff), 0);
	SDRPRINTF("sending command :- GET C to server\n");
	return 0;

}
/*
void Max2769_convert(char *src, char *dst, int n)
{
	unsigned int sign, mag;
	char index = 0;
	//char max2769_bit_table[4]={2,6,-2,-6};
	char max2769_bit_table[4] = { 1,3,-1,-5 };
	static int magnitude_total_count = 0, magnitude_count=0, sign_count=0;

	//Max2769 I Channel data Convertion
	if (n % 8)
		SDRPRINTF("ERROR: n%%8 != 0\n");
	for (int i = 0; i<n / 8; i++)
	{
		//	primary_data=*((unsigned int *)(&(src[8*i])));
		//	secondary_data=*((unsigned int *)(&(src[8*i+4])));
		sign = *((unsigned int *)(&(src[8 * i]))); //sign
		mag = *((unsigned int *)(&(src[8 * i + 4]))); //mag
													  //msb
		for (int j = 0; j<32; j++)
		{
			index = ((mag & (1 << j)) >> j) + ((sign & (1 << j)) >> j) * 2;
			dst[32 * i + 31 - j] = max2769_bit_table[index];
			//dst[32*i+j]=max2769_bit_table[index];		

			magnitude_total_count++;
			if ((mag & (1 << j)) >> j)
				magnitude_count++;
			if ((sign & (1 << j)) >> j)
				sign_count++;
		}
	}

}
*/

void Max2769_convert_stereo(char *src, char *dst, int n)
{
	static uint64_t magnitude_total_count=0, sign_count=0, magnitude_count = 0;
	int i;
	//char index = 0;
	unsigned char d;
	//char max2769_bit_table[4]={2,6,-2,-6};
	char max2769_bit_table[4] = { 1,3,-1,-5 }; //符号位为，则adc数据为负数
	

	for (i = 0; i<n; i++)
	{
		d = src[i];
		dst[i * 4 + 0] = max2769_bit_table[(d & 0x3)];
		dst[i * 4 + 1] = max2769_bit_table[((d >> 2) & 0x3)];
		dst[i * 4 + 2] = max2769_bit_table[((d >> 4) & 0x3)];
		dst[i * 4 + 3] = max2769_bit_table[((d >> 6) & 0x3)];

		magnitude_total_count += 4;
		sign_count += ((d >> 1) & 0x01) + ((d >> 3) & 0x01) + ((d >> 5) & 0x01) + ((d >> 7) & 0x01);
		magnitude_count += ((d >> 0) & 0x01) + ((d >> 2) & 0x01) + ((d >> 4) & 0x01) + ((d >> 6) & 0x01);
	}
	if (magnitude_total_count > 0 && magnitude_total_count % (4 * 16368000) == 0)
		SDRPRINTF("sign:%.3f mag:%.3f\n", (float)sign_count / magnitude_total_count,
		(float)magnitude_count / magnitude_total_count);

}

int fmax2769_net_pushtomembuf()
{
	char max2769_bit_table[4] = { 1,3,-1,-5 };  //符号位为，则adc数据为负数
	int64_t nout = 0;
	int rcv_n = 0;
	uint64_t cur_buff_front;

	
	//5ms 需要读取
	//while (y = recv(conn, buff, FILE_BUFFSIZE, 0))
	int rt;
	static char buff[FILE_BUFFSIZE/4],buff_conv[FILE_BUFFSIZE];
	static int recv_count = 0;
	
	rt = recv(conn, buff, FILE_BUFFSIZE / 4, 0);
	//fixme: 如果超过了，分片
	/*
	uint64_t membuffloc = dtype * buffloc % (MEMBUFFLEN*dtype*FILE_BUFFSIZE);
	int nout;

	n = dtype * n;
	nout = (int)((membuffloc + n) - (MEMBUFFLEN*dtype*FILE_BUFFSIZE));

	mlock(hbuffmtx);
	if (ftype == FTYPE1) {
		if (nout>0) {
			memcpy(expbuf, &sdrstat.buff[membuffloc], n - nout);
			memcpy(&expbuf[(n - nout)], &sdrstat.buff[0], nout);
		}
		else {
			memcpy(expbuf, &sdrstat.buff[membuffloc], n);
		}
	}*/

	if (rt > 0)
	{
		Max2769_convert_stereo(buff, buff_conv, rt);//buff_conv的长度为4*rt
		rcv_n = 4 * rt;
		cur_buff_front = sdrstat.buff_front % sdrstat.buffsize;
		mlock(hbuffmtx);
		nout = (cur_buff_front + rcv_n) - sdrstat.buffsize;
		if (nout > 0)
		{
			//Max2769_convert_stereo(buff, (char*)&sdrstat.buff[sdrstat.buff_front%sdrstat.buffsize], (sdrstat.buffsize-nout)/4);
			//Max2769_convert_stereo(&buff[(sdrstat.buffsize - nout) / 4], (char*)&sdrstat.buff[0], nout/4);
			memcpy((char*)&sdrstat.buff[cur_buff_front], &buff_conv[0], rcv_n-nout);
			memcpy((char*)&sdrstat.buff[0], &buff_conv[rcv_n - nout], nout);

		}else
			memcpy((char*)&sdrstat.buff[cur_buff_front], buff_conv, rcv_n);
		
		unmlock(hbuffmtx);

		mlock(hreadmtx);
		sdrstat.buff_front = sdrstat.buff_front + rcv_n;
		//sdrstat.buffcnt = (uint64_t)(sdrstat.buff_front / sdrstat.buffsize) ;
		sdrstat.buffcnt = (uint64_t)(sdrstat.buff_front / FILE_BUFFSIZE);
		//sdrstat.buffcnt++;
		//sdrstat.buff_front = (sdrstat.buff_front + sdrini.dtype[0] * FILE_BUFFSIZE) % sdrstat.buffsize;
		unmlock(hreadmtx);
		//SDRPRINTF("net_recv:%d buff_front=%d buffcnt=%d\n",rt, sdrstat.buff_front, sdrstat.buffcnt);
	}
	
	
	
	return 0;



}
int max2769_net_quit()
{
	closesocket(conn);
	SDRPRINTF("SOCKET closed\n");
	return 0;
}

#endif




/* sdr receiver initialization -------------------------------------------------
* receiver initialization, memory allocation, file open
* args   : sdrini_t *ini    I   sdr initialization struct
* return : int                  status 0:okay -1:failure
*-----------------------------------------------------------------------------*/
extern int rcvinit(sdrini_t *ini)
{
#ifdef WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(1,0),&wsaData);
#endif

    /* FFT initialization */
    fftwf_init_threads();

    sdrstat.buff=sdrstat.buff2=sdrstat.tmpbuff=NULL;

    switch (ini->fend) {
#ifdef STEREO
    /* NSL STEREO */
    case FEND_STEREO: 
        if (stereo_init()<0) return -1; /* stereo initialization */
        
        /* frontend buffer size */
        sdrstat.fendbuffsize=STEREO_DATABUFF_SIZE; /* frontend buff size */
        sdrstat.buffsize=STEREO_DATABUFF_SIZE*MEMBUFFLEN; /* total */

        /* memory allocation */
        sdrstat.buff=(uint8_t*)malloc(sdrstat.buffsize);
        if (NULL==sdrstat.buff) {
            SDRPRINTF("error: failed to allocate memory for the buffer\n");
            return -1;
        }

#ifdef STEREOV26
        /* memory allocation */
        sdrstat.tmpbuff=(uint8_t*)malloc(STEREO_PKT_SIZE*STEREO_NUM_BLKS);
        if (NULL==sdrstat.tmpbuff) {
            SDRPRINTF("error: failed to allocate memory for the buffer\n");
            return -1;
        }
        if (STEREO_ConnectEndPoint(L1_EP,sdrstat.tmpbuff,
                STEREO_PKT_SIZE*STEREO_NUM_BLKS)<0) {
            SDRPRINTF("error: STEREO_ConnectEndPoint\n");
            return -1;
        }
#else
        if (STEREO_GrabInit()<0) {
            SDRPRINTF("error: STEREO_GrabInit\n");
            return -1;
        }
#endif /* STEREOV26 */
        break;
    /* STEREO Binary File */
    case FEND_FSTEREO: 
        /* IF file open */
        if ((ini->fp1 = fopen(ini->file1,"rb"))==NULL){
            SDRPRINTF("error: failed to open file : %s\n",ini->file1);
            return -1;
        }
        sdrstat.fendbuffsize=STEREO_DATABUFF_SIZE; /* frontend buff size */
        sdrstat.buffsize=STEREO_DATABUFF_SIZE*MEMBUFFLEN; /* total */

        /* memory allocation */
        sdrstat.buff=(uint8_t*)malloc(sdrstat.buffsize);
        if (NULL==sdrstat.buff) {
            SDRPRINTF("error: failed to allocate memory for the buffer\n");
            return -1;
        }
        break;
#endif
#ifdef GN3S
    /* SiGe GN3S v2/v3 */
    case FEND_GN3SV2: 
    case FEND_GN3SV3: 
        if (gn3s_init()<0) return -1; /* GN3S initialization */

        if (ini->fend==FEND_GN3SV2)
            sdrstat.fendbuffsize=GN3S_BUFFSIZE/2; /* frontend buff size */
        if (ini->fend==FEND_GN3SV3)
            sdrstat.fendbuffsize=GN3S_BUFFSIZE; /* frontend buff size */

        sdrstat.buffsize=GN3S_BUFFSIZE*MEMBUFFLEN; /* total */

        /* memory allocation */
        sdrstat.buff=(uint8_t*)malloc(sdrstat.buffsize);
        if (NULL==sdrstat.buff) {
            SDRPRINTF("error: failed to allocate memory for the buffer\n");
            return -1;
        }
        break;
    /* GN3S Binary File */
    case FEND_FGN3SV2:
    case FEND_FGN3SV3:
        /* IF file open */
        if ((ini->fp1 = fopen(ini->file1,"rb"))==NULL){
            SDRPRINTF("error: failed to open file : %s\n",ini->file1);
            return -1;
        }
        
        if (ini->fend==FEND_GN3SV2)
            sdrstat.fendbuffsize=GN3S_BUFFSIZE/2; /* frontend buff size */
        if (ini->fend==FEND_GN3SV3)
            sdrstat.fendbuffsize=GN3S_BUFFSIZE; /* frontend buff size */

        sdrstat.buffsize=GN3S_BUFFSIZE*MEMBUFFLEN; /* total */

        /* memory allocation */
        sdrstat.buff=(uint8_t*)malloc(sdrstat.buffsize);
        if (NULL==sdrstat.buff) {
            SDRPRINTF("error: failed to allocate memory for the buffer\n");
            return -1;
        }
        break;
#endif
#ifdef BLADERF
    /* Nuand bladeRF */
    case FEND_BLADERF:
        if (bladerf_init()<0) return -1; /* bladeRF initialization */

        sdrstat.fendbuffsize=BLADERF_DATABUFF_SIZE; /* frontend buff size */
        sdrstat.buffsize=2*BLADERF_DATABUFF_SIZE*MEMBUFFLEN; /* total */

        /* memory allocation */
        sdrstat.buff=(uint8_t*)malloc(sdrstat.buffsize);
        if (NULL==sdrstat.buff) {
            SDRPRINTF("error: failed to allocate memory for the buffer\n");
            return -1;
        }
        break;
    /* BladeRF Binary File */
    case FEND_FBLADERF:
        /* IF file open */
        if ((ini->fp1 = fopen(ini->file1,"rb"))==NULL){
            SDRPRINTF("error: failed to open file : %s\n",ini->file1);
            return -1;
        }

        sdrstat.fendbuffsize=BLADERF_DATABUFF_SIZE; /* frontend buff size */
        sdrstat.buffsize=2*BLADERF_DATABUFF_SIZE*MEMBUFFLEN; /* total */

        /* memory allocation */
        sdrstat.buff=(uint8_t*)malloc(sdrstat.buffsize);
        if (NULL==sdrstat.buff) {
            SDRPRINTF("error: failed to allocate memory for the buffer\n");
            return -1;
        }
        break;
#endif
#ifdef RTLSDR
    /* RTL-SDR */
    case FEND_RTLSDR:
        if (rtlsdr_init()<0) return -1; /* rtlsdr initialization */

        /* frontend buffer size */
        sdrstat.fendbuffsize=RTLSDR_DATABUFF_SIZE; /* frontend buff size */
        sdrstat.buffsize=2*RTLSDR_DATABUFF_SIZE*MEMBUFFLEN; /* total */

        /* memory allocation */
        sdrstat.buff=(uint8_t*)malloc(sdrstat.buffsize);
        if (NULL==sdrstat.buff) {
            SDRPRINTF("error: failed to allocate memory for the buffer\n");
            return -1;
        }
        break;
    /* RTL-SDR Binary File */
    case FEND_FRTLSDR:
        /* IF file open */
        if ((ini->fp1 = fopen(ini->file1,"rb"))==NULL){
            SDRPRINTF("error: failed to open file : %s\n",ini->file1);
            return -1;
        }

        /* frontend buffer size */
        sdrstat.fendbuffsize=RTLSDR_DATABUFF_SIZE; /* frontend buff size */
        sdrstat.buffsize=2*RTLSDR_DATABUFF_SIZE*MEMBUFFLEN; /* total */

        /* memory allocation */
        sdrstat.buff=(uint8_t*)malloc(sdrstat.buffsize);
        if (NULL==sdrstat.buff) {
            SDRPRINTF("error: failed to allocate memory for the buffer\n");
            return -1;
        }
        break;
#endif
    /* File */
    case FEND_FILE:
        /* IF file open (FILE1) */
        if ((ini->fp1 = fopen(ini->file1,"rb"))==NULL){
            SDRPRINTF("error: failed to open file(FILE1): %s\n",ini->file1);
            return -1;
        }
        /* IF file open (FILE2) */
        if (strlen(ini->file2)!=0) {
            if ((ini->fp2 = fopen(ini->file2,"rb"))==NULL){
                SDRPRINTF("error: failed to open file(FILE2): %s\n",ini->file2);
                return -1;
            }
        }
        /* frontend buffer size */
        sdrstat.fendbuffsize=FILE_BUFFSIZE; /* frontend buff size */
        sdrstat.buffsize=FILE_BUFFSIZE*MEMBUFFLEN; /* total */

        /* memory allocation */
        if (ini->fp1!=NULL) {
            sdrstat.buff=(uint8_t*)malloc(ini->dtype[0]*sdrstat.buffsize);
            if (NULL==sdrstat.buff) {
                SDRPRINTF("error: failed to allocate memory for the buffer\n");
                return -1;
            }
        }
        if (ini->fp2!=NULL) {
            sdrstat.buff2=(uint8_t*)malloc(ini->dtype[1]*sdrstat.buffsize);
            if (NULL==sdrstat.buff2) {
                SDRPRINTF("error: failed to allocate memory for the buffer\n");
                return -1;
            }
        }
        break;
#ifdef MAX2769_NET
	case FEND_MAX2769_NET:
		max2769_net_init();
		/* frontend buffer size */
		sdrstat.fendbuffsize = FILE_BUFFSIZE; /* frontend buff size */
		sdrstat.buffsize = FILE_BUFFSIZE * MEMBUFFLEN; /* total */

		/* memory allocation */
		
			sdrstat.buff = (uint8_t*)malloc(ini->dtype[0] * sdrstat.buffsize);
			if (NULL == sdrstat.buff) {
				SDRPRINTF("error: failed to allocate memory for the buffer\n");
				return -1;
			}
		
		
		break;
#endif
    default:
        return -1;
    }
    return 0;
}
/* stop front-end --------------------------------------------------------------
* stop grabber of front end
* args   : sdrini_t *ini    I   sdr initialization struct
* return : int                  status 0:okay -1:failure
*-----------------------------------------------------------------------------*/
extern int rcvquit(sdrini_t *ini)
{
    switch (ini->fend) {
#ifdef STEREO
    /* NSL stereo */
    case FEND_STEREO: 
        stereo_quit();
        break;
#endif
#ifdef GN3S
    /* SiGe GN3S v2/v3 */
    case FEND_GN3SV2:
    case FEND_GN3SV3:
        gn3s_quit();
        break;
#endif
#ifdef BLADERF
    /* Nuand bladeRF */
    case FEND_BLADERF:
        bladerf_quit();
        break;
#endif
#ifdef RTLSDR
    /* RTL-SDR */
    case FEND_RTLSDR:
        rtlsdr_quit();
        break;
#endif
#ifdef MAX2769_NET
	case FEND_MAX2769_NET:
		max2769_net_quit();
		break;
#endif
    /* Front End Binary File */
    case FEND_FSTEREO:
    case FEND_FGN3SV2:
    case FEND_FGN3SV3:
    case FEND_FBLADERF:
    case FEND_FRTLSDR:
        if (ini->fp1!=NULL) fclose(ini->fp1); ini->fp1=NULL;
        break;
    /* File */
    case FEND_FILE:
        if (ini->fp1!=NULL) fclose(ini->fp1); ini->fp1=NULL;
        if (ini->fp2!=NULL) fclose(ini->fp2); ini->fp2=NULL;
        break;
    default:
        return -1;
    }
    /* free memory */
    if (NULL!=sdrstat.buff)    free(sdrstat.buff);    sdrstat.buff=NULL;
    if (NULL!=sdrstat.buff2)   free(sdrstat.buff2);   sdrstat.buff2=NULL;
    if (NULL!=sdrstat.tmpbuff) free(sdrstat.tmpbuff); sdrstat.tmpbuff=NULL;
    return 0;
}
/* start grabber ---------------------------------------------------------------
* start grabber of front end
* args   : sdrini_t *ini    I   sdr initialization struct
* return : int                  status 0:okay -1:failure
*-----------------------------------------------------------------------------*/
extern int rcvgrabstart(sdrini_t *ini)
{
    switch (ini->fend) {
#ifdef STEREO
    /* NSL stereo */
    case FEND_STEREO: 
#ifndef STEREOV26
        if (STEREO_GrabStart()<0) {
            SDRPRINTF("error: STEREO_GrabStart\n");
            return -1;
        }
#endif
#endif
    default:
        return 0;

    }
    return 0;
}
/* grab current data -----------------------------------------------------------
* push data to memory buffer from front end
* args   : sdrini_t *ini    I   sdr initialization struct
* return : int                  status 0:okay -1:failure
*-----------------------------------------------------------------------------*/
extern int rcvgrabdata(sdrini_t *ini)
{
    unsigned long buffcnt=0;

    switch (ini->fend) {
#ifdef STEREO
    /* NSL stereo */
    case FEND_STEREO: 
#ifdef STEREOV26
        buffcnt=(unsigned int)(sdrstat.buffcnt%MEMBUFFLEN);
        if (STEREO_ReapPacket(L1_EP,buffcnt, 300)<0) {
            SDRPRINTF("error: STEREO Buffer overrun...\n");
            return -1;
        }
#else
        if (STEREO_RefillDataBuffer()<0) {
            SDRPRINTF("error: STEREO Buffer overrun...\n");
            return -1;
        }
#endif
        stereo_pushtomembuf(); /* copy to membuffer */
        break;
    /* STEREO Binary File */
    case FEND_FSTEREO: 
        fstereo_pushtomembuf(); /* copy to membuffer */
        sleepms(5);
        break;
#endif
#ifdef GN3S
    /* SiGe GN3S v2/v3 */
    case FEND_GN3SV2:
    case FEND_GN3SV3:
        if (gn3s_pushtomembuf()<0) {
            SDRPRINTF("error: GN3S Buffer overrun...\n");
            return -1;
        }
        break;
    /* GN3S Binary File */
    case FEND_FGN3SV2:
    case FEND_FGN3SV3: 
        fgn3s_pushtomembuf(); /* copy to membuffer */
        sleepms(5);
        break;
#endif
#ifdef BLADERF
    /* Nuand BladeRF */
    case FEND_BLADERF:
        if (bladerf_start()<0) {
            SDRPRINTF("error: bladeRF...\n");
            return -1;
        }
        break;
    /* BladeRF Binary File */
    case FEND_FBLADERF: 
        fbladerf_pushtomembuf(); /* copy to membuffer */
        sleepms(5);
        break;
#endif
#ifdef RTLSDR
    /* RTL-SDR */
    case FEND_RTLSDR:
        if (rtlsdr_start()<0) {
            SDRPRINTF("error: rtlsdr...\n");
            return -1;
        }
        break;
    /* RTL-SDR Binary File */
    case FEND_FRTLSDR: 
        frtlsdr_pushtomembuf(); /* copy to membuffer */
        sleepms(5);
        break;/* File */
#endif
    case FEND_FILE:
        file_pushtomembuf(); /* copy to membuffer */
        sleepms(READFILE_DELAYMS);   //[ws] sleepms(5);
        break;
#ifdef MAX2769_NET
	case FEND_MAX2769_NET:
		fmax2769_net_pushtomembuf();
		sleepms(5);
		break;
#endif
    default:
        return -1;
    }
    return 0;
}
/* grab current buffer ---------------------------------------------------------
* get current data buffer from memory buffer
* args   : sdrini_t *ini    I   sdr initialization struct
*          uint64_t buffloc I   buffer location
*          int    n         I   number of grab data 
*          int    ftype     I   front end type (FTYPE1 or FTYPE2)
*          int    dtype     I   data type (DTYPEI or DTYPEIQ)
*          char   *expbuff  O   extracted data buffer
* return : int                  status 0:okay -1:failure
*-----------------------------------------------------------------------------*/
extern int rcvgetbuff(sdrini_t *ini, uint64_t buffloc, int n, int ftype,
                      int dtype, char *expbuf)
{
    switch (ini->fend) {
#ifdef STEREO
    /* NSL STEREO */
    case FEND_STEREO: 
        stereo_getbuff(buffloc,n,dtype,expbuf);
        break;
    /* STEREO Binary File */
    case FEND_FSTEREO: 
        stereo_getbuff(buffloc,n,dtype,expbuf);
        break;
#endif
#ifdef GN3S
    /* SiGe GN3S v2 */
    case FEND_GN3SV2:
        gn3s_getbuff_v2(buffloc,n,dtype,expbuf);
        break;
    /* SiGe GN3S v3 */
    case FEND_GN3SV3:
        gn3s_getbuff_v3(buffloc,n,dtype,expbuf);
        break;
    /* GN3Sv2/v3 Binary File */
    case FEND_FGN3SV2: 
    case FEND_FGN3SV3: 
        fgn3s_getbuff(buffloc,n,dtype,expbuf);
        break;
#endif
#ifdef BLADERF
    /* Nuand BladeRF */
    case FEND_BLADERF:
        bladerf_getbuff(buffloc,n,expbuf);
        break;    
    /* BladeRF Binary File */
    case FEND_FBLADERF: 
        bladerf_getbuff(buffloc,n,expbuf);
        break;
#endif
#ifdef RTLSDR
    /* RTL-SDR */
    case FEND_RTLSDR:
        rtlsdr_getbuff(buffloc,n,expbuf);
        break;    
    /* RTL-SDR Binary File */
    case FEND_FRTLSDR: 
        rtlsdr_getbuff(buffloc,n,expbuf);
        break;
#endif
    /* File */
    case FEND_FILE:
	case FEND_MAX2769_NET:
        file_getbuff(buffloc,n,ftype,dtype,expbuf);
        break;

    default:
        return -1;
    }
    return 0;
}
/* push data to memory buffer --------------------------------------------------
* post-processing function: push data to memory buffer
* args   : none
* return : none
*-----------------------------------------------------------------------------*/
extern void file_pushtomembuf(void) 
{
    size_t nread1=0,nread2=0;

//	if(sdrstat.buffcnt%MEMBUFFLEN)

    mlock(hbuffmtx);
    if(sdrini.fp1!=NULL) {
        nread1=fread(&sdrstat.buff[(sdrstat.buffcnt%MEMBUFFLEN)*sdrini.dtype[0]*FILE_BUFFSIZE],
			1,sdrini.dtype[0]*FILE_BUFFSIZE,
            sdrini.fp1);
    }
    if(sdrini.fp2!=NULL) {
        nread2=fread(&sdrstat.buff2[(sdrstat.buffcnt%MEMBUFFLEN)*sdrini.dtype[1]*FILE_BUFFSIZE],
			1,sdrini.dtype[1]*FILE_BUFFSIZE,
            sdrini.fp2);
    }
    unmlock(hbuffmtx);

    if ((sdrini.fp1!=NULL&&(int)nread1<sdrini.dtype[0]*FILE_BUFFSIZE)||
        (sdrini.fp2!=NULL&&(int)nread2<sdrini.dtype[1]*FILE_BUFFSIZE)) {
        sdrstat.stopflag=ON;
        SDRPRINTF("end of file!\n");
    }

    mlock(hreadmtx);
	sdrstat.buffcnt++;
	sdrstat.buff_front = (sdrstat.buff_front+sdrini.dtype[0]*FILE_BUFFSIZE)%sdrstat.buffsize;
    unmlock(hreadmtx);
}
/* get current data buffer from IF file ----------------------------------------
* post-processing function: get current data buffer from memory buffer
* args   : uint64_t buffloc I   buffer location
*          int    n         I   number of grab data 
*          int    ftype     I   front end type (FTYPE1 or FTYPE2)
*          int    dtype     I   data type (DTYPEI or DTYPEIQ)
*          char   *expbuff  O   extracted data buffer
* return : none
*-----------------------------------------------------------------------------*/
extern void file_getbuff(uint64_t buffloc, int n, int ftype, int dtype, 
                         char *expbuf)
{
    uint64_t membuffloc=dtype*buffloc%(MEMBUFFLEN*dtype*FILE_BUFFSIZE);
    int nout;

    n=dtype*n;
    nout=(int)((membuffloc+n)-(MEMBUFFLEN*dtype*FILE_BUFFSIZE));

    mlock(hbuffmtx);
    if (ftype==FTYPE1) {
        if (nout>0) {
            memcpy(expbuf,&sdrstat.buff[membuffloc],n-nout);
            memcpy(&expbuf[(n-nout)],&sdrstat.buff[0],nout);
        } else {
            memcpy(expbuf,&sdrstat.buff[membuffloc],n);
        }
    }
    if (ftype==FTYPE2) {
        if (nout>0) {
            memcpy(expbuf,&sdrstat.buff2[membuffloc],n-nout);
            memcpy(&expbuf[(n-nout)],&sdrstat.buff2[0],nout);
        } else {
            memcpy(expbuf,&sdrstat.buff2[membuffloc],n);
        }
    }
    unmlock(hbuffmtx);
}
