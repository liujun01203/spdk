/*-
 *   BSD LICENSE
 *
 *   Copyright (c) Intel Corporation.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "transport.h"

#include <stdlib.h>
#include <strings.h>

#include "spdk/log.h"
#include "spdk/queue.h"

static const struct spdk_nvmf_transport *const g_transports[] = {
#ifdef SPDK_CONFIG_RDMA
	&spdk_nvmf_transport_rdma,
#endif
};

#define NUM_TRANSPORTS (sizeof(g_transports) / sizeof(*g_transports))

int
spdk_nvmf_transport_init(void)
{
	size_t i;
	int count = 0;

	for (i = 0; i != NUM_TRANSPORTS; i++) {
		if (g_transports[i]->transport_init() < 0) {
			SPDK_NOTICELOG("%s transport init failed\n", g_transports[i]->name);
		} else {
			count++;
		}
	}

	return count;
}

int
spdk_nvmf_transport_fini(void)
{
	size_t i;
	int count = 0;

	for (i = 0; i != NUM_TRANSPORTS; i++) {
		if (g_transports[i]->transport_fini() < 0) {
			SPDK_NOTICELOG("%s transport fini failed\n", g_transports[i]->name);
		} else {
			count++;
		}
	}

	return count;
}

int
spdk_nvmf_acceptor_start(void)
{
	size_t i;

	for (i = 0; i != NUM_TRANSPORTS; i++) {
		if (g_transports[i]->transport_start() < 0) {
			return -1;
		}
	}

	return 0;
}

void
spdk_nvmf_acceptor_stop(void)
{
	size_t i;

	for (i = 0; i != NUM_TRANSPORTS; i++) {
		g_transports[i]->transport_stop();
	}
}

const struct spdk_nvmf_transport *
spdk_nvmf_transport_get(const char *name)
{
	size_t i;

	for (i = 0; i != NUM_TRANSPORTS; i++) {
		if (strcasecmp(name, g_transports[i]->name) == 0) {
			return g_transports[i];
		}
	}

	return NULL;
}
