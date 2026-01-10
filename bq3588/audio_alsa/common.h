/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ALSA_SND_COMMON_H
#define ALSA_SND_COMMON_H

/* Playback Path */
#define     SND_NUMID_PLAYBACK_PATH          55
#define     SND_ELEM_PLAYBACK_PATH           "OUT1 Switch"
#define     SND_OUT_CARD_OFF                 "0"   /* close play path */
#define     SND_OUT_CARD_RCV                 "1"   /* speaker */
#define     SND_OUT_CARD_SPK                 "1"   /* speaker */
#define     SND_OUT_CARD_HP                  "1"   /* headphone */
#define     SND_OUT_CARD_HP_NO_MIC           "1"   /* headphone */
#define     SND_OUT_CARD_BT                  "1"   /* bluetooth (Don't set!!!) */
#define     SND_OUT_CARD_SPK_HP              "1"   /* speaker and headphone */
#define     SND_OUT_CARD_RING_SPK            "1"   /* speaker */
#define     SND_OUT_CARD_RING_HP             "1"   /* headphone */
#define     SND_OUT_CARD_RING_HP_NO_MIC      "1"        /* headphone */
#define     SND_OUT_CARD_RING_SPK_HP         "1"  /* speaker and headphone */

/* Playback Path */
#define     SND_NUMID_PLAYBACK2_PATH         56
#define     SND_ELEM_PLAYBACK2_PATH          "OUT2 Switch"

/* Capture MIC Path */
#define     SND_NUMID_CAPUTRE_MIC_PATH       2
#define     SND_ELEM_CAPUTRE_MIC_PATH        "Capture Digital"
#define     SND_IN_CARD_MIC_OFF              "0"  /* close capture path */
#define     SND_IN_CARD_MAIN_MIC             "1"  /* main mic */
#define     SND_IN_CARD_HANDS_FREE_MIC       "2"  /* hands free mic */
#define     SND_IN_CARD_BT_SCO_MIC           "3"  /* bluetooth sco mic (Don't set!!!) */

/* Capture Left PGA Mux */
#define     SND_NUMID_CAPUTRE_LEFT_PGA_MUX       45
#define     SND_ELEM_CAPUTRE_LEFT_PGA_MUX        "Left PGA Mux"
#define     SND_IN_LINE_1L                        "0"   //Main Mic
#define     SND_IN_LINE_2L                        "1"   //Headset Mic
#define     SND_IN_LINE_DifferentialL             "2"

/* Capture Right PGA Mux */
#define     SND_NUMID_CAPUTRE_RIGHT_PGA_MUX       46
#define     SND_ELEM_CAPUTRE_RIGHT_PGA_MUX        "Right PGA Mux"
#define     SND_IN_LINE_1R                        "0"   //Main Mic
#define     SND_IN_LINE_2R                        "1"   //Headset Mic
#define     SND_IN_LINE_DifferentialR             "2"

/* DACL Playback Volume  */
#define     SND_NUMID_DACL_PLAYBACK_VOL      34
#define     SND_ELEM_DACL_PLAYBACK_VOL       "Output 1 Playback Volume"

/* DACR Playback Volume  */
#define     SND_NUMID_DACR_PLAYBACK_VOL      35
#define     SND_ELEM_DACR_PLAYBACK_VOL       "Output 2 Playback Volume"

/* DACL Capture Volume  */
#define     SND_NUMID_DACL_CAPTURE_VOL       27
#define     SND_ELEM_DACL_CAPTURE_VOL        "Left Channel Capture Volume"

#define     SND_CAPTURE_VOLUME                     "8"

/* DACR Capture Volume  */
#define     SND_NUMID_DACR_CAPTURE_VOL       28
#define     SND_ELEM_DACR_CAPTURE_VOL        "Right Channel Capture Volume"

#define     HP_STAYUS                           "/sys/devices/platform/es8388-sound/hp_det_gpio_status"

#define     SND_NUMID_SPK_SWITCH                39
#define     SND_ELEM_SPK_SWITCH                 "spk switch"
#define     HP_SWITCH_STATUS_ON                 "on"
#define     HP_SWITCH_STATUS_OFF                "off"

#endif /* ALSA_SND_COMMON_H */
