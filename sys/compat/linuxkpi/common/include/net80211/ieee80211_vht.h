/*-
 * Copyright (c) 2026 The DragonFly Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _NET80211_IEEE80211_VHT_H_
#define _NET80211_IEEE80211_VHT_H_

/* DragonFly 802.11 VHT (Very High Throughput) definitions for LinuxKPI */

/* VHT operation modes */
#define IEEE80211_VHT_OPMODE_VHT40_MHZ    0x00
#define IEEE80211_VHT_OPMODE_VHT80_MHZ    0x01
#define IEEE80211_VHT_OPMODE_VHT160_MHZ   0x02
#define IEEE80211_VHT_OPMODE_VHT80_80_MHZ 0x03

/* VHT channel widths */
#define IEEE80211_VHT_CHANWIDTH_80MHZ     80
#define IEEE80211_VHT_CHANWIDTH_160MHZ    160
#define IEEE80211_VHT_CHANWIDTH_80P80MHZ  8080

/* VHT capabilities flags */
#define IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_11454    0x00000002
#define IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_7991     0x00000001
#define IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_3895     0x00000000
#define IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160MHZ   0x0000000C
#define IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ 0x00000008
#define IEEE80211_VHT_CAP_RXLDPC                 0x00000010
#define IEEE80211_VHT_CAP_SHORT_GI_80            0x00000020
#define IEEE80211_VHT_CAP_SHORT_GI_160           0x00000040
#define IEEE80211_VHT_CAP_TXSTBC                 0x00000080
#define IEEE80211_VHT_CAP_RXSTBC_1               0x00000100
#define IEEE80211_VHT_CAP_RXSTBC_2               0x00000200
#define IEEE80211_VHT_CAP_RXSTBC_3               0x00000300
#define IEEE80211_VHT_CAP_RXSTBC_4               0x00000400
#define IEEE80211_VHT_CAP_SU_BEAMFORMER_CAPABLE  0x00000800
#define IEEE80211_VHT_CAP_SU_BEAMFORMEE_CAPABLE  0x00001000
#define IEEE80211_VHT_CAP_MU_BEAMFORMER_CAPABLE  0x00080000
#define IEEE80211_VHT_CAP_MU_BEAMFORMEE_CAPABLE  0x00100000
#define IEEE80211_VHT_CAP_VHT_TXOP_PS            0x00200000
#define IEEE80211_VHT_CAP_HTC_VHT                0x00400000
#define IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_SHIFT 23
#define IEEE80211_VHT_CAP_VHT_LINK_ADAPTATION_VHT_UNSOL_MFB 0x08000000
#define IEEE80211_VHT_CAP_VHT_LINK_ADAPTATION_VHT_MRQ_MFB   0x0C000000
#define IEEE80211_VHT_CAP_RX_ANTENNA_PATTERN     0x10000000
#define IEEE80211_VHT_CAP_TX_ANTENNA_PATTERN     0x20000000

/* VHT MCS (Modulation and Coding Scheme) defines */
#define IEEE80211_VHT_MCS_SUPPORT_0_7          0
#define IEEE80211_VHT_MCS_SUPPORT_0_8          1
#define IEEE80211_VHT_MCS_SUPPORT_0_9          2
#define IEEE80211_VHT_MCS_NOT_SUPPORTED        3

/* VHT maximum MCS set size */
#define IEEE80211_VHT_MCS_NUM_STREAMS          8

/* VHT NSS (Number of Spatial Streams) limits */
#define IEEE80211_VHT_MAX_NSS                  8
#define IEEE80211_VHT_MAX_MCS                  9

/* VHT rate defines */
#define IEEE80211_VHT_MCS_RATE_0               0
#define IEEE80211_VHT_MCS_RATE_1               1
#define IEEE80211_VHT_MCS_RATE_2               2
#define IEEE80211_VHT_MCS_RATE_3               3
#define IEEE80211_VHT_MCS_RATE_4               4
#define IEEE80211_VHT_MCS_RATE_5               5
#define IEEE80211_VHT_MCS_RATE_6               6
#define IEEE80211_VHT_MCS_RATE_7               7
#define IEEE80211_VHT_MCS_RATE_8               8
#define IEEE80211_VHT_MCS_RATE_9               9

/* VHT NSS mask for rates */
#define IEEE80211_VHT_NSS_MASK                 0x0F
#define IEEE80211_VHT_MCS_MASK                 0x0F

/* VHT STBC defines */
#define IEEE80211_VHT_STBC_NOT_SUPPORTED       0
#define IEEE80211_VHT_STBC_SUPPORTED           1

/* VHT beamforming defines */
#define IEEE80211_VHT_BF_NOT_SUPPORTED         0
#define IEEE80211_VHT_BF_SUPPORTED             1

/* VHT SGI (Short Guard Interval) defines */
#define IEEE80211_VHT_SGI_80                   0x01
#define IEEE80211_VHT_SGI_160                  0x02

#endif /* _NET80211_IEEE80211_VHT_H_ */
