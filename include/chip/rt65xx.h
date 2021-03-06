/*
 ***************************************************************************
 * MediaTek Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 1997-2012, MediaTek, Inc.
 *
 * All rights reserved. MediaTek source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of MediaTek. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of MediaTek Technology, Inc. is obtained.
 ***************************************************************************

*/

#ifndef __MT65XX_H__
#define __MT65XX_H__


#include "mt76x2.h"

struct rtmp_adapter;

/* b'00: 2.4G+5G external PA, b'01: 5G external PA, b'10: 2.4G external PA, b'11: Internal PA */
#define EXT_PA_2G_5G		0x0
#define EXT_PA_5G_ONLY		0x1
#define EXT_PA_2G_ONLY		0x2

#define INT_PA_2G_5G		0x3
#define INT_PA_5G			0x2
#define INT_PA_2G			0x1

#define MAX_CHECK_COUNT 200

#ifdef RTMP_USB_SUPPORT
VOID RT65xxUsbAsicRadioOn(struct rtmp_adapter *pAd, UCHAR Stage);
VOID RT65xxUsbAsicRadioOff(struct rtmp_adapter *pAd, UCHAR Stage);
#endif

/*
	EEPROM format
*/

#ifdef RT_BIG_ENDIAN
typedef union _EEPROM_NIC_CINFIG0_STRUC {
	struct {
		USHORT Rsv:5;
		USHORT PACurrent:1;
		USHORT PAType:2;			/* 00: 2.4G+5G external PA, 01: 5G external PA, 10: 2.4G external PA, 11: Internal PA */
		USHORT TxPath:4;			/* 1: 1T, 2: 2T, 3: 3T */
		USHORT RxPath:4;			/* 1: 1R, 2: 2R, 3: 3R */
	} field;
	USHORT word;
} EEPROM_NIC_CONFIG0_STRUC, *PEEPROM_NIC_CONFIG0_STRUC;
#else
typedef union _EEPROM_NIC_CINFIG0_STRUC {
	struct {
		USHORT RxPath:4;			/* 1: 1R, 2: 2R, 3: 3R */
		USHORT TxPath:4;			/* 1: 1T, 2: 2T, 3: 3T */
		USHORT PAType:2;			/* 00: 2.4G+5G external PA, 01: 5G external PA, 10: 2.4G external PA, 11: Internal PA */
		USHORT PACurrent:1;
		USHORT Rsv:5;
	} field;
	USHORT word;
} EEPROM_NIC_CONFIG0_STRUC, *PEEPROM_NIC_CONFIG0_STRUC;
#endif

VOID RT65xxDisableTxRx(struct rtmp_adapter *pAd, UCHAR Level);
void MT76xx_PciMlmeRadioOFF(struct rtmp_adapter *pAd);
void MT76xx_PciMlmeRadioOn(struct rtmp_adapter *pAd);
VOID dump_pwr_info(struct rtmp_adapter *pAd);

#endif /* __MT65XX_H__ */

