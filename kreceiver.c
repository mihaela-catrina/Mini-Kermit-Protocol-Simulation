#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10001

//conf -> old ack/nak for resend
msg *verify_and_send_confirmation(msg *conf, char *instance, byte *seq, int send_init, byte *eot) {
    msg *rcv_msg;                                   //received essage from sender
    int ct = 3;                                     //no of packages' resents
    int first_send_init = 1;                        //we don't have an old package received (an old confirmation)
    //if we don't have a send init package
    if (send_init == 0) first_send_init = 0;
    package p;

    while (ct > 0) {
        rcv_msg = receive_message_timeout(TIMEOUT);
        //if we didn't receive anything in time sec
        if (rcv_msg == NULL) {
            ct--;
            if (ct == 0) break;
            //already exist an ack or nak
            if (first_send_init == 0) {
                printf("[%s] Resending last confirmation -> seqNo = (%d)\n", instance, conf->payload[2]);
                send_message(conf);
            }
        } else {
            //computed crc for received package
            unsigned short crc = crc16_ccitt(rcv_msg->payload, rcv_msg->len - 3);
            //crc from received package
            unsigned short check;
            memcpy(&check, &rcv_msg->payload[rcv_msg->len - 3], 2);
            //the message is not corrupted
            if (crc == check) {
                //verify not to receive old packages left on the link
                if (first_send_init == 0 && rcv_msg->payload[2] != (*seq + 1) % div) {
                    continue;
                }
                //EOT/EOF package
                if (rcv_msg->payload[3] == 'Z' || rcv_msg->payload[3] == 'T') {
                    *eot = 1;
                }
                *seq = ((byte) rcv_msg->payload[2]) % 64;

                //first send-init => there's not an old ack or nak
                if (first_send_init == 1) {
                    p = constructPackage(NULL, 0, 'Y', *seq);
                    *conf = constructMessage(&p);
                } else {
                    //update old confirmation
                    update_message(conf, *seq, 0, 'Y');
                }
                printf("[%s] Sending ACK -> seqNo = (%d)\n", instance,
                       conf->payload[2]);
                send_message(conf);
                break;

            } else {
                //verify not to receive old packages left on the link ^_^
                if (first_send_init == 0 && rcv_msg->payload[2] == *seq) {
                    continue;
                }
                *seq = ((byte) (rcv_msg->payload[2])) % div;
                if (first_send_init == 1) {
                    p = constructPackage(NULL, 0, 'N', *seq);
                    *conf = constructMessage(&p);
                } else {
                    //first send-init => there's not an old ack or nak
                    update_message(conf, *seq, 0, 'N');
                }
                printf("[%s] Sending NAK ->  seqNo = (%d)\n", instance, conf->payload[2]);
                send_message(conf);
                ct = 3;
            }
            //we already have an ack or nak for a package
            //variable used for send init cases
            first_send_init = 0;
        }
    }
    return rcv_msg;

}

/*
 * Function used to write received data in the output file
 */
void write_to_file(char *filename, char *argv0, msg *conf, byte *seq, byte *eot) {

    msg *y;
    int file_desc = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (file_desc < 0)
        perror("File creation error");

    while (1) {
        y = verify_and_send_confirmation(conf, argv0, seq, 0, eot);
        if (y == NULL) {
            printf("[%s] TIMEOUT REC File-Data\n", argv0);
            return;
        }

        //end of data transmission
        if (*eot == 1) {
            break;
        }

        char *buffer = calloc(maxl, (sizeof(char)));
        memcpy(buffer, (y->payload + 4), y->len - 7);
        if (write(file_desc, buffer, y->len - 7) < 0) {
            printf("Writing file error!\n");
        }

        free(buffer);
    }
    close(file_desc);
}

int main(int argc, char **argv) {
    init(HOST, PORT);
    byte seq = 0;
    msg conf;                   //confirmation message (ACK/NAK)
    byte EOT = 0;               //En d of transmission variable

    //send init receiving
    msg *y = verify_and_send_confirmation(&conf, argv[0], &seq, 1, NULL);
    if (y == NULL) {
        printf("[%s] TIMEOUT REC Send-Init\n", argv[0]);
        return 0;
    }
    //end of send init receiving

    while (1) {
        //file name receiving
        y = verify_and_send_confirmation(&conf, argv[0], &seq, 0, &EOT);
        char *new_filename;
        if (y == NULL) {
            printf("[%s] TIMEOUT REC File-Header\n", argv[0]);
            return 0;
        }
        //end of file name receiving

        if (EOT == 1) {
            break;
        }

        //Construct new file name
        char *filename = malloc(y->len - 7 + 1);
        memcpy(filename, y->payload + 4, y->len - 7);
        filename[y->len - 6] = '\0';
        printf("[%s] Current File : %s\n", argv[0], filename);
        new_filename = malloc((strlen(filename) + 5) * (sizeof(char)));
        strcpy(new_filename, "recv_");
        strcat(new_filename, filename);

        byte eof = 0;
        //create output file and write data
        write_to_file(new_filename, argv[0], &conf, &seq, &eof);
        //end of data receiving
    }
    return 0;
}
