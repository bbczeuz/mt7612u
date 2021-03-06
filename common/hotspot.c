/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2011, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

	Module Name:
	hotspot.c

	Abstract:
	hotspot2.0 features

	Revision History:
	Who         When          What
	--------    ----------    ----------------------------------------------
*/

#include "rt_config.h"

extern UCHAR BROADCAST_ADDR[MAC_ADDR_LEN];

void wext_hotspot_onoff_event(struct net_device *net_dev, int onoff)
{
	struct hs_onoff *hotspot_onoff;
	u16 buflen = 0;
	char *buf;

	buflen = sizeof(*hotspot_onoff);

	buf = kmalloc(buflen, GFP_ATOMIC);
	memset(buf, buflen);

	hotspot_onoff = (struct hs_onoff *)buf;
	hotspot_onoff->ifindex = RtmpOsGetNetIfIndex(net_dev);
	hotspot_onoff->hs_onoff = onoff;

	RtmpOSWrielessEventSend(net_dev, RT_WLAN_EVENT_CUSTOM,
					OID_802_11_HS_ONOFF, NULL, (u8 *)buf, buflen);
	kfree(buf);
}


void HotspotOnOffEvent(struct net_device *net_dev, int onoff)
{

	wext_hotspot_onoff_event(net_dev, onoff);
}


static void wext_hotspot_ap_reload_event(struct net_device *net_dev)
{
	struct hs_onoff *hotspot_onoff;
	u16 buflen = 0;
	char *buf;

	buflen = sizeof(*hotspot_onoff);
	buf = kmalloc(buflen, GFP_ATOMIC);
	memset(buf, buflen);

	hotspot_onoff = (struct hs_onoff *)buf;
	hotspot_onoff->ifindex = RtmpOsGetNetIfIndex(net_dev);

	RtmpOSWrielessEventSend(net_dev, RT_WLAN_EVENT_CUSTOM,
					OID_802_11_HS_AP_RELOAD, NULL, (u8 *)buf, buflen);
	kfree(buf);
}


void HotspotAPReload(struct net_device *net_dev)
{
	wext_hotspot_ap_reload_event(net_dev);
}


#ifdef CONFIG_AP_SUPPORT
bool HSIPv4Check(
			IN struct rtmp_adapter *pAd,
			u8 *pWcid,
			struct sk_buff *pPacket,
			u8 *pSrcBuf,
			uint16_t srcPort,
			uint16_t dstPort)
{
	UCHAR apidx = RTMP_GET_PACKET_NET_DEVICE(pPacket);
	MULTISSID_STRUCT *pMbss = &pAd->ApCfg.MBSSID[apidx];

	if (pMbss->HotSpotCtrl.HotSpotEnable)
	{
		if (srcPort  == 0x43 && dstPort == 0x44)
		{
			//UCHAR *pTargetIPAddr = pSrcBuf + 24;
			/* Client hardware address */
			UCHAR *pTargetMACAddr = pSrcBuf + 36;

			/* Convert group-address DHCP packets to individually-addressed 802.11 frames */
			if (*pWcid == MCAST_WCID && pMbss->HotSpotCtrl.DGAFDisable)
			{
					UCHAR Index;
					u8 *pSrcBufOriginal = GET_OS_PKT_DATAPTR(pPacket);
					for (Index = 0; Index < MAC_ADDR_LEN; Index++)
					{
						DBGPRINT(RT_DEBUG_OFF, ("Original source address(%d) = %02x\n",Index, pSrcBufOriginal[Index]));
						pSrcBufOriginal[Index] = pTargetMACAddr[Index];
						DBGPRINT(RT_DEBUG_OFF, ("Source address(%d) = %02x\n", Index, pSrcBuf[Index]));
					}

					DBGPRINT(RT_DEBUG_OFF, ("Convert broadcast dhcp to unicat frame when dgaf disable\n"));

					if (!ApAllowToSendPacket(pAd, &pAd->ApCfg.MBSSID[apidx].wdev, pPacket, pWcid))
						return false;

					RTMP_SET_PACKET_WCID(pPacket, *pWcid);
			}
		}
	}
	return true;
}


static bool IsICMPv4EchoPacket(
			IN struct rtmp_adapter *pAd,
			IN u8 *pData)
{
	uint16_t ProtoType;
	UCHAR *Pos = pData;

	memmove(&ProtoType, pData, 2);
	ProtoType = OS_NTOHS(ProtoType);
	Pos += 2;

	if (ProtoType == ETH_P_IP)
	{
		Pos += 9;

		if (*Pos == 0x01)
		{
			Pos += 11;
			if (*Pos == 0x08)
			{
				Pos++;
				if (*Pos == 0x00)
				{
					DBGPRINT(RT_DEBUG_OFF, ("ICMPv4Echp Packet\n"));
					return true;
				}
			}
		}

	}

	return false;
}

bool L2FilterInspection(
			IN struct rtmp_adapter *pAd,
			IN PHOTSPOT_CTRL pHSCtrl,
			IN u8 *pData)
{
	if (IsICMPv4EchoPacket(pAd, pData))
	{
		if (pHSCtrl->ICMPv4Deny)
			return true;
		else
			return false;
	}

	return false;
}

bool ProbeReqforHSAP(
	IN struct rtmp_adapter *pAd,
	IN UCHAR APIndex,
	IN struct _PEER_PROBE_REQ_PARAM *ProbeReqParam)
{

	PHOTSPOT_CTRL pHSCtrl;
	pHSCtrl = &pAd->ApCfg.MBSSID[APIndex].HotSpotCtrl;

	if (pHSCtrl->HotSpotEnable && ProbeReqParam->IsIWIE)
	{
		if (ProbeReqParam->IsHessid && pHSCtrl->IsHessid)
		{
			if(NdisEqualMemory(ProbeReqParam->Hessid, pHSCtrl->Hessid, MAC_ADDR_LEN) ||
		   	  NdisEqualMemory(ProbeReqParam->Hessid, BROADCAST_ADDR, MAC_ADDR_LEN))
				;
			else
				return false;
		}

		if ((ProbeReqParam->AccessNetWorkType == pHSCtrl->AccessNetWorkType) ||
			(ProbeReqParam->AccessNetWorkType == 0x0f))
			return true;
		else
			return false;
	}
	else
		return true;
}

inline INT Set_HotSpot_DGAF(
	IN struct rtmp_adapter *pAd,
	UCHAR Disable)
{
	struct os_cookie *pObj = pAd->OS_Cookie;
	UCHAR APIndex = pObj->ioctl_if;
	PHOTSPOT_CTRL pHSCtrl;

	pHSCtrl = &pAd->ApCfg.MBSSID[APIndex].HotSpotCtrl;

	pHSCtrl->DGAFDisable = Disable;

	return 0;
}

INT Set_HotSpot_Param(
	IN struct rtmp_adapter *pAd,
	uint32_t Param,
	uint32_t Value)
{

	struct os_cookie *pObj = pAd->OS_Cookie;
	UCHAR APIndex = pObj->ioctl_if;
	PHOTSPOT_CTRL pHSCtrl;
	PWNM_CTRL pWNMCtrl;
	PGAS_CTRL pGASCtrl;

	pHSCtrl = &pAd->ApCfg.MBSSID[APIndex].HotSpotCtrl;
	pWNMCtrl = &pAd->ApCfg.MBSSID[APIndex].WNMCtrl;
	pGASCtrl = &pAd->ApCfg.MBSSID[APIndex].GASCtrl;

	switch (Param) {
		case PARAM_DGAF_DISABLED:
			pHSCtrl->DGAFDisable = Value;
			break;
		case PARAM_PROXY_ARP:
			pWNMCtrl->ProxyARPEnable = Value;
			break;
		case PARAM_L2_FILTER:
			pHSCtrl->L2Filter = Value;
			break;
		case PARAM_ICMPV4_DENY:
			pHSCtrl->ICMPv4Deny = Value;
			break;
		case PARAM_MMPDU_SIZE:
			pHSCtrl->MMPDUSize = Value;
			break;
		case PARAM_EXTERNAL_ANQP_SERVER_TEST:
			pGASCtrl->ExternalANQPServerTest = Value;
			break;
		default:
			DBGPRINT(RT_DEBUG_ERROR, ("Unknow Parameter\n"));
			break;
	}

	return 0;
}
#endif /* CONFIG_AP_SUPPORT */

VOID HSCtrlRemoveAllIE(PHOTSPOT_CTRL pHSCtrl)
{

	/* Remove all IE from daemon */
	if (pHSCtrl->P2PIELen)
	{
		pHSCtrl->P2PIELen = 0;
		kfree(pHSCtrl->P2PIE);
	}

	if (pHSCtrl->HSIndicationIELen)
	{
		pHSCtrl->HSIndicationIELen = 0;
		kfree(pHSCtrl->HSIndicationIE);
	}

	if (pHSCtrl->InterWorkingIELen)
	{
		pHSCtrl->InterWorkingIELen = 0;
		kfree(pHSCtrl->InterWorkingIE);
	}

	if (pHSCtrl->AdvertisementProtoIELen)
	{
		pHSCtrl->AdvertisementProtoIELen = 0;
		kfree(pHSCtrl->AdvertisementProtoIE);
	}

	if (pHSCtrl->QosMapSetIELen)
	{
		pHSCtrl->AdvertisementProtoIELen = 0;
		kfree(pHSCtrl->QosMapSetIE);
	}

	if (pHSCtrl->RoamingConsortiumIELen)
	{
		pHSCtrl->RoamingConsortiumIELen = 0;
		kfree(pHSCtrl->RoamingConsortiumIE);
	}
}


#ifdef CONFIG_AP_SUPPORT
VOID Clear_Hotspot_All_IE(
	IN struct rtmp_adapter *pAd)
{
	struct os_cookie *pObj = pAd->OS_Cookie;
	UCHAR APIndex = pObj->ioctl_if;
	PHOTSPOT_CTRL pHSCtrl;

	pHSCtrl = &pAd->ApCfg.MBSSID[APIndex].HotSpotCtrl;

	HSCtrlRemoveAllIE(pHSCtrl);
}
#endif

INT Set_HotSpot_OnOff(
	IN struct rtmp_adapter *pAd,
	IN UINT8 OnOff,
	IN UINT8 EventTrigger,
	IN UINT8 EventType)
{
	UCHAR *Buf;
	HSCTRL_EVENT_DATA *Event;
	uint32_t Len = 0;
#ifdef CONFIG_AP_SUPPORT
	struct os_cookie *pObj = pAd->OS_Cookie;
	UCHAR APIndex = pObj->ioctl_if;
#endif /* CONFIG_AP_SUPPORT */

	buf = kmalloc(sizeof(*Event), GFP_ATOMIC);
	if (!Buf) {
		DBGPRINT(RT_DEBUG_ERROR, ("%s Not available memory\n", __FUNCTION__));
		return false;
	}

	memset(Buf, sizeof(*Event));

	Event = (HSCTRL_EVENT_DATA *)Buf;
#ifdef CONFIG_STA_SUPPORT
	Event->ControlIndex = 0;
#endif /*CONFIG_STA_SUPPORT */
#ifdef CONFIG_AP_SUPPORT
	Event->ControlIndex = APIndex;
#endif /* CONFIG_STA_SUPPORT */
	Len += 1;

	Event->EventTrigger = EventTrigger;
	Len += 1;

	Event->EventType = EventType;
	Len += 1;

	if (OnOff)
		MlmeEnqueue(pAd, HSCTRL_STATE_MACHINE, HSCTRL_ON, Len, Buf, 0);
	else
		MlmeEnqueue(pAd, HSCTRL_STATE_MACHINE, HSCTRL_OFF, Len, Buf, 0);

	kfree(Buf);

	return true;
}


enum HSCTRL_STATE HSCtrlCurrentState(
	IN struct rtmp_adapter *pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	PHOTSPOT_CTRL pHSCtrl;
#ifdef CONFIG_AP_SUPPORT
	PHSCTRL_EVENT_DATA Event = (PHSCTRL_EVENT_DATA)Elem->Msg;
#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
	pHSCtrl = &pAd->ApCfg.MBSSID[Event->ControlIndex].HotSpotCtrl;
#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
	pHSCtrl = &pAd->StaCfg.HotSpotCtrl;
#endif /* CONFIG_STA_SUPPORT */

	return pHSCtrl->HSCtrlState;
}


VOID HSCtrlSetCurrentState(
	IN struct rtmp_adapter *pAd,
	IN MLME_QUEUE_ELEM *Elem,
	IN enum HSCTRL_STATE State)
{

	PHOTSPOT_CTRL pHSCtrl;
#ifdef CONFIG_AP_SUPPORT
	PHSCTRL_EVENT_DATA Event = (PHSCTRL_EVENT_DATA)Elem->Msg;
#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
	pHSCtrl = &pAd->ApCfg.MBSSID[Event->ControlIndex].HotSpotCtrl;
#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
	pHSCtrl = &pAd->StaCfg.HotSpotCtrl;
#endif /* CONFIG_STA_SUPPORT */

	pHSCtrl->HSCtrlState = State;
}


static VOID HSCtrlOn(
    IN struct rtmp_adapter *   pAd,
    IN MLME_QUEUE_ELEM  *Elem)
{
	PHOTSPOT_CTRL pHSCtrl;
	PGAS_CTRL pGASCtrl;
	struct net_device *NetDev;
	HSCTRL_EVENT_DATA *Event = (HSCTRL_EVENT_DATA *)Elem->Msg;

	printk("%s\n", __FUNCTION__);

#ifdef CONFIG_STA_SUPPORT
	NetDev = pAd->net_dev;
	pHSCtrl = &pAd->StaCfg.HotSpotCtrl;
	pGASCtrl = &pAd->StaCfg.GASCtrl;
#endif /* CONFIG_STA_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
	NetDev = pAd->ApCfg.MBSSID[Event->ControlIndex].wdev.if_dev;
	pHSCtrl = &pAd->ApCfg.MBSSID[Event->ControlIndex].HotSpotCtrl;
	pGASCtrl = &pAd->ApCfg.MBSSID[Event->ControlIndex].GASCtrl;
#endif /* CONFIG_AP_SUPPORT */

	RTMP_SEM_LOCK(&pGASCtrl->GASPeerListLock);
	DlListInit(&pGASCtrl->GASPeerList);
	RTMP_SEM_UNLOCK(&pGASCtrl->GASPeerListLock);

	pHSCtrl->HotSpotEnable = 1;
	pHSCtrl->HSDaemonReady = 1;
#ifdef CONFIG_AP_SUPPORT
	APMakeAllBssBeacon(pAd);
	APUpdateAllBeaconFrame(pAd);
#endif /* CONFIG_AP_SUPPORT */

	HSCtrlSetCurrentState(pAd, Elem, HSCTRL_IDLE);

	/* Send indication to daemon */
	if (Event->EventTrigger) {
		switch (Event->EventType) {
		case HS_ON_OFF_BASE:
			HotspotOnOffEvent(NetDev, 1);
			break;
		case HS_AP_RELOAD:
			HotspotAPReload(NetDev);
			break;
		default:
			DBGPRINT(RT_DEBUG_ERROR, ("%s: Unknow event type(%d)\n", __FUNCTION__, Event->EventType));
			break;
		}
	}
}


static VOID HSCtrlInit(
	IN struct rtmp_adapter *pAd)
{
	PHOTSPOT_CTRL pHSCtrl;
#ifdef CONFIG_AP_SUPPORT
	UCHAR APIndex;
#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
	pHSCtrl = &pAd->StaCfg.HotSpotCtrl;
	memset(pHSCtrl, sizeof(*pHSCtrl));
	pHSCtrl->HotSpotEnable = 0;
	pHSCtrl->HSCtrlState = HSCTRL_IDLE;
#endif /* CONFIG_STA_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
	for (APIndex = 0; APIndex < MAX_MBSSID_NUM(pAd); APIndex++)
	{
		pHSCtrl = &pAd->ApCfg.MBSSID[APIndex].HotSpotCtrl;
		memset(pHSCtrl, sizeof(*pHSCtrl));
		pHSCtrl->HotSpotEnable = 0;
		pHSCtrl->HSCtrlState = HSCTRL_IDLE;
	}
#endif /* CONFIG_AP_SUPPORT */
}


VOID HSCtrlExit(
	IN struct rtmp_adapter *pAd)
{
	PHOTSPOT_CTRL pHSCtrl;
#ifdef CONFIG_AP_SUPPORT
	UCHAR APIndex;
#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
	pHSCtrl = &pAd->StaCfg.HotSpotCtrl;

	/* Remove all IE */
	HSCtrlRemoveAllIE(pHSCtrl);
#endif /* CONFIG_STA_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
	for (APIndex = 0; APIndex < MAX_MBSSID_NUM(pAd); APIndex++)
	{
		pHSCtrl = &pAd->ApCfg.MBSSID[APIndex].HotSpotCtrl;

		/* Remove all IE */
		HSCtrlRemoveAllIE(pHSCtrl);
	}
#endif /* CONFIG_AP_SUPPORT */
}


VOID HSCtrlHalt(
	IN struct rtmp_adapter *pAd)
{

	PHOTSPOT_CTRL pHSCtrl;
#ifdef CONFIG_AP_SUPPORT
	UCHAR APIndex;
#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
	pHSCtrl = &pAd->StaCfg.HotSpotCtrl;
	pHSCtrl->HotSpotEnable = 0;
#endif /* CONFIG_STA_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
	for (APIndex = 0; APIndex < MAX_MBSSID_NUM(pAd); APIndex++)
	{
		pHSCtrl = &pAd->ApCfg.MBSSID[APIndex].HotSpotCtrl;
		pHSCtrl->HotSpotEnable = 0;

	}
#endif /* CONFIG_AP_SUPPORT */
}

static VOID HSCtrlOff(
    IN struct rtmp_adapter *   pAd,
    IN MLME_QUEUE_ELEM  *Elem)
{
	PHOTSPOT_CTRL pHSCtrl;
	PGAS_CTRL pGASCtrl;
	struct net_device *NetDev;
	HSCTRL_EVENT_DATA *Event = (HSCTRL_EVENT_DATA *)Elem->Msg;

	printk("%s\n", __FUNCTION__);

#ifdef CONFIG_AP_SUPPORT
	NetDev = pAd->ApCfg.MBSSID[Event->ControlIndex].wdev.if_dev;
	pHSCtrl = &pAd->ApCfg.MBSSID[Event->ControlIndex].HotSpotCtrl;
	pGASCtrl = &pAd->ApCfg.MBSSID[Event->ControlIndex].GASCtrl;
#endif /* CONFIG_AP_SUPPORT */

 	pHSCtrl->HotSpotEnable = 0;
	pHSCtrl->HSDaemonReady = 0;

#ifdef CONFIG_AP_SUPPORT
	APMakeAllBssBeacon(pAd);
	APUpdateAllBeaconFrame(pAd);
#endif /* CONFIG_AP_SUPPORT */

	HSCtrlSetCurrentState(pAd, Elem, HSCTRL_IDLE);

	if (Event->EventTrigger) {
		switch (Event->EventType) {
		case HS_ON_OFF_BASE:
			HotspotOnOffEvent(NetDev, 0);
			break;
		case HS_AP_RELOAD:
			HotspotAPReload(NetDev);
			break;
		default:
			DBGPRINT(RT_DEBUG_ERROR, ("%s: Unknow event type(%d)\n", __FUNCTION__, Event->EventType));
			break;
		}
	}
}

bool HotSpotEnable(
	IN struct rtmp_adapter *pAd,
	IN MLME_QUEUE_ELEM *Elem,
	IN INT Type)
{
	PHOTSPOT_CTRL pHSCtrl = NULL;

#ifdef CONFIG_AP_SUPPORT
	UCHAR APIndex;
	PGAS_EVENT_DATA Event;
	GAS_FRAME *GASFrame;
#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
	if (Type == GAS_STATE_MESSAGES)
	{
		Event =  (PGAS_EVENT_DATA)Elem->Msg;
		pHSCtrl = &pAd->ApCfg.MBSSID[Event->ControlIndex].HotSpotCtrl;
	}
	else if (Type == ACTION_STATE_MESSAGES)
	{
		GASFrame = (GAS_FRAME *)Elem->Msg;

		for (APIndex = 0; APIndex < MAX_MBSSID_NUM(pAd); APIndex++)
		{
			if (MAC_ADDR_EQUAL(GASFrame->Hdr.Addr3, pAd->ApCfg.MBSSID[APIndex].wdev.bssid))
			{
				pHSCtrl = &pAd->ApCfg.MBSSID[APIndex].HotSpotCtrl;
				break;
			}
		}

		if (!pHSCtrl)
		{
			DBGPRINT(RT_DEBUG_ERROR, ("%s Can not find Peer Control\n", __FUNCTION__));
				return false;
		}
	}
#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
	pHSCtrl = &pAd->StaCfg.HotSpotCtrl;
#endif /* CONFIG_STA_SUPPORT */

	return pHSCtrl->HotSpotEnable;
}


VOID HSCtrlStateMachineInit(
	IN	struct rtmp_adapter *	pAd,
	IN	STATE_MACHINE		*S,
	OUT	STATE_MACHINE_FUNC	Trans[])
{

	DBGPRINT(RT_DEBUG_TRACE, ("%s\n", __FUNCTION__));

	HSCtrlInit(pAd);

	StateMachineInit(S,	(STATE_MACHINE_FUNC*)Trans, MAX_HSCTRL_STATE, MAX_HSCTRL_MSG, (STATE_MACHINE_FUNC)Drop, HSCTRL_IDLE, HSCTRL_MACHINE_BASE);

	StateMachineSetAction(S, HSCTRL_IDLE, HSCTRL_ON, (STATE_MACHINE_FUNC)HSCtrlOn);
	StateMachineSetAction(S, HSCTRL_IDLE, HSCTRL_OFF, (STATE_MACHINE_FUNC)HSCtrlOff);
}
