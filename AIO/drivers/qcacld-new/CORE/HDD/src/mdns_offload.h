// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2023 Amlogic, Inc. All rights reserved.
 */

#ifndef __MDNS_OFFLOAD_H__
#define __MDNS_OFFLOAD_H__

#include <linux/version.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <net/cfg80211.h>
#include <net/netlink.h>

typedef uint32_t u32_boolean;

#define GOOGLE_VENDOR_OUI 0x1A11

typedef enum {
    WIFI_MDNS_OFFLOAD_SET_STATE = 0x1664,
    WIFI_MDNS_OFFLOAD_RESET_ALL,
    WIFI_MDNS_OFFLOAD_ADD_PROTOCOL_RESPONSES,
    WIFI_MDNS_OFFLOAD_REMOVE_PROTOCOL_RESPONSES,
    WIFI_MDNS_OFFLOAD_GET_AND_RESET_HIT_COUNTER,
    WIFI_MDNS_OFFLOAD_GET_AND_RESET_MISS_COUNTER,
    WIFI_MDNS_OFFLOAD_ADD_TO_PASSTHROUGH_LIST,
    WIFI_MDNS_OFFLOAD_REMOVE_FROM_PASSTHROUGH_LIST,
    WIFI_MDNS_OFFLOAD_SET_PASSTHROUGH_BEHAVIOR,
} wifi_mdns_offload_subcmd_t;

typedef enum {
    WIFI_MDNS_OFFLOAD_ATTRIBUTE_NONE,
    WIFI_MDNS_OFFLOAD_ATTRIBUTE_STATE,
    WIFI_MDNS_OFFLOAD_ATTRIBUTE_NETWORK_INTERFACE,
    WIFI_MDNS_OFFLOAD_ATTRIBUTE_OFFLOAD_PKT_LEN,
    WIFI_MDNS_OFFLOAD_ATTRIBUTE_OFFLOAD_PKT_DATA,
    WIFI_MDNS_OFFLOAD_ATTRIBUTE_MATCH_CRITERIA_NUM,
    WIFI_MDNS_OFFLOAD_ATTRIBUTE_MATCH_CRITERIA_DATA,
    WIFI_MDNS_OFFLOAD_ATTRIBUTE_RECORD_KEY,
    WIFI_MDNS_OFFLOAD_ATTRIBUTE_QNAME,
    WIFI_MDNS_OFFLOAD_ATTRIBUTE_PASSTHROUGH_BEHAVIOR,
    WIFI_MDNS_OFFLOAD_ATTRIBUTE_MAX,
} wifi_mdns_offload_attr_t;

typedef struct {
    /* QTYPE RRTYPE */
    int type;
    /* RRNAME offset in the rawOffloadPacket */
    int nameOffset;
} matchCriteria;

typedef struct {
    unsigned char *rawOffloadPacket;
    uint32_t rawOffloadPacketLen;
    matchCriteria *matchCriteriaList;
    uint32_t matchCriteriaListNum;
} mdnsProtocolData;

typedef enum {
    /* All the queries are forwarded to the system without any modification */
    FORWARD_ALL,
    /* All the queries are dropped.*/
    DROP_ALL,
    /* Only the queries present in the passthrough list are forwarded
     * to the system without any modification.
    */
    PASSTHROUGH_LIST,
} passthroughBehavior;

struct MDNS_OFFLOAD_OPS {
    u32_boolean (*setOffloadState)(u32_boolean enabled);
    void (*resetAll)();
    int (*addProtocolResponses)(char *networkInterface,
        mdnsProtocolData *offloadData);
    void (*removeProtocolResponses)(int recordKey);
    int (*getAndResetHitCounter)(int recordKey);
    int (*getAndResetMissCounter)();
    u32_boolean (*addToPassthroughList)(char *networkInterface, char *qname);
    void (*removeFromPassthroughList)(char *networkInterface, char *qname);
    void (*setPassthroughBehavior)(char *networkInterface,
        passthroughBehavior behavior);
};

extern const struct MDNS_OFFLOAD_OPS mdns_offload_ops;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0))
extern const struct nla_policy mdns_offload_attr_policy[];
#define DEFINE_MDNS_OFFLOAD_ATTR_POLICY \
    const struct nla_policy  \
        mdns_offload_attr_policy[WIFI_MDNS_OFFLOAD_ATTRIBUTE_MAX] = {\
        [WIFI_MDNS_OFFLOAD_ATTRIBUTE_STATE] = {\
            .type = NLA_U32, .len = sizeof(uint32) },\
        [WIFI_MDNS_OFFLOAD_ATTRIBUTE_NETWORK_INTERFACE] = {\
            .type = NLA_NUL_STRING },\
        [WIFI_MDNS_OFFLOAD_ATTRIBUTE_OFFLOAD_PKT_LEN] = {\
            .type = NLA_U32, .len = sizeof(uint32) },\
        [WIFI_MDNS_OFFLOAD_ATTRIBUTE_OFFLOAD_PKT_DATA] = {\
            .type = NLA_BINARY },\
        [WIFI_MDNS_OFFLOAD_ATTRIBUTE_MATCH_CRITERIA_NUM] = {\
            .type = NLA_U32, .len = sizeof(uint32) },\
        [WIFI_MDNS_OFFLOAD_ATTRIBUTE_MATCH_CRITERIA_DATA] = {\
            .type = NLA_BINARY },\
        [WIFI_MDNS_OFFLOAD_ATTRIBUTE_RECORD_KEY] = {\
            .type = NLA_U32, .len = sizeof(uint32) },\
        [WIFI_MDNS_OFFLOAD_ATTRIBUTE_QNAME] = {\
            .type = NLA_NUL_STRING },\
        [WIFI_MDNS_OFFLOAD_ATTRIBUTE_PASSTHROUGH_BEHAVIOR] = {\
            .type = NLA_U32, .len = sizeof(uint32) },\
    }
#define MDNS_OFFLOAD_ATTR_POLICY \
    .policy = mdns_offload_attr_policy,\
    .maxattr = WIFI_MDNS_OFFLOAD_ATTRIBUTE_MAX
#else
#define DEFINE_MDNS_OFFLOAD_ATTR_POLICY
#define MDNS_OFFLOAD_ATTR_POLICY
#endif /* LINUX_VERSION >= 5.3 */

#define _MDNS_OFFLOAD_VENDOR_CMD(cmd, func) \
{\
    {\
        .vendor_id = GOOGLE_VENDOR_OUI,\
        .subcmd    = cmd,\
    },\
    .flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,\
    .doit = func,\
    MDNS_OFFLOAD_ATTR_POLICY\
}

#define VENDOR_CMD_FUNC(func) __mdnsOffload_##func

#define VENDOR_CMD_FUNC_IMPL(func) \
static inline int __mdnsOffload_##func(struct wiphy *wiphy,\
    struct wireless_dev *wdev, const void *data, int len)

#define MDNS_OFFLOAD_VENDOR_IMPL \
    DEFINE_MDNS_OFFLOAD_ATTR_POLICY;\
    const struct MDNS_OFFLOAD_OPS mdns_offload_ops

static inline char *__mdnsOffload_decode_qname(unsigned char *buf,
    uint32_t buf_len, uint32_t offset)
{
    char *qname = NULL;
    unsigned char *p = NULL, *c = NULL;
    uint32_t n = 0, i = 0;

    if (!buf || buf_len < 1 || offset < 1 || offset > buf_len)
        goto err;
    p = buf + offset - 1;
    if (*p == 0)
        goto err;
    qname = (char *)kmalloc(256, GFP_KERNEL);
    if (!qname) {
        printk("mdnsOffload: alloc failed!\n");
        return NULL;
    }
    memset(qname, 0, 256);
    c = (unsigned char *)qname;
    while (*p) {
        if ((*p >> 6) == 0x03) {
            n = (((*p << 8) | *(p + 1)) & 0x3fff) - 1;
            if (n > (buf_len - 1))
                goto err;
            p = buf + n;
            continue;
        }
        n = *p;
        if (p + 1 + n > buf + buf_len - 1)
            goto err;
        p++;
        for (i = 0; i < n; i++) {
            if (*p > 32 && *p < 127)
                *c++ = *p++;
            else
                goto err;
        }
        if (*p != 0)
            *c++ = '.';
    }
    return qname;
err:
    printk("mdnsOffload: decode qname failed!\n");
    kfree(qname);
    return NULL;
}

static inline void __mdnsOffload_dump_msg(unsigned char *buf,
    uint32_t len)
{
    int line = 16, i = 0, j = 0;
    uint32_t n = 0;
    unsigned char *dump = NULL;

    dump = (unsigned char *)kmalloc(256, GFP_KERNEL);
    if (!dump) {
        printk("mdnsOffload: alloc failed!\n");
        return;
    }
    for (i = 0; i < len; i++) {
        memset(dump, 0, 256);
        n = 0;
        n += sprintf(dump + n, "%04x|", i);
        for (j = i; j < i + line; j++) {
            if (j < len)
                n += sprintf(dump + n, "%02x", buf[j]);
            else
                n += sprintf(dump + n, "  ");
            if (j == i + line - 1)
                n += sprintf(dump + n, "|");
            else
                n += sprintf(dump + n, " ");
        }
        for (j = i; j < i + line && j < len; j++) {
            if (buf[j] > 32 && buf[j] < 127)
                n += sprintf(dump + n, "%c", buf[j]);
            else
                n += sprintf(dump + n, ".");
        }
        printk("%s\n", dump);
        i = i + line - 1;
    }
    kfree(dump);
    dump = NULL;
}

static inline int __mdnsOffload_send_vendor_cmd_reply(struct wiphy *wiphy,
    uint32_t cmd, const void  *data, int len)
{
    struct sk_buff *skb;
    uint32_t vendor_id = GOOGLE_VENDOR_OUI;
    int err;

    skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, len);
    if (unlikely(!skb)) {
        printk("skb alloc failed!\n");
        err = -ENOMEM;
        goto exit;
    }
    nla_put_u32(skb, NL80211_ATTR_VENDOR_ID, vendor_id);
    nla_put_u32(skb, NL80211_ATTR_VENDOR_SUBCMD, cmd);
    nla_put(skb, NL80211_ATTR_VENDOR_DATA, len, data);
    err = cfg80211_vendor_cmd_reply(skb);
    if (err)
        printk("cfg80211_vendor_cmd_reply failed!ret=%d\n", err);
exit:
    return err;
}

VENDOR_CMD_FUNC_IMPL(setOffloadState)
{
    int rem, type, err = 0;
    const struct nlattr *iter;
    u32_boolean enabled = 0;
    u32_boolean *p_enabled = NULL;
    u32_boolean reply = 0;

    printk("mdnsOffload: setOffloadState\n");
    nla_for_each_attr(iter, data, len, rem) {
        type = nla_type(iter);
        //printk("mdnsOffload: attr type:%d\n", type);
        switch (type) {
            case WIFI_MDNS_OFFLOAD_ATTRIBUTE_STATE:
                enabled = nla_get_u32(iter);
                p_enabled = &enabled;
                break;
            default:
                printk("mdnsOffload: unknown type:%d\n", type);
                break;
        }
    }
    if (!p_enabled) {
        err = -EINVAL;
        goto exit;
    }
    printk("mdnsOffload: setOffloadState: enabled:%d\n", enabled);
    if (mdns_offload_ops.setOffloadState) {
        reply = mdns_offload_ops.setOffloadState((u32_boolean)enabled);
        printk("mdnsOffload: setOffloadState: reply:%u\n", reply);
        err = __mdnsOffload_send_vendor_cmd_reply(wiphy,
            WIFI_MDNS_OFFLOAD_SET_STATE,
            &reply, sizeof(reply));
    } else {
        printk("mdnsOffload: setOffloadState: unsupported!\n");
        err = -EPERM;
        goto exit;
    }
exit:
    if (err)
        printk("mdnsOffload: setOffloadState: failed!err:%d\n", err);
    return err;
}

VENDOR_CMD_FUNC_IMPL(resetAll)
{
    int err = 0;

    printk("mdnsOffload: resetAll\n");
    if (mdns_offload_ops.resetAll)
        mdns_offload_ops.resetAll();
    else {
        printk("mdnsOffload: resetAll: unsupported!\n");
        err = -EPERM;
        goto exit;
    }
exit:
    if (err)
        printk("mdnsOffload: resetAll: failed!err=%d\n", err);
    return err;
}

VENDOR_CMD_FUNC_IMPL(addProtocolResponses)
{
    int rem, type, err = 0, i = 0, size = 0;
    const struct nlattr *iter;
    char ifname[32];
    char *p_ifname = NULL;
    int reply = 0;
    mdnsProtocolData offloadData;
    uint32_t pkt_len = 0, criteriaListNum = 0;
    uint32_t *p_pkt_len = NULL, *p_criteriaListNum = NULL;
    unsigned char *pkt_data = NULL;
    matchCriteria *criteriaList = NULL;
    char *qname = NULL;

    printk("mdnsOffload: addProtocolResponses\n");
    memset(ifname, 0, sizeof(ifname));
    memset(&offloadData, 0, sizeof(offloadData));
    nla_for_each_attr(iter, data, len, rem) {
        type = nla_type(iter);
        //printk("mdnsOffload: attr type:%d\n", type);
        switch (type) {
            case WIFI_MDNS_OFFLOAD_ATTRIBUTE_NETWORK_INTERFACE:
                strncpy(ifname, (char *)nla_data(iter), sizeof(ifname) - 1);
                p_ifname = ifname;
                break;
            case WIFI_MDNS_OFFLOAD_ATTRIBUTE_OFFLOAD_PKT_LEN:
                pkt_len = nla_get_u32(iter);
                p_pkt_len = &pkt_len;
                break;
            case WIFI_MDNS_OFFLOAD_ATTRIBUTE_OFFLOAD_PKT_DATA:
                if (pkt_len > 0) {
                    pkt_data = (unsigned char *)kmalloc(pkt_len, GFP_KERNEL);
                    if (!pkt_data) {
                        printk("mdnsOffload: alloc failed!\n");
                        err = -ENOMEM;
                        goto exit;
                    }
                    memset(pkt_data, 0, pkt_len);
                    memcpy(pkt_data, nla_data(iter), pkt_len);
                }
                break;
            case WIFI_MDNS_OFFLOAD_ATTRIBUTE_MATCH_CRITERIA_NUM:
                criteriaListNum = nla_get_u32(iter);
                p_criteriaListNum = &criteriaListNum;
                break;
            case WIFI_MDNS_OFFLOAD_ATTRIBUTE_MATCH_CRITERIA_DATA:
                if (criteriaListNum > 0) {
                    size = criteriaListNum * sizeof(matchCriteria);
                    criteriaList = (matchCriteria *)kmalloc(size, GFP_KERNEL);
                    if (!criteriaList) {
                        printk("mdnsOffload: alloc failed!\n");
                        err = -ENOMEM;
                        goto exit;
                    }
                    memset(criteriaList, 0, size);
                    memcpy(criteriaList, nla_data(iter), size);
                }
                break;
            default:
                printk("mdnsOffload: unknown type:%d\n", type);
                break;
        }
    }
    if (!p_ifname || !p_pkt_len || !p_criteriaListNum
        || !pkt_data || !criteriaList) {
        err = -EINVAL;
        goto exit;
    }
    printk("mdnsOffload: addProtocolResponses: ifname:%s\n", ifname);
    printk("mdnsOffload: addProtocolResponses: pkt_len:%u\n", pkt_len);
    printk("mdnsOffload: addProtocolResponses: criteriaListNum:%u\n",
        criteriaListNum);
    printk("mdnsOffload: addProtocolResponses: dump:\n");
    printk("criteria list:\n");
    for (i = 0; i < criteriaListNum; i++) {
        qname = __mdnsOffload_decode_qname(pkt_data, pkt_len,
            criteriaList[i].nameOffset);
        printk("%d. type:%d\tnameOffset:%d\tname:%s\n", i + 1,
            criteriaList[i].type,
            criteriaList[i].nameOffset,
            (qname && strlen(qname) > 0) ? qname : "none");
        kfree(qname);
        qname = NULL;
    }
    printk("rawOffloadPacket:\n");
    __mdnsOffload_dump_msg(pkt_data, pkt_len);
    if (mdns_offload_ops.addProtocolResponses) {
        offloadData.rawOffloadPacketLen = pkt_len;
        offloadData.rawOffloadPacket = pkt_data;
        offloadData.matchCriteriaListNum = criteriaListNum;
        offloadData.matchCriteriaList = criteriaList;
        reply = mdns_offload_ops.addProtocolResponses(ifname, &offloadData);
        printk("mdnsOffload: addProtocolResponses: reply:%d\n", reply);
        err = __mdnsOffload_send_vendor_cmd_reply(wiphy,
            WIFI_MDNS_OFFLOAD_ADD_PROTOCOL_RESPONSES,
            &reply, sizeof(reply));
    } else {
        printk("mdnsOffload: addProtocolResponses: unsupported!\n");
        err = -EPERM;
        goto exit;
    }
exit:
    kfree(pkt_data);
    kfree(criteriaList);
    if (err)
        printk("mdnsOffload: addProtocolResponses: failed!err:%d\n", err);
    return err;
}

VENDOR_CMD_FUNC_IMPL(removeProtocolResponses)
{
    int rem, type, err = 0;
    const struct nlattr *iter;
    int recordKey = -1;
    int *p_recordKey = NULL;

    printk("mdnsOffload: removeProtocolResponses\n");
    nla_for_each_attr(iter, data, len, rem) {
        type = nla_type(iter);
        //printk("mdnsOffload: attr type:%d\n", type);
        switch (type) {
            case WIFI_MDNS_OFFLOAD_ATTRIBUTE_RECORD_KEY:
                recordKey = nla_get_u32(iter);
                p_recordKey = &recordKey;
                break;
            default:
                printk("mdnsOffload: unknown type:%d\n", type);
                break;
        }
    }
    if (!p_recordKey) {
        err = -EINVAL;
        goto exit;
    }
    printk("mdnsOffload: removeProtocolResponses: recordKey:%d\n",
        recordKey);
    if (mdns_offload_ops.removeProtocolResponses) {
        mdns_offload_ops.removeProtocolResponses(recordKey);
    } else {
        printk("mdnsOffload: removeProtocolResponses: unsupported!\n");
        err = -EPERM;
        goto exit;
    }
exit:
    if (err)
        printk("mdnsOffload: removeProtocolResponses: failed!err:%d\n", err);
    return err;
}

VENDOR_CMD_FUNC_IMPL(getAndResetHitCounter)
{
    int rem, type, err = 0;
    const struct nlattr *iter;
    int recordKey = -1;
    int *p_recordKey = NULL;
    int reply = 0;

    printk("mdnsOffload: getAndResetHitCounter\n");
    nla_for_each_attr(iter, data, len, rem) {
        type = nla_type(iter);
        //printk("mdnsOffload: attr type:%d\n", type);
        switch (type) {
            case WIFI_MDNS_OFFLOAD_ATTRIBUTE_RECORD_KEY:
                recordKey = nla_get_u32(iter);
                p_recordKey = &recordKey;
                break;
            default:
                printk("mdnsOffload: unknown type:%d\n", type);
                break;
        }
    }
    if (!p_recordKey) {
        err = -EINVAL;
        goto exit;
    }
    printk("mdnsOffload: getAndResetHitCounter: recordKey:%d\n",
        recordKey);
    if (mdns_offload_ops.getAndResetHitCounter) {
        reply = mdns_offload_ops.getAndResetHitCounter(recordKey);
        printk("mdnsOffload: getAndResetHitCounter: reply:%d\n", reply);
        err = __mdnsOffload_send_vendor_cmd_reply(wiphy,
            WIFI_MDNS_OFFLOAD_GET_AND_RESET_HIT_COUNTER,
            &reply, sizeof(reply));
    } else {
        printk("mdnsOffload: getAndResetHitCounter: unsupported!\n");
        err = -EPERM;
        goto exit;
    }
exit:
    if (err)
        printk("mdnsOffload: getAndResetHitCounter: failed!err:%d\n", err);
    return err;
}

VENDOR_CMD_FUNC_IMPL(getAndResetMissCounter)
{
    int err = 0;
    int reply = 0;

    printk("mdnsOffload: getAndResetMissCounter\n");
    if (mdns_offload_ops.getAndResetMissCounter) {
        reply = mdns_offload_ops.getAndResetMissCounter();
        printk("mdnsOffload: getAndResetMissCounter: reply:%d\n", reply);
        err = __mdnsOffload_send_vendor_cmd_reply(wiphy,
            WIFI_MDNS_OFFLOAD_GET_AND_RESET_MISS_COUNTER,
            &reply, sizeof(reply));
    } else {
        printk("mdnsOffload: getAndResetMissCounter: unsupported!\n");
        err = -EPERM;
        goto exit;
    }
exit:
    if (err)
        printk("mdnsOffload: getAndResetMissCounter: failed!err:%d\n", err);
    return err;
}

VENDOR_CMD_FUNC_IMPL(addToPassthroughList)
{
    int rem, type, err = 0;
    const struct nlattr *iter;
    char ifname[32];
    char *p_ifname = NULL;
    char qname[64];
    char *p_qname = NULL;
    u32_boolean reply = 0;

    printk("mdnsOffload: addToPassthroughList\n");
    memset(ifname, 0, sizeof(ifname));
    memset(qname, 0, sizeof(qname));
    nla_for_each_attr(iter, data, len, rem) {
        type = nla_type(iter);
        //printk("mdnsOffload: attr type:%d\n", type);
        switch (type) {
            case WIFI_MDNS_OFFLOAD_ATTRIBUTE_NETWORK_INTERFACE:
                strncpy(ifname, (char *)nla_data(iter), sizeof(ifname) - 1);
                p_ifname = ifname;
                break;
            case WIFI_MDNS_OFFLOAD_ATTRIBUTE_QNAME:
                strncpy(qname, (char *)nla_data(iter), sizeof(qname) - 1);
                p_qname = qname;
                break;
            default:
                printk("mdnsOffload: unknown type:%d\n", type);
                break;
        }
    }
    if (!p_ifname || !p_qname) {
        err = -EINVAL;
        goto exit;
    }
    printk("mdnsOffload: addToPassthroughList: ifname:%s\n", ifname);
    printk("mdnsOffload: addToPassthroughList: qname:%s\n", qname);
    if (mdns_offload_ops.addToPassthroughList) {
        reply = mdns_offload_ops.addToPassthroughList(ifname, qname);
        printk("mdnsOffload: addToPassthroughList: reply:%u\n", reply);
        err = __mdnsOffload_send_vendor_cmd_reply(wiphy,
            WIFI_MDNS_OFFLOAD_ADD_TO_PASSTHROUGH_LIST,
            &reply, sizeof(reply));
    } else {
        printk("mdnsOffload: addToPassthroughList: unsupported!\n");
        err = -EPERM;
        goto exit;
    }
exit:
    if (err)
        printk("mdnsOffload: addToPassthroughList: failed!err:%d\n", err);
    return err;
}

VENDOR_CMD_FUNC_IMPL(removeFromPassthroughList)
{
    int rem, type, err = 0;
    const struct nlattr *iter;
    char ifname[32];
    char *p_ifname = NULL;
    char qname[64];
    char *p_qname = NULL;

    printk("mdnsOffload: removeFromPassthroughList\n");
    memset(ifname, 0, sizeof(ifname));
    memset(qname, 0, sizeof(qname));
    nla_for_each_attr(iter, data, len, rem) {
        type = nla_type(iter);
        //printk("mdnsOffload: attr type:%d\n", type);
        switch (type) {
            case WIFI_MDNS_OFFLOAD_ATTRIBUTE_NETWORK_INTERFACE:
                strncpy(ifname, (char *)nla_data(iter), sizeof(ifname) - 1);
                p_ifname = ifname;
                break;
            case WIFI_MDNS_OFFLOAD_ATTRIBUTE_QNAME:
                strncpy(qname, (char *)nla_data(iter), sizeof(qname) - 1);
                p_qname = qname;
                break;
            default:
                printk("mdnsOffload: unknown type:%d\n", type);
                break;
        }
    }
    printk("mdnsOffload: removeFromPassthroughList: ifname:%s\n", ifname);
    printk("mdnsOffload: removeFromPassthroughList: qname:%s\n", qname);
    if (!p_ifname || !p_qname) {
        err = -EINVAL;
        goto exit;
    }
    if (mdns_offload_ops.removeFromPassthroughList) {
        mdns_offload_ops.removeFromPassthroughList(ifname, qname);
    } else {
        printk("mdnsOffload: removeFromPassthroughList: unsupported!\n");
        err = -EPERM;
        goto exit;
    }
exit:
    if (err)
        printk("mdnsOffload: removeFromPassthroughList: failed!err:%d\n", err);
    return err;
}

VENDOR_CMD_FUNC_IMPL(setPassthroughBehavior)
{
    int rem, type, err = 0;
    const struct nlattr *iter;
    char ifname[32];
    char *p_ifname = NULL;
    int behavior = -1;
    int *p_behavior = NULL;

    printk("mdnsOffload: setPassthroughBehavior\n");
    memset(ifname, 0, sizeof(ifname));
    nla_for_each_attr(iter, data, len, rem) {
        type = nla_type(iter);
        //printk("mdnsOffload: attr type:%d\n", type);
        switch (type) {
            case WIFI_MDNS_OFFLOAD_ATTRIBUTE_NETWORK_INTERFACE:
                strncpy(ifname, (char *)nla_data(iter), sizeof(ifname) - 1);
                p_ifname = ifname;
                break;
            case WIFI_MDNS_OFFLOAD_ATTRIBUTE_PASSTHROUGH_BEHAVIOR:
                behavior = nla_get_u32(iter);
                p_behavior = &behavior;
                break;
            default:
                printk("mdnsOffload: unknown type:%d\n", type);
                break;
        }
    }
    printk("mdnsOffload: setPassthroughBehavior: ifname:%s\n", ifname);
    printk("mdnsOffload: setPassthroughBehavior: behavior:%d\n", behavior);
    if (!p_ifname || !p_behavior) {
        err = -EINVAL;
        goto exit;
    }
    if (mdns_offload_ops.setPassthroughBehavior) {
        mdns_offload_ops.setPassthroughBehavior(ifname,
            (passthroughBehavior)behavior);
    } else {
        printk("mdnsOffload: setPassthroughBehavior: unsupported!\n");
        err = -EPERM;
        goto exit;
    }
exit:
    if (err)
        printk("mdnsOffload: setPassthroughBehavior: failed!err:%d\n", err);
    return err;
}

#define MDNS_OFFLOAD_VENDOR_CMD \
_MDNS_OFFLOAD_VENDOR_CMD(WIFI_MDNS_OFFLOAD_SET_STATE,\
  VENDOR_CMD_FUNC(setOffloadState)),\
_MDNS_OFFLOAD_VENDOR_CMD(WIFI_MDNS_OFFLOAD_RESET_ALL,\
  VENDOR_CMD_FUNC(resetAll)),\
_MDNS_OFFLOAD_VENDOR_CMD(WIFI_MDNS_OFFLOAD_ADD_PROTOCOL_RESPONSES,\
  VENDOR_CMD_FUNC(addProtocolResponses)),\
_MDNS_OFFLOAD_VENDOR_CMD(WIFI_MDNS_OFFLOAD_REMOVE_PROTOCOL_RESPONSES,\
  VENDOR_CMD_FUNC(removeProtocolResponses)),\
_MDNS_OFFLOAD_VENDOR_CMD(WIFI_MDNS_OFFLOAD_GET_AND_RESET_HIT_COUNTER,\
  VENDOR_CMD_FUNC(getAndResetHitCounter)),\
_MDNS_OFFLOAD_VENDOR_CMD(WIFI_MDNS_OFFLOAD_GET_AND_RESET_MISS_COUNTER,\
  VENDOR_CMD_FUNC(getAndResetMissCounter)),\
_MDNS_OFFLOAD_VENDOR_CMD(WIFI_MDNS_OFFLOAD_ADD_TO_PASSTHROUGH_LIST,\
  VENDOR_CMD_FUNC(addToPassthroughList)),\
_MDNS_OFFLOAD_VENDOR_CMD(WIFI_MDNS_OFFLOAD_REMOVE_FROM_PASSTHROUGH_LIST,\
  VENDOR_CMD_FUNC(removeFromPassthroughList)),\
_MDNS_OFFLOAD_VENDOR_CMD(WIFI_MDNS_OFFLOAD_SET_PASSTHROUGH_BEHAVIOR,\
  VENDOR_CMD_FUNC(setPassthroughBehavior))

#endif /* __MDNS_OFFLOAD_H__ */

