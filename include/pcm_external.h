/**
 * \file include/pcm_external.h
 * \brief External PCM plugin SDK
 * \author Takashi Iwai <tiwai@suse.de>
 * \date 2005
 *
 * Extern PCM plugin SDK.
 */

/*
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#ifndef __ALSA_PCM_EXTERNAL_H
#define __ALSA_PCM_EXTERNAL_H

#ifndef __ASOUNDLIB_LOCAL
#include <alsa/asoundlib.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  \defgroup Plugin_SDK External PCM plugin SDK
 *  \{
 */

/**
 * Define the object entry for external PCM plugins
 */
#define SND_PCM_PLUGIN_ENTRY(name) _snd_pcm_##name##_open

/**
 * Define the symbols of the given plugin with versions
 */
#define SND_PCM_PLUGIN_SYMBOL(name) SND_DLSYM_BUILD_VERSION(SND_PCM_PLUGIN_ENTRY(name), SND_PCM_DLSYM_VERSION);

/**
 * Define the plugin
 */
#define SND_PCM_PLUGIN_DEFINE_FUNC(plugin) \
int SND_PCM_PLUGIN_ENTRY(plugin) (snd_pcm_t **pcmp, const char *name,\
				  snd_config_t *root, snd_config_t *conf, \
				  snd_pcm_stream_t stream, int mode)

#include "pcm_ioplug.h"
#include "pcm_extplug.h"

int snd_pcm_parse_control_id(snd_config_t *conf, snd_ctl_elem_id_t *ctl_id, int *cardp,
			     int *cchannelsp, int *hwctlp) __attribute__((deprecated));

/** \} */

#ifdef __cplusplus
}
#endif

#endif /* __ALSA_PCM_EXTERNAL_H */
