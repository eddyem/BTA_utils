/*
 * client_streaming.c - example of streaming client
 *
 * Copyright 2013 Edward V. Emelianoff <eddy@sao.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */
#include <netdb.h> // addrinfo
#include <stdio.h> // printf etc
#include <string.h> // memset, strdup, strlen
#include <stdlib.h> // exit
#include <unistd.h> // close, read, sleep
#include <fcntl.h> // fcntl
#include <arpa/inet.h> // inet_ntop
#include <math.h>
#include <assert.h>
#include <json/json.h>

#define PORT "12345"
#define RESOURSE "GET /bta_par HTTP/1.1\r\n"

typedef enum{
	T_STRING,	// string
	T_DMS,		// degrees:minutes:seconds
	T_HMS,		// hours:minutes:seconds
	T_DOUBLE	// double
} otype;

/*
#define defpar(val)  const char* val = #val
defpar(mtime);
defpar(sidtime);
defpar(telmode);
defpar(telfocus);
defpar(target);
defpar(p2mode);
defpar(eqcoor);
defpar(horcoor);
defpar(valsens);
defpar(diff);
defpar(vel);
defpar(corr);
defpar(meteo);
#undef defpar
*/
/*
{
"M_time": "22:49:13.61",
"JDate": 2456367.284184,
"S_time": "09:09:07.34",
"Tel_Mode": "Stopping",
"Tel_Focus": "Prime",
"ValFoc": 43.54,
"Tel_Taget": "Nest",
"P2_Mode": "Stop",
"CurAlpha": "00:26:06.78",
"CurDelta": "+25:34:34.0",
"SrcAlpha": "06:03:08.53",
"SrcDelta": "+30:00:00.0",
"InpAlpha": "06:03:08.53",
"InpDelta": "+30:00:00.0",
"TelAlpha": "04:23:09.30",
"TelDelta": "+25:34:41.3",
"InpAzim": "+085:27:36.4",
"InpZenD": "39:01:06.5",
"CurAzim": "+136:27:11.0",
"CurZenD": "97:21:25.3",
"CurPA": "033:32:49.7",
"SrcPA": "056:23:33.0",
"InpPA": "056:23:33.0",
"TelPA": "052:39:24.3",
"ValAzim": "+097:37:52.4",
"ValZenD": "59:37:00.2",
"ValP2": "310:24:30.2",
"ValDome": "+180:16:47.0",
"DiffAzim": "+000:00:00.0",
"DiffZenD": "+00:00:00.0",
"DiffP2": "+000:00:00.6",
"DiffDome": "-082:38:54.6",
"VelAzim": "+00:00:00.0",
"VelZenD": "-00:00:00.0",
"VelP2": "+00:00:00.0",
"VelPA": "+00:00:00.0",
"VelDome": "+00:00:00.0",
"CorrAlpha": "+0:00:00.00",
"CorrDelta": "+0:00:00.0",
"CorrAzim": "+0:00:00.0",
"CorrZenD": "+0:00:00.0",
"ValTind": +02.5,
"ValTmir": +01.7,
"ValPres": 590.3,
"ValWind": 06.6,
"Blast10": 1.4,
"Blast15": 3.4,
"ValHumd": 50.0
}
*/

/**
 * Get double parameter
 * @param jobj - json record with double parameter
 * @return NULL in case of error or allocated double value (MUST BE FREE LATER)
 */
double* get_jdouble(json_object *jobj){
	enum json_type type = json_object_get_type(jobj);
	double val, *ret;
	switch(type){
		case json_type_double:
			val = json_object_get_double(jobj);
		break;
		case json_type_int:
			val = json_object_get_int(jobj);
		break;
		default:
			fprintf(stderr, "Wrong value! Get non-number!\n");
			return NULL;
	}
	ret = malloc(sizeof(double));
	assert(ret);
	memcpy(ret, &val, sizeof(double));
	return ret;
}
double strtod(const char *nptr, char **endptr);

char *atodbl(char *str, double *d){
	char *eptr;
	*d = strtod(str, &eptr);
	if(eptr == str) return NULL;
	return eptr;
}

/**
 * get string parameter
 * @param jobj - json record
 * @return string or NULL in case of error
 */

char *get_jstr(json_object *jobj){
	enum json_type type = json_object_get_type(jobj);
	if(type != json_type_string) return NULL;
	return (char*)json_object_get_string(jobj);
}

double *get_jdhms(json_object *jobj){
	char *jptr = get_jstr(jobj);
	char *str = jptr, *endln = str + strlen(str);
	double h,m,s, sgn;
	#define GetVal(x) do{if(str >= endln) return NULL; str = atodbl(str, &x); if(!str)return NULL; str++;}while(0)
	GetVal(h);
	GetVal(m);
	GetVal(s);
	#undef GetVal
	sgn = (h < 0.) ? -1. : 1.;
	h = fabs(h);
	double *ret = malloc(sizeof(double));
	assert(ret);
	*ret = sgn * (h + m / 60. + s / 3600.);
	return ret;
}

/**
 * get parameter from json object
 * @param jobj - json records
 * @param par - parameter to find
 * @param type - type of par
 * @return dinamycally allocated pointer to char (if T_STRING) or double (for other types);
 * 		if type is angle or time, its value converts from DMS/HMS to double
 * 		in case of parameter absense or error function returns NULL
 */
void *get_json_par(json_object *jobj, char *par, otype type){
	json_object *o = json_object_object_get(jobj, par);
	void *ret;
	if(!o) return NULL;
	switch(type){
		case T_DOUBLE:
			ret = (void*)get_jdouble(o);
		break;
		case T_DMS:
		case T_HMS:
			ret = (void*)get_jdhms(o);
		break;
		case T_STRING:
		default:
			ret = (void*)strdup(get_jstr(o));
	}
	json_object_put(o); // free object's memory
	return ret;
}


/**
 * set non-blocking flag to socket
 * @param sock - socket fd
 *
void setnonblocking(int sock){
	int opts = fcntl(sock, F_GETFL);
	if(opts < 0){
		perror("fcntl(F_GETFL)");
		exit(-1);
	}
	opts = (opts | O_NONBLOCK);
	if (fcntl(sock,F_SETFL,opts) < 0) {
		perror("fcntl(F_SETFL)");
		exit(-1);
	}
	return;
}*/

/**
 * wait for answer from server
 * @param sock - socket fd
 * @return 0 in case of error or timeout, 1 in case of socket ready
 */
int waittoread(int sock){
	fd_set fds;
	struct timeval timeout;
	int rc;
	timeout.tv_sec = 1; // wait not more than 1 second
	timeout.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(sock, &fds);
	rc = select(sock+1, &fds, NULL, NULL, &timeout);
	if(rc < 0){
		perror("select failed");
		return 0;
	}
	if(rc > 0 && FD_ISSET(sock, &fds)) return 1;
	return 0;
}

int main(int argc, char *argv[]){
	size_t BUFSZ = 1024;
	int sockfd = 0;
	char *recvBuff = calloc(BUFSZ, 1);
	assert(recvBuff);
	struct addrinfo h, *r, *p;
	memset(&h, 0, sizeof(h));
	h.ai_family = AF_INET;
	h.ai_socktype = SOCK_STREAM;
	h.ai_flags = AI_CANONNAME;
	char *host = "localhost";
	if(argc > 1) host = argv[1];
	char *port = PORT;
	if(argc > 2) port = argv[2];
	if(getaddrinfo(host, port, &h, &r)){perror("getaddrinfo"); return -1;}
	struct sockaddr_in *ia = (struct sockaddr_in*)r->ai_addr;
	char str[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(ia->sin_addr), str, INET_ADDRSTRLEN);
	printf("canonname: %s, port: %u, addr: %s\n", r->ai_canonname, ntohs(ia->sin_port), str);
	for(p = r; p; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("socket");
			continue;
		}
		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("connect");
			continue;
		}
		break; // if we get here, we must have connected successfully
	}
	if(p == NULL){
		// looped off the end of the list with no connection
		fprintf(stderr, "failed to connect\n");
		return -1;
	}
	freeaddrinfo(r);
	//setnonblocking(sockfd);
	char *msg;
	char *res = RESOURSE;
	if(argc == 4) res = argv[3];
	msg = strdup(res);
	do{
		if(send(sockfd, msg, strlen(msg), 0) != strlen(msg)){perror("send"); return -1;}
		if(!waittoread(sockfd)) return -1;
		int offset = 0, n = 0;
		do{
			if(offset >= BUFSZ){
				BUFSZ += 1024;
				recvBuff = realloc(recvBuff, BUFSZ);
				assert(recvBuff);
				fprintf(stderr, "Buffer reallocated, new size: %zd\n", BUFSZ);
			}
			n = read(sockfd, &recvBuff[offset], BUFSZ - offset);
			if(!n) break;
			if(n < 0){
				perror("read");
				return -1;
			}
			offset += n;
		}while(waittoread(sockfd));
		if(!offset){
			fprintf(stderr, "Socket closed\n");
			return 0;
		}
		printf("read %d bytes\n", offset);
		recvBuff[offset] = 0;
		printf("Received data:\n-->%s<--\n", recvBuff);
		fflush(stdout);
		// here we do something with values we got
		// for example - parce them & print
		json_object *jobj = json_tokener_parse(recvBuff);
		if(!jobj){
			fprintf(stderr, "Can't parse json!\n");
			return -1;
		}
		double *focval = (double*)get_json_par(jobj, "ValFoc", T_DOUBLE);
		double *valaz = (double*)get_json_par(jobj, "ValAzim", T_DMS);
		double *valzd = (double*)get_json_par(jobj, "ValZenD", T_DMS);
		double *valdaz = (double*)get_json_par(jobj, "ValDome", T_DMS);
		double *valP2 = (double*)get_json_par(jobj, "ValP2", T_DMS);
		#define prntdbl(name, val) do{if(val) printf("%s = %g\n", name, *val); free(val);}while(0)
		prntdbl("Focus value", focval);
		prntdbl("Telescope azimuth", valaz);
		prntdbl("Telescope zenith distance", valzd);
		prntdbl("Dome azimuth", valdaz);
		prntdbl("P2 angle", valP2);
		#undef prntdbl
		json_object_put(jobj); // destroy jobj
		sleep(1);
	}while(1);
	return 0;
}
