// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/string.h>
#include "mtk_battery.h"
#include "mtk_battery_daemon.h"
#include "mtk_battery_legacy.h"

struct fgd_legacy_chunk {
	u32 type;
	u32 total_size;
	u32 size;
	u32 idx;
	u8 input[DATA_SIZE];
};

struct fgd_legacy_int {
	u32 type;
	s32 input;
	s32 output;
	s32 status;
};

bool mtk_battery_legacy_message(const void *data, unsigned int len)
{
	const struct fgd_legacy_msg *msg = data;

	return len >= FGD_LEGACY_HDR_LEN && msg->identity == FGD_NL_MAGIC;
}

static void legacy_custom_data(struct mtk_battery *gm, void *data)
{
	const struct fuel_gauge_custom_data *cust = &gm->fg_cust_data;
	u32 *out = data;
	size_t start = offsetof(struct fuel_gauge_custom_data, hardwareVersion);
	size_t end = offsetof(struct fuel_gauge_custom_data, min_uisoc_at_kpoc);

	/* Stock adds get/set limits and omits the later KPOC field. */
	BUILD_BUG_ON(sizeof(*cust) != 660);
	BUILD_BUG_ON(20 + 648 - 12 + 8 != FGD_LEGACY_CUSTOM_LEN);
	out[0] = FGD_LEGACY_COMMANDS;
	out[1] = FGD_LEGACY_CUSTOM_LEN;
	out[2] = 13;
	out[3] = 7;
	out[4] = 1015;
	memcpy(out + 5, (const u8 *)cust + start, end - start);
	out[164] = cust->daemon_log_level;
	out[165] = cust->record_log;
}

static void legacy_upload_region(void *dst, u32 wire_offset, u32 length,
	const struct fgd_legacy_chunk *in)
{
	u32 start = max(in->idx, wire_offset);
	u32 end = min(in->idx + in->size, wire_offset + length);

	if (end > start)
		memcpy((u8 *)dst + start - wire_offset,
		       in->input + start - in->idx, end - start);
}

static int legacy_chunk(struct mtk_battery *gm,
	const struct fgd_legacy_msg *msg, struct fgd_nl_msg_t *reply)
{
	const struct fgd_legacy_chunk *in = (const void *)msg->data;
	struct fgd_legacy_chunk *out = (void *)reply->fgd_data;
	struct fuel_gauge_custom_data *cust = &gm->fg_data.fg_cust_data;
	u32 length;

	BUILD_BUG_ON(sizeof(gm->fg_table_cust_data) != 14820);
	if (msg->data_len != sizeof(*in))
		return -EINVAL;
	if (in->type == 0 && msg->cmd == 3) {
		if (msg->ret_len < sizeof(*out))
			return -EINVAL;
		length = sizeof(gm->fg_table_cust_data);
	} else if (in->type == 1 && msg->cmd == 84) {
		length = FGD_LEGACY_CUSTOM_LEN + sizeof(gm->fg_table_cust_data);
	} else {
		return -EINVAL;
	}
	if (in->total_size != length || in->size > DATA_SIZE ||
	    in->idx > length || in->size > length - in->idx)
		return -EINVAL;
	if (msg->cmd == 84) {
		/* Upload the daemon's snapshot, not the live kernel calibration. */
		legacy_upload_region(cust, 0, 12, in);
		legacy_upload_region(&cust->hardwareVersion, 20, 636, in);
		legacy_upload_region(&cust->daemon_log_level, 656, 4, in);
		legacy_upload_region(&cust->record_log, 660, 4, in);
		legacy_upload_region(&gm->fg_data.fg_table_cust_data,
				     FGD_LEGACY_CUSTOM_LEN,
				     sizeof(gm->fg_table_cust_data), in);
		return 0;
	}
	out->type = in->type;
	out->total_size = length;
	out->size = in->size;
	out->idx = in->idx;
	memcpy(out->input, (u8 *)&gm->fg_table_cust_data + in->idx, in->size);
	reply->fgd_data_len = sizeof(*out);
	return 0;
}

static int legacy_int_command(struct mtk_battery *gm,
	const struct fgd_legacy_msg *msg, struct fgd_nl_msg_t *request,
	struct fgd_nl_msg_t *reply)
{
	const struct fgd_legacy_int *in = (const void *)msg->data;
	struct fgd_legacy_int *out = (void *)reply->fgd_data;
	int command;
	s32 value;

	if (msg->data_len != sizeof(*in) || msg->ret_len < sizeof(*out))
		return -EINVAL;
	if (in->type >= 1 && in->type <= 6)
		command = FG_DAEMON_CMD_GET_SHUTDOWN_CAR + in->type - 1;
	else if (in->type >= 1000 && in->type <= 1014)
		command = FG_DAEMON_CMD_SET_SOC + in->type - 1000;
	else
		return -EINVAL;
	request->fgd_cmd = command;
	request->fgd_data_len = sizeof(in->input);
	memcpy(request->fgd_data, &in->input, sizeof(in->input));
	mtk_battery_daemon_handler(gm, request, reply);
	/* Several donor integer commands omit their reply length. */
	memcpy(&value, reply->fgd_data, sizeof(value));
	out->type = in->type;
	out->input = in->input;
	out->output = value;
	out->status = 0;
	reply->fgd_data_len = sizeof(*out);
	return 0;
}

int mtk_battery_legacy_dispatch(struct mtk_battery *gm,
	const struct fgd_legacy_msg *msg, struct fgd_nl_msg_t *request,
	struct fgd_nl_msg_t *reply)
{
	int ret;

	BUILD_BUG_ON(FGD_LEGACY_HDR_LEN != 28);
	if (msg->cmd >= FGD_LEGACY_COMMANDS)
		return -EINVAL;
	request->fgd_cmd = msg->cmd > 3 ? msg->cmd + 1 : msg->cmd;
	request->fgd_subcmd = msg->subcmd;
	request->fgd_subcmd_para1 = msg->para1;
	if (!request->fgd_subcmd && msg->para1 == FGD_LEGACY_HDR_LEN)
		request->fgd_subcmd_para1 = FGD_NL_HDR_LEN;
	request->fgd_data_len = msg->data_len;
	request->fgd_ret_data_len = msg->ret_len;
	memcpy(request->fgd_data, msg->data, msg->data_len);

	switch (msg->cmd) {
	case 2:
		if (msg->ret_len < FGD_LEGACY_CUSTOM_LEN)
			return -EINVAL;
		legacy_custom_data(gm, reply->fgd_data);
		reply->fgd_data_len = FGD_LEGACY_CUSTOM_LEN;
		break;
	case 3:
	case 84:
		ret = legacy_chunk(gm, msg, reply);
		if (ret)
			return ret;
		break;
	case 85:
		ret = legacy_int_command(gm, msg, request, reply);
		if (ret)
			return ret;
		break;
	default:
		mtk_battery_daemon_handler(gm, request, reply);
		break;
	}
	/* Preserve the wire command for the userspace response dispatcher. */
	reply->fgd_cmd = msg->cmd;
	return 0;
}
