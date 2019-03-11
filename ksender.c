#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10000

int open_file(char *filename) {

    int file_desc = open(filename, O_RDONLY);
    if (file_desc < 0) {
        puts("This file can't be open");
        exit(1);
    }
    return file_desc;
}


msg *verify_and_resent_message(msg t, char *instance, int *seq) {
    int resent_times = 3;       //no of times for resending messages
    msg *rcv_msg;               //received massage from receiver

    while (1) {
        //wait for ack/nak
        rcv_msg = receive_message_timeout(TIMEOUT);
        //timeout
        if (rcv_msg == NULL) {
            resent_times--;
            if (resent_times == 0) {
                break;
            }
            printf("[%s]: Resending last message : seqNo = [%d]\n", instance, *seq);
            send_message(&t);
        } else {
            //if it receive an old ack/nak (that have been already received)
            if ((rcv_msg->payload[2] + 1) % div == *seq) {
                printf("[%s] : Got reply old confirmation:  with seq [%d]\n", instance, rcv_msg->payload[2]);
                printf("[%s]: Resending last message : seqNo = [%d]\n", instance, *seq);
                send_message(&t);
                continue;
            }
            //type of received confirmation (Y/N)
            byte confirmation = *(byte *) (rcv_msg->payload + 3);
            //update sequence
            *seq = (((byte) (rcv_msg->payload[2])) + 1) % div;
            //nak
            if (confirmation == 'N') {
                printf("[%s] : Got reply : %s for seq [%d]\n", instance, "NAK", *seq - 1);
                //update message and resent
                memcpy(t.payload + 2, seq, 1);
                unsigned short crc = crc16_ccitt(t.payload, t.len - 3);
                memcpy(t.payload + t.len - 3, &crc, 2);
                printf("[%s] Sending message with seqNo = [%d]\n", instance, *seq);
                send_message(&t);
                resent_times = 3;
                continue;
            } else {
                printf("[%s] : Got reply : %s for seq [%d]\n", instance, "ACK", *seq - 1);
                break;
            }
        }
    }
    return rcv_msg;

}

/*
 * read content of input file and send data
 */
void read_file(int fd, int *seq, char *instance, int MAXL) {
    package p;
    msg t;
    msg *y;
    int ct;
    char *buf = calloc(maxl, sizeof(char));
    while ((ct = read(fd, buf, MAXL)) > 0) {
        p = constructPackage(buf, ct, 'D', *seq);
        t = constructMessage(&p);
        printf("[%s] Sending Data, seqNo = %d\n", instance, *seq);
        send_message(&t);
        y = verify_and_resent_message(t, instance, seq);
        if (y == NULL)    // no message received after resending 3 times
        {
            printf("[%s] TIMEOUT File-Data\n", instance);
            return;
        }
    }
    close(fd);
    free(buf);

    //EOF transmission
    p = constructPackage(NULL, 0, 'Z', *seq);
    t = constructMessage(&p);
    printf("[%s] Sending EOF, seqNo = %d\n", instance,
           (unsigned char) t.payload[2]);
    send_message(&t);
    y = verify_and_resent_message(t, instance, seq);

    if (y == NULL)    // no message received after resending 3 times
    {
        printf("[%s] TIMEOUT EOF\n", instance);
        return;
    }
    //end of EOF transmission
}

int main(int argc, char **argv) {

    if (argc <= 1) {
        printf("%s\n", "You need to give a filename argument");
        exit(1);
    }
    init(HOST, PORT);

    int seq = 0;
    s_package s;
    msg t;
    package p;

    //Send init package transmission
    s = constructSPackage();
    p = constructPackage(&s, sizeof(s), 'S', seq);
    t = constructMessage(&p);

    msg *y;
    printf("[%s] Sending Send-Init Message: seqNo = %d\n", argv[0], (byte) t.payload[2]);
    send_message(&t);
    y = verify_and_resent_message(t, argv[0], &seq);

    //no message after three times resending
    if (y == NULL) {
        printf("Send Init TIMEOUT");
        return 0;
    }
    //End of Send Init Transmission and verifications;

    for (int i = 1; i < argc; i++) {

        //send file header
        p = constructPackage(argv[i], strlen(argv[i]), 'F', seq);
        t = constructMessage(&p);

        printf("[%s] Sending File-Header: seqNo = %d\n", argv[0],
               (byte) t.payload[2]);
        send_message(&t);
        y = verify_and_resent_message(t, argv[0], &seq);

        if (y == NULL) {
            printf("[%s] TIMEOUT File-Header\n", argv[0]);
            return 0;
        }

        //end of file header transmission

        // file data transmission
        int fd = open_file(argv[i]);  //file to be sent
        read_file(fd, &seq, argv[0], 250);
        //end of file data transmission
    }
    //EOT transmission
    p = constructPackage(NULL, 0, 'T', seq);
    t = constructMessage(&p);
    printf("[%s] Sending EOT; seqNo = %d\n", argv[0], (byte) t.payload[2]);
    send_message(&t);
    y = verify_and_resent_message(t, argv[0], &seq);
    if (y == NULL) {
        printf("[%s] TIMEOUT EOT\n", argv[0]);
        return 0;
    }
    //End of EOT transmission

    return 0;
}