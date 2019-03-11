#ifndef LIB
#define LIB

#define div 64
#define TIMEOUT 5000
#define maxl 250


typedef unsigned char byte;

typedef struct {
    int len;
    char payload[1400];
} msg;

typedef struct {

    byte MAXL;
    byte TIME;
    byte NPAD;
    byte PADc;
    byte EOL;
    byte QCTL;
    byte QBIN;
    byte CHRK;
    byte REPT;
    byte CAPA;
    byte R;

} s_package;

typedef struct {
    byte SOH;
    byte LEN;
    byte SEQ;
    byte TYPE;
    char *data;             //^_^
    unsigned short CHECK;
    byte MARK;
} package;

s_package constructSPackage() {
    s_package s;
    s.MAXL = 250;
    s.TIME = 5;
    s.NPAD = 0x00;
    s.PADc = 0X00;
    s.EOL = 0x0D;
    s.QCTL = 0x00;
    s.QBIN = 0x00;
    s.CHRK = 0x00;
    s.REPT = 0x00;
    s.CAPA = 0x00;
    s.R = 0x00;
    return s;
}


void init(char *remote, int remote_port);

void set_local_port(int port);

void set_remote(char *ip, int port);

int send_message(const msg *m);

int recv_message(msg *r);

msg *receive_message_timeout(int time); //timeout in milliseconds
unsigned short crc16_ccitt(const void *buf, int len);

msg constructMessage(package *p) {
    msg t;
    t.len = p->LEN + 2;
    memcpy(t.payload, &(p->SOH), 1);
    memcpy(t.payload + 1, &(p->LEN), 1);
    memcpy(t.payload + 2, &(p->SEQ), 1);
    memcpy(t.payload + 3, &(p->TYPE), 1);
    memcpy(t.payload + 4, p->data, p->LEN - 5);
    unsigned short crc = crc16_ccitt(t.payload, t.len - 3);
    p->CHECK = crc;
    memcpy(t.payload + 4 + p->LEN - 5, &(p->CHECK), 2);
    memcpy(t.payload + 4 + p->LEN - 5 + 2, &(p->MARK), 1);
    return t;
}

/*
 * General Package Construction
 */
package constructPackage(void *data, byte length, byte type, byte seq) {
    package p;
    p.SOH = 0x01;
    p.SEQ = seq;
    p.LEN = 5 + length;
    p.TYPE = type;
    if (length == 0) {
        p.data = NULL;
    } else {
        p.data = (char *) malloc(length);
        memcpy(p.data, data, length);
    }
    p.MARK = 0x0D;

    return p;
}

//function used to update ack and nak messages
void update_message(msg *m, int new_seq, int data_size, byte type) {
    memcpy(m->payload + 2, &new_seq, 1);
    memcpy(m->payload + 3, &type, 1);
    unsigned short crc = crc16_ccitt(m->payload, m->len - 3);
    memcpy(m->payload + 4 + data_size, &crc, 2);
}

#endif

