#include "../sock.h"
#include "../iscsi.h"
#include "../dcirlist.h"
#include "../iscsi_mem.h"
#include "../tf.h"
#include "../iscsi_param.h"
#include "../conf_reader.h"
#include "../param_helper.h"
#include "../iscsi_session.h"

#include <stdio.h>
#include <stdlib.h>

#include "../debug/iscsi_debug.h"
#include "tf_sg.h"
#include <stropts.h>
#include <fcntl.h>
#include <scsi/sg.h>

void *sglinux_tfInit()
{
	int fd = open("/proc/scsi/sg/devices", O_RDONLY);
	if (fd < 0) {
		DEBUG("sglinux_tfInit: Can't open /proc/scsi/sg/devices\n");
		return NULL;
	}
	close(fd);
	return (void *)1;
}

void sglinux_tfCleanup(void *handle)
{
}

void *sglinux_tfAttach(void *handle, struct Session *ses)
{
	sglinux_data_t sd;
	int ver = 0;

	struct confElement *targetInfo = (struct confElement *)ses->params[ISP_TARGETNAME].pvalue;
	struct confElement *devName = findElement(targetInfo->childs, SG_DEVICE_NAME);
	
	if (!devName) {
		return NULL;
	}
	
	sd.fd = open(devName->value, O_RDONLY); /* O_RDWR  in common application*/
	if (sd.fd < 0) {
		DEBUG1("sglinux_tfAttach: Can't open %s\n", devName->value);
		return NULL;
	}
	
	if ((ioctl(sd.fd, SG_GET_VERSION_NUM, &ver) < 0) || (ver < 30000)) {
		DEBUG1("sglinux_tfAttach: Old version of sg driver %d. Need kernel 2.4.19 or later.\n", ver);
		close(sd.fd);
		return NULL;
	}
	
	return (void *)sd.fd;
}

void sglinux_tfDetach(void *handle, void *prot)
{
	close(((int)prot));
}
static int getCommandLength(char *cmd)
{
	switch ((cmd[0] & 0xE0) >> 5) {	
	case 0:
		return 6;
	case 1:
	case 2:
		return 10;
	case 4:
		return 12;
	case 5:
		return 16;
	default:
		DEBUG1("getCommandLength: UNKNOWN size for command %x\n", cmd[0]);
		return 16;
	}
}

int sglinux_tfSCSICommand(struct tfCommand *cmd)
{
	sg_io_hdr_t io_hd;
	int res;
	int fd = (int)cmd->targetHandle;
	memset(&io_hd, 0, sizeof(sg_io_hdr_t));

	io_hd.interface_id = 'S';
	io_hd.cmd_len = getCommandLength(cmd->cdbBytes);
	io_hd.mx_sb_len = cmd->senseLen;
	io_hd.sbp = cmd->sense;
	
	io_hd.cmdp = cmd->cdbBytes;
	io_hd.timeout = 20000;

	/* Cyrrently supported only one LUN */
	if (cmd->lun > 0) {
		cmd->response = 1;
		cmd->readedCount = 0;
		cmd->writedCount = 0;
		cmd->senseLen = 0;
		goto sg_exit;
	}
	
	if (cmd->writeCount > 0) {
		io_hd.dxfer_direction = SG_DXFER_TO_DEV;
		io_hd.dxferp = cmd->imWriteBuffer->data;
		io_hd.dxfer_len = cmd->writeCount;
	} else if (cmd->residualReadCount > 0) {
		io_hd.dxfer_direction = SG_DXFER_FROM_DEV;
		io_hd.dxferp = cmd->readBuffer->data;
		io_hd.dxfer_len = cmd->residualReadCount;
	} else {
		io_hd.dxfer_direction = SG_DXFER_NONE;
		io_hd.dxferp = NULL;
		io_hd.dxfer_len = 0;
	}

	/* Workaround for SPC-2 */
	if (cmd->cdbBytes[0] == 0xa0) {
		cmd->response = 0;
		cmd->status = 0;
		cmd->readedCount = cmd->residualReadCount;
		cmd->writedCount = 0;
		cmd->senseLen = 0;
		memset(cmd->readBuffer->data, 0, cmd->residualReadCount);
		cmd->readBuffer->data[3] = 8;
		cmd->readBuffer->data[9] = 0x00;
		return sesSCSICmdResponse(cmd);
	}

			
	res = ioctl(fd, SG_IO, &io_hd);

	if (res == 0) {
		cmd->senseLen = io_hd.sb_len_wr;
		cmd->status = io_hd.status;
		
		if ((io_hd.info & SG_INFO_OK_MASK) != SG_INFO_OK) {
			if (io_hd.sb_len_wr > 0) {
				cmd->response = 0;
			} else {
				cmd->response = 1;
			}
			cmd->writeCount = 0;
			cmd->readedCount = 0;
		} else {
			cmd->response = 0;
		
			if ((cmd->writeCount > 0)) {
				cmd->writedCount = cmd->writeCount;
				cmd->readedCount = 0;
			} else if (cmd->residualReadCount > 0) {
				cmd->readedCount = cmd->residualReadCount;
				cmd->writeCount = 0;
			} else {
				cmd->writeCount = 0;
				cmd->readedCount = 0;
			}	
		}
	
	} else {
		cmd->response = 1;
		cmd->readedCount = 0;
		cmd->writedCount = 0;
		cmd->senseLen = 0;
		DEBUG("sglinux_tfSCSICommand: ioctl failed!\n");
	}

sg_exit:
	return sesSCSICmdResponse(cmd);
}
