/*
 * This file is part of the 3steppers project.
 * Copyright 2022 Edward V. Emelianov <edward.emelianoff@gmail.com>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <limits.h> // PATH_MAX

#include "bta_meteo_modbus.h"
#include "bta_shdata.h"
#include "usefull_macros.h"

// length of answer packet (addr fno len h l crch crcl)
#define ANS_LEN     7
// length of request packet
#define REQ_LEN     8

// meteo registers (from datasheet)
#define REG_WSPEED  30001
#define REG_WDIR    30201
#define REG_TAIR    30401
#define REG_HUM     30601
#define REG_DEW     30701
#define REG_PRES    30801

extern int gotsegm;
static int portfd = -1;

/*
 * maximum Modbus packet size. By the standard is 300 bytes
 */
#define MODBUS_MAX_PACKET_SIZE 300

static uint16_t crc16_table[] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040,
};

static int crc_check(uint8_t *buffer, int length){
    uint8_t byte;
    uint16_t crc = 0xFFFF;
    int valid_crc;

    #ifdef EBUG
    printf("buffer: ");
    for(int i = 0; i < length; ++i) printf("%02x ", buffer[i]);
    printf("\n");
    #endif

    while (length-- > 2) {
        byte = *buffer++ ^ crc;
        crc >>= 8;
        crc ^= crc16_table[byte];
    }
    valid_crc = (crc >> 8) == buffer[1] && (crc & 0xFF) == buffer[0];
    if(!valid_crc) DBG("CRC BAD");
    return valid_crc;
}

static int opendev(int fd){
    struct termios tty;
    if(tcgetattr(fd, &tty) < 0){
        WARN("tcgetattr");
        return FALSE;
    }
    tty.c_cflag = CS8 | CREAD | CLOCAL | B19200;
    tty.c_cc[VTIME] = 0;
    tty.c_cc[VMIN] = 0;
//    cfsetispeed(&tty, speed);
//    cfsetospeed(&tty, speed);
    if(tcsetattr(fd, TCSANOW, &tty) < 0){
        WARN("tcsetattr");
        return FALSE;
    }
    return TRUE;
}

int connect2tty(){
    int fd = -1;
    char fname[PATH_MAX];
    char *names[2] = {"USB", "ACM"};
    for(int j = 0; j < 2; ++j){
        for(int i = 0; i < 256; ++i){
            snprintf(fname, PATH_MAX, "/dev/tty%s%d", names[j], i);
            if((fd = open(fname, O_RDONLY)) > 0){
                LOG("Try to open %s", fname);
                if(opendev(fd)){
                    portfd = fd;
                    LOG("Success");
                    return TRUE;
                }
                LOG("Error");
            }
        }
    }
    LOG("USB to serial not found");
    return FALSE;
}

#define WSFLAG (1 << 0)
#define TFLAG  (1 << 1)
#define HFLAG  (1 << 2)
#define PFLAG  (1 << 3)
//#define WDFLAG (1 << 4)
// all flags set
//#define ALLFLAGS (0x1f)
#define ALLFLAGS (0x0f)

params_ans check_meteo_params(){
    if(portfd < 0) return ANS_LOSTCONN;
    int n_bytes = -1, res, size = 0;
    uint8_t meteoflags = 0;
    static uint16_t lastpar = 0;
    unsigned char buffer[MODBUS_MAX_PACKET_SIZE];
    struct timeval timeout;
    fd_set set;
    //time_t tstart = time(NULL);
    int ctr = 30; // max 30 tries
    while(ctr--){
        FD_ZERO(&set);
        FD_SET(portfd, &set);
        timeout.tv_sec = 0;
        timeout.tv_usec = 1500; // 1.5ms pause

        if((res = select(portfd + 1, &set, NULL, NULL, &timeout)) < 0 && errno != EINTR){
            WARN("lost connection (select)");
            return ANS_LOSTCONN;
        }
        if(res > 0){
            if ((n_bytes = read(portfd, buffer + size, MODBUS_MAX_PACKET_SIZE - size)) < 0){
        	WARN("lost connection (read)");
                return ANS_LOSTCONN;
    	    }
            size += n_bytes;
            if(n_bytes) continue;
        }else if(size == 0){
            //DBG("no data");
            return ANS_NODATA;
        }
        DBG("Ctr=%d, size=%d, res=%d", ctr, size, res);
        // read all or end of packet
        if(size > 0 && res == 0 && (size == REQ_LEN || size == ANS_LEN || size == REQ_LEN + ANS_LEN)){
            int16_t val = 0;
            if(size == REQ_LEN){ // request from master
                if(!crc_check(buffer, size)){
                    lastpar = 0;
                    continue;
                }
                DBG("request");
                lastpar = buffer[2] << 8 | buffer[3];
                continue;
            }
            if(size == ANS_LEN){
                if(!crc_check(buffer, size)){
                    lastpar = 0;
                    continue;
                }
                DBG("answer");
                val = buffer[3] << 8 | buffer[4];
            }else if(size == REQ_LEN + ANS_LEN){
                if(!crc_check(buffer, REQ_LEN) || !crc_check(buffer+REQ_LEN, ANS_LEN)){
                    lastpar = 0;
                    continue;
                }
                DBG("both request and answer");
                lastpar = buffer[2] << 8 | buffer[3];
                val = buffer[3 + REQ_LEN] << 8 | buffer[4 + REQ_LEN];
            }
            ctr = 30;
            int prval = 1;
            float f = (float)val / 10.f;
            switch(lastpar){
                case REG_WSPEED:
                    DBG("wind speed");
                    meteoflags |= WSFLAG;
                    if(gotsegm && !(MeteoMode & INPUT_WND)){ // not entered by hands
                        if(f >= 0.f && f < 200.f){
                        val_Wnd = f;
                        MeteoMode &= ~NET_WND;
                        MeteoMode |= SENSOR_WND;
                        }
                    }
                break;
                case REG_WDIR:
                    DBG("wind direction");
                    //meteoflags |= WDFLAG;
                break;
                case REG_TAIR:
                    DBG("air temperature");
                    meteoflags |= TFLAG;
                    if(gotsegm && !(MeteoMode & INPUT_T1)){
                        if(f > -40.f && f < 40.f){
                        val_T1 = f;
                        MeteoMode &= ~NET_T1;
                        MeteoMode |= SENSOR_T1;
                        }
                    }
                break;
                case REG_HUM:
                    DBG("humidity");
                    meteoflags |= HFLAG;
                    if(gotsegm && !(MeteoMode & INPUT_HMD)){
                        if(f >= 0.f && f <= 100.f){
                        val_Hmd = f;
                        MeteoMode &= ~NET_HMD;
                        MeteoMode |= SENSOR_HMD;
                        }
                    }
                break;
                case REG_DEW:
                    DBG("dewpoint");
                break;
                case REG_PRES:
                    DBG("pressure");
                    f *= 760.f/1013.f; // convert hPa->mmHg
                    meteoflags |= PFLAG;
                    if(gotsegm && !(MeteoMode & INPUT_B)){
                        if(f > 500.f && f < 700.f){
                        val_B = f;
                        MeteoMode &= ~NET_B;
                        MeteoMode |= SENSOR_B;
                        }
                    }
                break;
                default:
                    prval = 0;
            }
            lastpar = 0;
            if(prval){
                DBG("=%.1f\n", f);
                return ANS_OK;
            }
            size = 0;
        }
        /*
        if(meteoflags == ALLFLAGS){
            DBG("Got all data");
            return ANS_OK;
        }
        if(time(NULL) - tstart >= METEO_TIMEOUT){
            DBG("Timeout, not all data meteoflags=0x%02X instead of 0x%02X", meteoflags, ALLFLAGS);
            return ANS_NODATA;
        }*/
    }
    return ANS_NODATA;
}


