#include <stdio.h>
#include <unistd.h>
#include "net_addrs.h"
#include "utilities/utils_sockets.h"
#include "rollservo.h"

static int sd;

void rollservo_init(void){
	sd = udp_socket();
	if(sd < 0){
		return;
	}

	if(connect(sd, ROLL_ADDR, sizeof(struct sockaddr_in)) < 0){
		perror("rollservo_init: connect() failed");
		close(sd);
	}
}

void rollservo_final(void){
	close(sd);
}

void rs_receive_adj(RollServoMessage* adj){
	char data[3];
	data[0] = (0xff & adj->u16ServoPulseWidthBin14);
	data[1] = (0xff00 & adj->u16ServoPulseWidthBin14) >> 8;
	data[2] = adj->u8ServoDisableFlag;
	if(write(sd, data, sizeof(data)) != sizeof(data)){
		perror("rs_receive_adj: write failed");
	}
}

