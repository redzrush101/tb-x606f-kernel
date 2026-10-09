/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MTK_BATTERY_LEGACY_H
#define _MTK_BATTERY_LEGACY_H

/* Android 10 GM30 netlink ABI, before command hashes and split commands. */
struct fgd_legacy_msg {
	u32 cmd;
	u32 subcmd;
	u32 para1;
	u32 para2;
	u32 data_len;
	u32 ret_len;
	u32 identity;
	u8 data[];
};

#define FGD_LEGACY_HDR_LEN sizeof(struct fgd_legacy_msg)
#define FGD_LEGACY_COMMANDS 86
#define FGD_LEGACY_CUSTOM_LEN 664

bool mtk_battery_legacy_message(const void *data, unsigned int len);
int mtk_battery_legacy_dispatch(struct mtk_battery *gm,
	const struct fgd_legacy_msg *msg, struct fgd_nl_msg_t *request,
	struct fgd_nl_msg_t *reply);
void mtk_battery_daemon_handler(struct mtk_battery *gm, void *data,
	struct fgd_nl_msg_t *reply);

#endif
