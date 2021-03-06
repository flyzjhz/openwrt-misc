// Copyright 2011 Juri Glass, Mathias Runge, Nadim El Sayed
// DAI-Labor, TU-Berlin
//
// This file is part of libSML.
//
// libSML is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// libSML is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with libSML.  If not, see <http://www.gnu.org/licenses/>.

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <math.h>
#include <pthread.h>
#include "snmp.h"

#include <sml/sml_file.h>
#include <sml/sml_transport.h>

#define SML_BUFFER_LEN 8096
#define SNMP_PORT 161
#define MAXLINE 128

pthread_mutex_t value_mutex = PTHREAD_MUTEX_INITIALIZER;

extern void *snmp_agent(void *);
const char obis_tarif0[] = {0x01,0x00,0x01,0x08,0x00};
const char obis_tarif1[] = {0x01,0x00,0x01,0x08,0x01};
const char obis_tarif2[] = {0x01,0x00,0x01,0x08,0x02};

unsigned int counter_tarif0;
unsigned int counter_tarif1;
unsigned int counter_tarif2;

void print_hex(unsigned char c)
{
   unsigned char hex=c;
   hex = (hex >> 4) & 0x0f;
   if (hex > 9) {
       hex = hex + 'A' - 10;
   } else {
       hex = hex + '0';
   }
   putchar(hex);
   hex = c & 0x0f;
   if (hex > 9) {
       hex = hex + 'A' - 10;
   } else {
       hex = hex + '0';
   }
   putchar(hex);
   putchar(0x20);
}

void print_octet_str(octet_string *object) {
   int i;
   for (i=0;i<object->len;i++) {
     print_hex(object->str[i]);
   }
}

void print_char_str(char *object, int len) {
   int i;
   for (i=0;i<len;i++) {
     print_hex(object[i]);
   }
}
  
int serial_port_open(const char* device) {
	int bits;
	struct termios config;
	memset(&config, 0, sizeof(config));

	int fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd < 0) {
		printf("error: open(%s): %s\n", device, strerror(errno));
		return -1;
	}

	// set RTS
	ioctl(fd, TIOCMGET, &bits);
	bits |= TIOCM_RTS;
	ioctl(fd, TIOCMSET, &bits);

	tcgetattr( fd, &config ) ;

	// set 8-N-1
	config.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	config.c_oflag &= ~OPOST;
	config.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	config.c_cflag &= ~(CSIZE | PARENB | PARODD | CSTOPB);
	config.c_cflag |= CS8;

	// set speed to 9600 baud
	cfsetispeed( &config, B9600);
	cfsetospeed( &config, B9600);

	tcsetattr(fd, TCSANOW, &config);
	return fd;
}

void transport_receiver(unsigned char *buffer, size_t buffer_len) {
	short i;
	//unsigned char message_buffer[SML_BUFFER_LEN];
	sml_get_list_response *body;
        sml_list *entry;
        size_t m = 0, n= 10;
        struct timeval time;

	// the buffer contains the whole message, with transport escape sequences.
	// these escape sequences are stripped here.
	sml_file *file = sml_file_parse(buffer + 8, buffer_len - 16);
	// the sml file is parsed now

	// read here some values ..
	for (i = 0; i < file->messages_len; i++) {
		sml_message *message = file->messages[i];

		if (*message->message_body->tag == SML_MESSAGE_GET_LIST_RESPONSE) {
			body = (sml_get_list_response *) message->message_body->data;
			// printf("new message from: %*s\n", body->server_id->len, body->server_id->str);
                        entry = body->val_list;
                	printf ("SML Message\n");

			int time_mode = 1;
			if (body->act_sensor_time) {
				time.tv_sec = *body->act_sensor_time->data.timestamp;
				time.tv_usec = 0;
                                time_mode = 1;
                                printf("sensor time: %lu.%lu, %i\n", time.tv_sec, time.tv_usec, *body->act_sensor_time->tag);
                	}
                        if (body->act_gateway_time) {
                                time.tv_sec = *body->act_sensor_time->data.timestamp;
                                time.tv_usec = 0;
                                time_mode = -1;
                                printf("sensor time: %lu.%lu, %i\n", time.tv_sec, time.tv_usec, *body->act_sensor_time->tag);
                        }
                        for (entry = body->val_list; entry != NULL; entry = entry->next) { /* linked list */
				//int unit = (entry->unit) ? *entry->unit : 0;
                                int scaler = (entry->scaler) ? *entry->scaler : 1;
				// printf("  scale: %d  unit: %d\n",unit,scaler);
				double value;

                                switch (entry->value->type) {
                                	case 0x51: value = *entry->value->data.int8; break;
                                        case 0x52: value = *entry->value->data.int16; break;
                                        case 0x54: value = *entry->value->data.int32; break;
                                        case 0x58: value = *entry->value->data.int64; break;
                                        case 0x61: value = *entry->value->data.uint8; break;
                                        case 0x62: value = *entry->value->data.uint16; break;
                                        case 0x64: value = *entry->value->data.uint32; break;
                                        case 0x68: value = *entry->value->data.uint64; break;

                                        default:
                                        //	fprintf(stderr, "Unknown value type: %x\n", entry->value->type);
                                                value = 0;
                                        }

                                /* apply scaler */
                                value *= pow(10, scaler);
				// printf("Value: %f\n",value);

                                /* get time */
                                // if (entry->val_time) { // TODO handle SML_TIME_SEC_INDEX
                                //	time.tv_sec = *entry->val_time->data.timestamp;
                                //        time.tv_usec = 0;
                                //        time_mode = 2;
                                // } else if (time_mode == 0) {
                                //        gettimeofday(&time, NULL);
                                //        time_mode = 3;
                                //}
                                gettimeofday(&time, NULL);

				pthread_mutex_lock( &value_mutex );
				if (!memcmp(entry->obj_name->str,obis_tarif0,sizeof(obis_tarif0))) {counter_tarif0=(int)(value+0.5);printf("CT0:%d\n",counter_tarif0); }
				if (!memcmp(entry->obj_name->str,obis_tarif1,sizeof(obis_tarif1))) {counter_tarif1=(int)(value+0.5);printf("CT1:%d\n",counter_tarif1); }
				if (!memcmp(entry->obj_name->str,obis_tarif2,sizeof(obis_tarif2))) {counter_tarif2=(int)(value+0.5);printf("CT2:%d\n",counter_tarif2); }
				pthread_mutex_unlock( &value_mutex );

                                // printf("%lu.%lu (%i)\t%.2f %s\n", time.tv_sec, time.tv_usec, time_mode, value, dlms_get_unit(unit));
				print_octet_str(entry->obj_name);
                                printf("%lu.%lu (%i)\t%.2f\n", time.tv_sec, time.tv_usec, time_mode, value);

			}
                                


                }
		/* iterating through linked list */
                for (m = 0; m < n && entry != NULL; m++) {
//                	meter_sml_parse(entry, &rds[m]);
                	printf (" -> entry %d\n",m);
                	entry = entry->next;
		}

        }


	// this prints some information about the file
	// sml_file_print(file);

	// free the malloc'd memory
	sml_file_free(file);
}

void *reader_thread (void *fd) {
	sml_transport_listen((int)fd,&transport_receiver);
	return 0;
}


int main(int argc, char **argv) {
	// this example assumes that a EDL21 meter sending SML messages via a
	// serial device. Adjust as needed.
	char *device = "/dev/ttyUSB0";
	int snmp_port=SNMP_PORT;
	//thread_data *thread_reader_data;
        pthread_t thread_reader;
        pthread_t thread_snmp;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	int fd = serial_port_open(device);
	if (fd > 0) {
		// listen on the serial device, this call is blocking.
		pthread_create(&thread_reader,NULL,reader_thread,(void*)fd);
		pthread_create(&thread_snmp,NULL,snmp_agent,(void *)snmp_port);
		//sml_transport_listen(fd, &transport_receiver);
		pthread_join(thread_reader,NULL);
		pthread_join(thread_snmp,NULL);
	}
	close(fd);
	return 0;
}

