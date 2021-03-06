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

#include <arpa/inet.h>
#include <string.h>

#include "session.h"
#include "nvmf_internal.h"
#include "subsystem.h"
#include "transport.h"
#include "spdk/log.h"
#include "spdk/trace.h"
#include "spdk/nvme_spec.h"

static void
nvmf_init_discovery_session_properties(struct nvmf_session *session)
{
	struct spdk_nvmf_extended_identify_ctrlr_data *nvmfdata;

	session->vcdata.maxcmd = SPDK_NVMF_DEFAULT_MAX_QUEUE_DEPTH;
	/* extended data for get log page supportted */
	session->vcdata.lpa.edlp = 1;
	session->vcdata.cntlid = 0; /* There is one controller per subsystem, so its id is 0 */
	nvmfdata = (struct spdk_nvmf_extended_identify_ctrlr_data *)session->vcdata.nvmf_specific;
	nvmfdata->ioccsz = (NVMF_H2C_MAX_MSG / 16);
	nvmfdata->iorcsz = (NVMF_C2H_MAX_MSG / 16);
	nvmfdata->icdoff = 0; /* offset starts directly after SQE */
	nvmfdata->ctrattr = 0; /* dynamic controller model */
	nvmfdata->msdbd = 1; /* target supports single SGL in capsule */
	session->vcdata.sgls.keyed_sgl = 1;
	session->vcdata.sgls.sgl_offset = 1;

	/* Properties */
	session->vcprop.cap.raw = 0;
	session->vcprop.cap.bits.cqr = 1;	/* NVMF specification required */
	session->vcprop.cap.bits.mqes = (session->vcdata.maxcmd - 1);	/* max queue depth */
	session->vcprop.cap.bits.ams = 0;	/* optional arb mechanisms */
	session->vcprop.cap.bits.dstrd = 0;	/* fixed to 0 for NVMf */
	session->vcprop.cap.bits.css_nvm = 1; /* NVM command set */
	session->vcprop.cap.bits.mpsmin = 0; /* 2 ^ 12 + mpsmin == 4k */
	session->vcprop.cap.bits.mpsmax = 0; /* 2 ^ 12 + mpsmax == 4k */

	/* Version Supported: 1.0 */
	session->vcprop.vs.bits.mjr = 1;
	session->vcprop.vs.bits.mnr = 0;
	session->vcprop.vs.bits.ter = 0;

	session->vcprop.cc.raw = 0;

	session->vcprop.csts.raw = 0;
	session->vcprop.csts.bits.rdy = 0; /* Init controller as not ready */
}

static void
nvmf_init_nvme_session_properties(struct nvmf_session *session)
{
	const struct spdk_nvme_ctrlr_data	*cdata;
	struct spdk_nvmf_extended_identify_ctrlr_data *nvmfdata;

	/*
	  Here we are going to initialize the features, properties, and
	  identify controller details for the virtual controller associated
	  with a specific subsystem session.
	*/

	/* Init the virtual controller details using actual HW details */
	cdata = spdk_nvme_ctrlr_get_data(session->subsys->ctrlr);
	memcpy(&session->vcdata, cdata, sizeof(struct spdk_nvme_ctrlr_data));

	session->vcdata.aerl = 0;
	session->vcdata.cntlid = 0;
	session->vcdata.kas = 10;
	session->vcdata.maxcmd = SPDK_NVMF_DEFAULT_MAX_QUEUE_DEPTH;
	session->vcdata.mdts = SPDK_NVMF_MAX_RECV_DATA_TRANSFER_SIZE / 4096;
	session->vcdata.sgls.keyed_sgl = 1;
	session->vcdata.sgls.sgl_offset = 1;

	nvmfdata = (struct spdk_nvmf_extended_identify_ctrlr_data *)session->vcdata.nvmf_specific;
	nvmfdata->ioccsz = (NVMF_H2C_MAX_MSG / 16);
	nvmfdata->iorcsz = (NVMF_C2H_MAX_MSG / 16);
	nvmfdata->icdoff = 0; /* offset starts directly after SQE */
	nvmfdata->ctrattr = 0; /* dynamic controller model */
	nvmfdata->msdbd = 1; /* target supports single SGL in capsule */

	SPDK_TRACELOG(SPDK_TRACE_NVMF, "	ctrlr data: maxcmd %x\n",
		      session->vcdata.maxcmd);
	SPDK_TRACELOG(SPDK_TRACE_NVMF, "	ext ctrlr data: ioccsz %x\n",
		      nvmfdata->ioccsz);
	SPDK_TRACELOG(SPDK_TRACE_NVMF, "	ext ctrlr data: iorcsz %x\n",
		      nvmfdata->iorcsz);
	SPDK_TRACELOG(SPDK_TRACE_NVMF, "	ext ctrlr data: icdoff %x\n",
		      nvmfdata->icdoff);
	SPDK_TRACELOG(SPDK_TRACE_NVMF, "	ext ctrlr data: ctrattr %x\n",
		      nvmfdata->ctrattr);
	SPDK_TRACELOG(SPDK_TRACE_NVMF, "	ext ctrlr data: msdbd %x\n",
		      nvmfdata->msdbd);
	SPDK_TRACELOG(SPDK_TRACE_NVMF, "	sgls data: 0x%x\n",
		      *(uint32_t *)&session->vcdata.sgls);

	session->vcprop.cap.raw = 0;
	session->vcprop.cap.bits.cqr = 0;	/* queues not contiguous */
	session->vcprop.cap.bits.mqes = (session->vcdata.maxcmd - 1);	/* max queue depth */
	session->vcprop.cap.bits.ams = 0;	/* optional arb mechanisms */
	session->vcprop.cap.bits.to = 1;	/* ready timeout - 500 msec units */
	session->vcprop.cap.bits.dstrd = 0;	/* fixed to 0 for NVMf */
	session->vcprop.cap.bits.css_nvm = 1; /* NVM command set */
	session->vcprop.cap.bits.mpsmin = 0; /* 2 ^ 12 + mpsmin == 4k */
	session->vcprop.cap.bits.mpsmax = 0; /* 2 ^ 12 + mpsmax == 4k */

	/* Version Supported: 1.0 */
	session->vcprop.vs.bits.mjr = 1;
	session->vcprop.vs.bits.mnr = 0;
	session->vcprop.vs.bits.ter = 0;

	session->vcprop.cc.raw = 0;
	session->vcprop.cc.bits.en = 0; /* Init controller disabled */

	session->vcprop.csts.raw = 0;
	session->vcprop.csts.bits.rdy = 0; /* Init controller as not ready */

	SPDK_TRACELOG(SPDK_TRACE_NVMF, "	cap %" PRIx64 "\n",
		      session->vcprop.cap.raw);
	SPDK_TRACELOG(SPDK_TRACE_NVMF, "	vs %x\n", session->vcprop.vs.raw);
	SPDK_TRACELOG(SPDK_TRACE_NVMF, "	cc %x\n", session->vcprop.cc.raw);
	SPDK_TRACELOG(SPDK_TRACE_NVMF, "	csts %x\n",
		      session->vcprop.csts.raw);
}

void
spdk_nvmf_session_destruct(struct nvmf_session *session)
{
	session->subsys->session = NULL;

	while (!TAILQ_EMPTY(&session->connections)) {
		struct spdk_nvmf_conn *conn = TAILQ_FIRST(&session->connections);

		TAILQ_REMOVE(&session->connections, conn, link);
		nvmf_disconnect(conn->sess, conn);
		conn->transport->conn_fini(conn);
	}

	free(session);
}

static void
invalid_connect_response(struct spdk_nvmf_fabric_connect_rsp *rsp, uint8_t iattr, uint16_t ipo)
{
	rsp->status.sct = SPDK_NVME_SCT_COMMAND_SPECIFIC;
	rsp->status.sc = SPDK_NVMF_FABRIC_SC_INVALID_PARAM;
	rsp->status_code_specific.invalid.iattr = iattr;
	rsp->status_code_specific.invalid.ipo = ipo;
}

void
spdk_nvmf_session_connect(struct spdk_nvmf_conn *conn,
			  struct spdk_nvmf_fabric_connect_cmd *cmd,
			  struct spdk_nvmf_fabric_connect_data *data,
			  struct spdk_nvmf_fabric_connect_rsp *rsp)
{
	struct nvmf_session *session;
	struct spdk_nvmf_subsystem *subsystem;

#define INVALID_CONNECT_CMD(field) invalid_connect_response(rsp, 0, offsetof(struct spdk_nvmf_fabric_connect_cmd, field))
#define INVALID_CONNECT_DATA(field) invalid_connect_response(rsp, 1, offsetof(struct spdk_nvmf_fabric_connect_data, field))

	SPDK_TRACELOG(SPDK_TRACE_NVMF, "recfmt 0x%x qid %u sqsize %u\n",
		      cmd->recfmt, cmd->qid, cmd->sqsize);

	SPDK_TRACELOG(SPDK_TRACE_NVMF, "Connect data:\n");
	SPDK_TRACELOG(SPDK_TRACE_NVMF, "  cntlid:  0x%04x\n", data->cntlid);
	SPDK_TRACELOG(SPDK_TRACE_NVMF, "  hostid: %08x-%04x-%04x-%02x%02x-%04x%08x ***\n",
		      ntohl(*(uint32_t *)&data->hostid[0]),
		      ntohs(*(uint16_t *)&data->hostid[4]),
		      ntohs(*(uint16_t *)&data->hostid[6]),
		      data->hostid[8],
		      data->hostid[9],
		      ntohs(*(uint16_t *)&data->hostid[10]),
		      ntohl(*(uint32_t *)&data->hostid[12]));
	SPDK_TRACELOG(SPDK_TRACE_NVMF, "  subnqn: \"%s\"\n", data->subnqn);
	SPDK_TRACELOG(SPDK_TRACE_NVMF, "  hostnqn: \"%s\"\n", data->hostnqn);

	subsystem = nvmf_find_subsystem(data->subnqn, data->hostnqn);
	if (subsystem == NULL) {
		SPDK_ERRLOG("Could not find subsystem '%s'\n", data->subnqn);
		INVALID_CONNECT_DATA(subnqn);
		return;
	}

	if (cmd->qid == 0) {
		conn->type = CONN_TYPE_AQ;

		SPDK_TRACELOG(SPDK_TRACE_NVMF, "Connect Admin Queue for controller ID 0x%x\n", data->cntlid);

		if (data->cntlid != 0xFFFF) {
			/* This NVMf target only supports dynamic mode. */
			SPDK_ERRLOG("The NVMf target only supports dynamic mode (CNTLID = 0x%x).\n", data->cntlid);
			INVALID_CONNECT_DATA(cntlid);
			return;
		}

		if (subsystem->session) {
			SPDK_ERRLOG("Cannot connect to already-connected controller\n");
			rsp->status.sct = SPDK_NVME_SCT_COMMAND_SPECIFIC;
			rsp->status.sc = SPDK_NVMF_FABRIC_SC_CONTROLLER_BUSY;
			return;
		}

		/* Establish a new session */
		subsystem->session = session = calloc(1, sizeof(struct nvmf_session));
		if (session == NULL) {
			SPDK_ERRLOG("Memory allocation failure\n");
			rsp->status.sc = SPDK_NVME_SC_INTERNAL_DEVICE_ERROR;
			return;
		}

		TAILQ_INIT(&session->connections);
		session->num_connections = 0;
		session->subsys = subsystem;
		session->max_connections_allowed = g_nvmf_tgt.max_queues_per_session;

		if (subsystem->subtype == SPDK_NVMF_SUBTYPE_NVME) {
			nvmf_init_nvme_session_properties(session);
		} else {
			nvmf_init_discovery_session_properties(session);
		}
	} else {
		conn->type = CONN_TYPE_IOQ;
		SPDK_TRACELOG(SPDK_TRACE_NVMF, "Connect I/O Queue for controller id 0x%x\n", data->cntlid);

		/* We always return CNTLID 0, so verify that the I/O connect CNTLID matches */
		if (data->cntlid != 0) {
			SPDK_ERRLOG("Unknown controller ID 0x%x\n", data->cntlid);
			INVALID_CONNECT_DATA(cntlid);
			return;
		}

		session = subsystem->session;
		if (session == NULL || !session->vcprop.cc.bits.en) {
			SPDK_ERRLOG("Got I/O connect before ctrlr was enabled\n");
			INVALID_CONNECT_CMD(qid);
			return;
		}

		if (1u << session->vcprop.cc.bits.iosqes != sizeof(struct spdk_nvme_cmd)) {
			SPDK_ERRLOG("Got I/O connect with invalid IOSQES %u\n",
				    session->vcprop.cc.bits.iosqes);
			INVALID_CONNECT_CMD(qid);
			return;
		}

		if (1u << session->vcprop.cc.bits.iocqes != sizeof(struct spdk_nvme_cpl)) {
			SPDK_ERRLOG("Got I/O connect with invalid IOCQES %u\n",
				    session->vcprop.cc.bits.iocqes);
			INVALID_CONNECT_CMD(qid);
			return;
		}

		/* check if we would exceed session connection limit */
		if (session->num_connections >= session->max_connections_allowed) {
			SPDK_ERRLOG("connection limit %d\n", session->num_connections);
			rsp->status.sct = SPDK_NVME_SCT_COMMAND_SPECIFIC;
			rsp->status.sc = SPDK_NVMF_FABRIC_SC_CONTROLLER_BUSY;
			return;
		}
	}

	session->num_connections++;
	TAILQ_INSERT_HEAD(&session->connections, conn, link);
	conn->sess = session;

	rsp->status.sc = SPDK_NVME_SC_SUCCESS;
	rsp->status_code_specific.success.cntlid = 0;
	SPDK_TRACELOG(SPDK_TRACE_NVMF, "connect capsule response: cntlid = 0x%04x\n",
		      rsp->status_code_specific.success.cntlid);
}

void
nvmf_disconnect(struct nvmf_session *session,
		struct spdk_nvmf_conn *conn)
{
	session->num_connections--;
	TAILQ_REMOVE(&session->connections, conn, link);
}

static uint64_t
nvmf_prop_get_cap(struct nvmf_session *session)
{
	return session->vcprop.cap.raw;
}

static uint64_t
nvmf_prop_get_vs(struct nvmf_session *session)
{
	return session->vcprop.vs.raw;
}

static uint64_t
nvmf_prop_get_cc(struct nvmf_session *session)
{
	return session->vcprop.cc.raw;
}

static bool
nvmf_prop_set_cc(struct nvmf_session *session, uint64_t value)
{
	union spdk_nvme_cc_register cc, diff;

	cc.raw = (uint32_t)value;

	SPDK_TRACELOG(SPDK_TRACE_NVMF, "cur CC: 0x%08x\n", session->vcprop.cc.raw);
	SPDK_TRACELOG(SPDK_TRACE_NVMF, "new CC: 0x%08x\n", cc.raw);

	/*
	 * Calculate which bits changed between the current and new CC.
	 * Mark each bit as 0 once it is handled to determine if any unhandled bits were changed.
	 */
	diff.raw = cc.raw ^ session->vcprop.cc.raw;

	if (diff.bits.en) {
		if (cc.bits.en) {
			SPDK_TRACELOG(SPDK_TRACE_NVMF, "Property Set CC Enable!\n");
			session->vcprop.cc.bits.en = 1;
			session->vcprop.csts.bits.rdy = 1;
		} else {
			SPDK_ERRLOG("CC.EN transition from 1 to 0 (reset) not implemented!\n");
			/* TODO: reset */
		}
		diff.bits.en = 0;
	}

	if (diff.bits.shn) {
		if (cc.bits.shn == SPDK_NVME_SHN_NORMAL ||
		    cc.bits.shn == SPDK_NVME_SHN_ABRUPT) {
			SPDK_TRACELOG(SPDK_TRACE_NVMF, "Property Set CC Shutdown %u%ub!\n",
				      cc.bits.shn >> 1, cc.bits.shn & 1);
			session->vcprop.cc.bits.shn = cc.bits.shn;
			session->vcprop.cc.bits.en = 0;
			session->vcprop.csts.bits.rdy = 0;
			session->vcprop.csts.bits.shst = SPDK_NVME_SHST_COMPLETE;
		} else if (cc.bits.shn == 0) {
			session->vcprop.cc.bits.shn = 0;
		} else {
			SPDK_ERRLOG("Prop Set CC: Invalid SHN value %u%ub\n",
				    cc.bits.shn >> 1, cc.bits.shn & 1);
			return false;
		}
		diff.bits.shn = 0;
	}

	if (diff.bits.iosqes) {
		SPDK_TRACELOG(SPDK_TRACE_NVMF, "Prop Set IOSQES = %u (%u bytes)\n",
			      cc.bits.iosqes, 1u << cc.bits.iosqes);
		session->vcprop.cc.bits.iosqes = cc.bits.iosqes;
		diff.bits.iosqes = 0;
	}

	if (diff.bits.iocqes) {
		SPDK_TRACELOG(SPDK_TRACE_NVMF, "Prop Set IOCQES = %u (%u bytes)\n",
			      cc.bits.iocqes, 1u << cc.bits.iocqes);
		session->vcprop.cc.bits.iocqes = cc.bits.iocqes;
		diff.bits.iocqes = 0;
	}

	if (diff.raw != 0) {
		SPDK_ERRLOG("Prop Set CC toggled reserved bits 0x%x!\n", diff.raw);
		return false;
	}

	return true;
}

static uint64_t
nvmf_prop_get_csts(struct nvmf_session *session)
{
	return session->vcprop.csts.raw;
}

struct nvmf_prop {
	uint32_t ofst;
	uint8_t size;
	char name[11];
	uint64_t (*get_cb)(struct nvmf_session *session);
	bool (*set_cb)(struct nvmf_session *session, uint64_t value);
};

#define PROP(field, size, get_cb, set_cb) \
	{ \
		offsetof(struct spdk_nvme_registers, field), \
		SPDK_NVMF_PROP_SIZE_##size, \
		#field, \
		get_cb, set_cb \
	}

static const struct nvmf_prop nvmf_props[] = {
	PROP(cap,  8, nvmf_prop_get_cap,  NULL),
	PROP(vs,   4, nvmf_prop_get_vs,   NULL),
	PROP(cc,   4, nvmf_prop_get_cc,   nvmf_prop_set_cc),
	PROP(csts, 4, nvmf_prop_get_csts, NULL),
};

static const struct nvmf_prop *
find_prop(uint32_t ofst)
{
	size_t i;

	for (i = 0; i < sizeof(nvmf_props) / sizeof(*nvmf_props); i++) {
		const struct nvmf_prop *prop = &nvmf_props[i];

		if (prop->ofst == ofst) {
			return prop;
		}
	}

	return NULL;
}

void
nvmf_property_get(struct nvmf_session *session,
		  struct spdk_nvmf_fabric_prop_get_cmd *cmd,
		  struct spdk_nvmf_fabric_prop_get_rsp *response)
{
	const struct nvmf_prop *prop;

	response->status.sc = 0;
	response->value.u64 = 0;

	SPDK_TRACELOG(SPDK_TRACE_NVMF, "size %d, offset 0x%x\n",
		      cmd->attrib.size, cmd->ofst);

	if (cmd->attrib.size != SPDK_NVMF_PROP_SIZE_4 &&
	    cmd->attrib.size != SPDK_NVMF_PROP_SIZE_8) {
		SPDK_ERRLOG("Invalid size value %d\n", cmd->attrib.size);
		response->status.sc = SPDK_NVMF_FABRIC_SC_INVALID_PARAM;
		return;
	}

	prop = find_prop(cmd->ofst);
	if (prop == NULL || prop->get_cb == NULL) {
		/* Reserved properties return 0 when read */
		return;
	}

	SPDK_TRACELOG(SPDK_TRACE_NVMF, "name: %s\n", prop->name);
	if (cmd->attrib.size != prop->size) {
		SPDK_ERRLOG("offset 0x%x size mismatch: cmd %u, prop %u\n",
			    cmd->ofst, cmd->attrib.size, prop->size);
		response->status.sc = SPDK_NVMF_FABRIC_SC_INVALID_PARAM;
		return;
	}

	response->value.u64 = prop->get_cb(session);
	SPDK_TRACELOG(SPDK_TRACE_NVMF, "response value: 0x%" PRIx64 "\n", response->value.u64);
}

void
nvmf_property_set(struct nvmf_session *session,
		  struct spdk_nvmf_fabric_prop_set_cmd *cmd,
		  struct spdk_nvme_cpl *response)
{
	const struct nvmf_prop *prop;
	uint64_t value;

	SPDK_TRACELOG(SPDK_TRACE_NVMF, "size %d, offset 0x%x, value 0x%" PRIx64 "\n",
		      cmd->attrib.size, cmd->ofst, cmd->value.u64);

	prop = find_prop(cmd->ofst);
	if (prop == NULL || prop->set_cb == NULL) {
		SPDK_ERRLOG("Invalid offset 0x%x\n", cmd->ofst);
		response->status.sc = SPDK_NVMF_FABRIC_SC_INVALID_PARAM;
		return;
	}

	SPDK_TRACELOG(SPDK_TRACE_NVMF, "name: %s\n", prop->name);
	if (cmd->attrib.size != prop->size) {
		SPDK_ERRLOG("offset 0x%x size mismatch: cmd %u, prop %u\n",
			    cmd->ofst, cmd->attrib.size, prop->size);
		response->status.sc = SPDK_NVMF_FABRIC_SC_INVALID_PARAM;
		return;
	}

	value = cmd->value.u64;
	if (prop->size == SPDK_NVMF_PROP_SIZE_4) {
		value = (uint32_t)value;
	}

	if (!prop->set_cb(session, value)) {
		SPDK_ERRLOG("prop set_cb failed\n");
		response->status.sc = SPDK_NVMF_FABRIC_SC_INVALID_PARAM;
		return;
	}
}

int
spdk_nvmf_session_poll(struct nvmf_session *session)
{
	struct spdk_nvmf_conn	*conn, *tmp;

	TAILQ_FOREACH_SAFE(conn, &session->connections, link, tmp) {
		if (conn->transport->conn_poll(conn) < 0) {
			SPDK_ERRLOG("Transport poll failed for conn %p; closing connection\n", conn);
			nvmf_disconnect(session, conn);
		}
	}

	return 0;
}
