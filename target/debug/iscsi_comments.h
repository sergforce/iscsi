#ifndef _iSCSI_COMMENTS_
#define _iSCSI_COMMENTS_

const char *opcode_comment[] = {
/*0x00*/ "NOP-Out",
/*0x01*/ "SCSI Command", /*(encapsulates a SCSI Command Descriptor Block)*/
/*0x02*/ "SCSI Task Management function request",
/*0x03*/ "Login Request",
/*0x04*/ "Text Request",
/*0x05*/ "SCSI Data-Out",  /*(for WRITE operations)*/
/*0x06*/ "Logout Request",
/*0x07*/ "reserved 0x07",
/*0x08*/ "reserved 0x08",
/*0x09*/ "reserved 0x09",
/*0x0a*/ "reserved 0x0a",
/*0x0b*/ "reserved 0x0b",
/*0x0c*/ "reserved 0x0c",
/*0x0d*/ "reserved 0x0d",
/*0x0e*/ "reserved 0x0e",
/*0x0f*/ "reserved 0x0f",

/*0x10*/ "SNACK Request",
/*0x11*/ "reserved 0x11",
/*0x12*/ "reserved 0x12",
/*0x13*/ "reserved 0x13",
/*0x14*/ "reserved 0x14",
/*0x15*/ "reserved 0x15",
/*0x16*/ "reserved 0x16",
/*0x17*/ "reserved 0x17",
/*0x18*/ "reserved 0x18",
/*0x19*/ "reserved 0x19",
/*0x1a*/ "reserved 0x1a",
/*0x1b*/ "reserved 0x1b",
/*0x1c*/ "Vendor specific code 0x1c",
/*0x1d*/ "Vendor specific code 0x1d",
/*0x1e*/ "Vendor specific code 0x1e",
/*0x1f*/ "reserved 0x1f",

/*0x20*/ "NOP-In",
/*0x21*/ "SCSI Response", /*contains SCSI status and possibly sense
      information or other response information.*/
/*0x22*/ "SCSI Task Management function response",
/*0x23*/ "Login Response",
/*0x24*/ "Text Response",
/*0x25*/ "SCSI Data-In", /*for READ operations.*/
/*0x26*/ "Logout Response",
/*0x27*/ "reserved 0x27",
/*0x28*/ "reserved 0x28",
/*0x29*/ "reserved 0x29",
/*0x2a*/ "reserved 0x2a",
/*0x2b*/ "reserved 0x2b",
/*0x2c*/ "reserved 0x2c",
/*0x2d*/ "reserved 0x2d",
/*0x2e*/ "reserved 0x2e",
/*0x2f*/ "reserved 0x2f",

/*0x30*/ "reserved 0x30",
/*0x31*/ "Ready To Transfer", /*(R2T) - sent by target when it is ready
      to receive data.*/
/*0x32*/ "Asynchronous Message", /*sent by target to indicate certain
      special conditions.*/
/*0x32*/ "reserved 0x32",
/*0x33*/ "reserved 0x33",
/*0x34*/ "reserved 0x34",
/*0x35*/ "reserved 0x35",
/*0x36*/ "reserved 0x36",
/*0x37*/ "reserved 0x37",
/*0x38*/ "reserved 0x38",
/*0x39*/ "reserved 0x39",
/*0x3a*/ "reserved 0x3a",
/*0x3b*/ "reserved 0x3b",
/*0x3c*/ "Vendor specific code 0x3c",
/*0x3d*/ "Vendor specific code 0x3d",
/*0x3e*/ "Vendor specific code 0x3e",
/*0x3f*/ "Reject"
};


#endif
