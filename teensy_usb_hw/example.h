#ifndef __EXAMPLE_H__
#define __EXAMPLE_H__

/* unpacked request struct; analogous to struct read_request in
 * ../usb_driver/teensy.h */
struct teensy_msg {
        uint8_t packet_id; /* packet_id in kernel land version */
        uint8_t destination;
        uint8_t size;
        uint8_t *buf;
};

/* a handler function looks like */
//void handler(teensy_req);
#endif
