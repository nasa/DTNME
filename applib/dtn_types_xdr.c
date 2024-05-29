/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#include "dtn_types.h"
/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
*/

/**********************************
 * This file defines the types used in the DTN client API. The structures are
 * autogenerated using rpcgen, as are the marshalling and unmarshalling
 * XDR routines.
 */

#include <limits.h>
#ifndef ARG_MAX
#define ARG_MAX _POSIX_ARG_MAX
#endif

/**********************************
 * Constants.
 * (Note that we use #defines to get the comments as well)
 */
#define DTN_MAX_ENDPOINT_ID 64 /* max endpoint_id size (bytes) - was 256 */
#define DTN_MAX_PATH_LEN PATH_MAX /* max path length */
//#define DTN_MAX_EXEC_LEN ARG_MAX	/* length of string passed to exec() */
#define DTN_MAX_EXEC_LEN 1050000 /* length of string passed to exec() */
#define DTN_MAX_AUTHDATA 1024 /* length of auth/security data*/
#define DTN_MAX_REGION_LEN 64 /* 64 chars "should" be long enough */
#define DTN_MAX_BUNDLE_MEM 100000000 /* biggest in-memory bundle is 100MB */
#define DTN_MAX_BLOCK_LEN 1024 /* length of block data (currently 1K) */
#define DTN_MAX_BLOCKS 256 /* number of blocks in bundle */

/**
 * Specification of a dtn endpoint id, i.e. a URI, implemented as a
 * fixed-length char buffer. Note that for efficiency reasons, this
 * fixed length is relatively small (256 bytes). 
 * 
 * The alternative is to use the string XDR type but then all endpoint
 * ids would require malloc / free which is more prone to leaks / bugs.
 */

bool_t
xdr_dtn_endpoint_id_t (XDR *xdrs, dtn_endpoint_id_t *objp)
{
	register int32_t *buf;

	int i;
	 if (!xdr_opaque (xdrs, objp->uri, DTN_MAX_ENDPOINT_ID))
		 return FALSE;
	return TRUE;
}

/**
 * A registration cookie.
 */

bool_t
xdr_dtn_reg_id_t (XDR *xdrs, dtn_reg_id_t *objp)
{
	register int32_t *buf;

	 if (!xdr_u_int (xdrs, objp))
		 return FALSE;
	return TRUE;
}

/**
 * DTN timeouts are specified in seconds.
 * typedef u_int dtn_timeval_t; // 32-bit systems
 * typedef u_int32_t dtn_timeval_t; // 64-bit systems
 */

bool_t
xdr_dtn_timeval_t (XDR *xdrs, dtn_timeval_t *objp)
{
	register int32_t *buf;

	 if (!xdr_u_int (xdrs, objp))
		 return FALSE;
	return TRUE;
}

/**
 * An infinite wait is a timeout of -1.
 */
#define DTN_TIMEOUT_INF ((dtn_timeval_t)-1)

#include "dtn-config.h"

#ifdef HAVE_XDR_U_QUAD_T
 #define u_hyper u_quad_t
 #define xdr_u_hyper_t xdr_u_quad_t
#elif defined(HAVE_XDR_U_INT64_T)
 #define u_hyper u_int64_t
 #define xdr_u_hyper_t xdr_u_int64_t
#else
 #define u_hyper u_quad_t
 #define xdr_u_hyper_t xdr_u_quad_t
#endif
/**
 * BPv6 uses seconds and BPv7 uses milliseconds
 */

bool_t
xdr_dtn_timestamp_t (XDR *xdrs, dtn_timestamp_t *objp)
{
	register int32_t *buf;

	 if (!xdr_u_hyper (xdrs, &objp->secs_or_millisecs))
		 return FALSE;
	 if (!xdr_u_hyper (xdrs, &objp->seqno))
		 return FALSE;
	return TRUE;
}

/**
 * Specification of a service tag used in building a local endpoint
 * identifier.
 * 
 * Note that the application cannot (in general) expect to be able to
 * use the full DTN_MAX_ENDPOINT_ID, as there is a chance of overflow
 * when the daemon concats the tag with its own local endpoint id.
 */

bool_t
xdr_dtn_service_tag_t (XDR *xdrs, dtn_service_tag_t *objp)
{
	register int32_t *buf;

	int i;
	 if (!xdr_vector (xdrs, (char *)objp->tag, DTN_MAX_ENDPOINT_ID,
		sizeof (char), (xdrproc_t) xdr_char))
		 return FALSE;
	return TRUE;
}

/**
 * Value for an unspecified registration cookie (i.e. indication that
 * the daemon should allocate a new unique id).
 */

/**
 * Registration flags are a bitmask of the following:

 * Delivery failure actions (exactly one must be selected):
 *     DTN_REG_DROP   - drop bundle if registration not active
 *     DTN_REG_DEFER  - spool bundle for later retrieval
 *     DTN_REG_EXEC   - exec program on bundle arrival
 *
 * Session flags:
 *     DTN_SESSION_CUSTODY   - app assumes custody for the session
 *     DTN_SESSION_PUBLISH   - creates a publication point
 *     DTN_SESSION_SUBSCRIBE - create subscription for the session
 *
 * Other flags:
 *     DTN_DELIVERY_ACKS - application will acknowledge delivered
 *                         bundles with dtn_ack()
 *
 */

bool_t
xdr_dtn_reg_flags_t (XDR *xdrs, dtn_reg_flags_t *objp)
{
	register int32_t *buf;

	 if (!xdr_enum (xdrs, (enum_t *) objp))
		 return FALSE;
	return TRUE;
}

/**
 * Replay flags - behavior when apps bind to an existing registration
 *     DTN_REPLAY_NEW  - [default] deliver new bundles rcvd on reg while not bound
 *     DTN_REPLAY_NONE - don't deliver new bundles rcvd on reg while not bound
 *     DTN_REPLAY_ALL  - deliver _all_ bundles queued up on the reg
 *                       (makes duplicate bundle deliveries possible)
 */

bool_t
xdr_dtn_replay_flags_t (XDR *xdrs, dtn_replay_flags_t *objp)
{
	register int32_t *buf;

	 if (!xdr_enum (xdrs, (enum_t *) objp))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_dtn_reg_token_t (XDR *xdrs, dtn_reg_token_t *objp)
{
	register int32_t *buf;

	 if (!xdr_u_hyper (xdrs, objp))
		 return FALSE;
	return TRUE;
}

/**
 * Registration state.
 */

bool_t
xdr_dtn_reg_info_t (XDR *xdrs, dtn_reg_info_t *objp)
{
	register int32_t *buf;

	 if (!xdr_dtn_endpoint_id_t (xdrs, &objp->endpoint))
		 return FALSE;
	 if (!xdr_dtn_reg_id_t (xdrs, &objp->regid))
		 return FALSE;
	 if (!xdr_u_int (xdrs, &objp->flags))
		 return FALSE;
	 if (!xdr_u_int (xdrs, &objp->replay_flags))
		 return FALSE;
	 if (!xdr_dtn_timeval_t (xdrs, &objp->expiration))
		 return FALSE;
	 if (!xdr_bool (xdrs, &objp->init_passive))
		 return FALSE;
	 if (!xdr_dtn_reg_token_t (xdrs, &objp->reg_token))
		 return FALSE;
	 if (!xdr_bytes (xdrs, (char **)&objp->script.script_val, (u_int *) &objp->script.script_len, DTN_MAX_EXEC_LEN))
		 return FALSE;
	return TRUE;
}

/**
 * Bundle priority specifier.
 *     COS_BULK      - lowest priority
 *     COS_NORMAL    - regular priority
 *     COS_EXPEDITED - important
 *     COS_RESERVED  - TBD
 */

bool_t
xdr_dtn_bundle_priority_t (XDR *xdrs, dtn_bundle_priority_t *objp)
{
	register int32_t *buf;

	 if (!xdr_enum (xdrs, (enum_t *) objp))
		 return FALSE;
	return TRUE;
}

/**
 * Bundle delivery option flags. Note that multiple options may be
 * selected for a given bundle.
 *     
 *     DOPTS_NONE            - no custody, etc
 *     DOPTS_CUSTODY         - custody xfer
 *     DOPTS_DELIVERY_RCPT   - end to end delivery (i.e. return receipt)
 *     DOPTS_RECEIVE_RCPT    - per hop arrival receipt
 *     DOPTS_FORWARD_RCPT    - per hop departure receipt
 *     DOPTS_CUSTODY_RCPT    - per custodian receipt
 *     DOPTS_DELETE_RCPT     - request deletion receipt
 *     DOPTS_SINGLETON_DEST  - destination is a singleton
 *     DOPTS_MULTINODE_DEST  - destination is not a singleton
 *     DOPTS_DO_NOT_FRAGMENT - set the do not fragment bit
 */

bool_t
xdr_dtn_bundle_delivery_opts_t (XDR *xdrs, dtn_bundle_delivery_opts_t *objp)
{
	register int32_t *buf;

	 if (!xdr_enum (xdrs, (enum_t *) objp))
		 return FALSE;
	return TRUE;
}

/**
 * Extension block flags. Note that multiple flags may be selected
 * for a given block.
 *
 *     BLOCK_FLAG_NONE          - no flags
 *     BLOCK_FLAG_REPLICATE     - block must be replicated in every fragment
 *     BLOCK_FLAG_REPORT        - transmit report if block can't be processed
 *     BLOCK_FLAG_DELETE_BUNDLE - delete bundle if block can't be processed
 *     BLOCK_FLAG_LAST          - last block
 *     BLOCK_FLAG_DISCARD_BLOCK - discard block if it can't be processed
 *     BLOCK_FLAG_UNPROCESSED   - block was forwarded without being processed
 */

bool_t
xdr_dtn_extension_block_flags_t (XDR *xdrs, dtn_extension_block_flags_t *objp)
{
	register int32_t *buf;

	 if (!xdr_enum (xdrs, (enum_t *) objp))
		 return FALSE;
	return TRUE;
}

/**
 *	   BPQ extension block type
 */

#define DTN_BPQ_BLOCK_TYPE 0x0B

/**
 * BPQ Extension block kind.
 *
 *     BPQ_BLOCK_KIND_QUERY				- query bundles
 *     BPQ_BLOCK_KIND_RESPONSE				- response bundles
 *     BPQ_BLOCK_KIND_RESPONSE_DO_NOT_CACHE_FRAG	- response bundles that should not be cached unless complete
 *     BPQ_BLOCK_KIND_PUBLISH				- publish bundles - treated like response except local storage
 */


bool_t
xdr_dtn_bpq_extension_block_kind_t (XDR *xdrs, dtn_bpq_extension_block_kind_t *objp)
{
	register int32_t *buf;

	 if (!xdr_enum (xdrs, (enum_t *) objp))
		 return FALSE;
	return TRUE;
}
/**
 * BPQ Extension block matching rule. (More may be added later)
 *
 *     BPQ_MATCHING_RULE_EXACT
 */


bool_t
xdr_dtn_bpq_extension_block_matching_rule_t (XDR *xdrs, dtn_bpq_extension_block_matching_rule_t *objp)
{
	register int32_t *buf;

	 if (!xdr_enum (xdrs, (enum_t *) objp))
		 return FALSE;
	return TRUE;
}
/**
 * Extension block.
 */

bool_t
xdr_dtn_extension_block_data_t (XDR *xdrs, dtn_extension_block_data_t *objp)
{
	register int32_t *buf;

	 if (!xdr_u_int (xdrs, &objp->data_len))
		 return FALSE;
	 if (!xdr_pointer (xdrs, (char **)&objp->data_val, sizeof (char), (xdrproc_t) xdr_char))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_dtn_extension_block_t (XDR *xdrs, dtn_extension_block_t *objp)
{
	register int32_t *buf;

	 if (!xdr_u_int (xdrs, &objp->type))
		 return FALSE;
	 if (!xdr_u_int (xdrs, &objp->flags))
		 return FALSE;
	 if (!xdr_dtn_extension_block_data_t (xdrs, &objp->data))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_dtn_bpq_ext_block_original_id_t (XDR *xdrs, dtn_bpq_ext_block_original_id_t *objp)
{
	register int32_t *buf;

	 if (!xdr_dtn_timestamp_t (xdrs, &objp->creation_ts))
		 return FALSE;
	 if (!xdr_u_int (xdrs, &objp->source_len))
		 return FALSE;
	 if (!xdr_dtn_endpoint_id_t (xdrs, &objp->source))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_dtn_bpq_ext_block_query_t (XDR *xdrs, dtn_bpq_ext_block_query_t *objp)
{
	register int32_t *buf;

	 if (!xdr_u_int (xdrs, &objp->query_len))
		 return FALSE;
	 if (!xdr_pointer (xdrs, (char **)&objp->query_val, sizeof (char), (xdrproc_t) xdr_char))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_dtn_bpq_ext_block_fragments_t (XDR *xdrs, dtn_bpq_ext_block_fragments_t *objp)
{
	register int32_t *buf;

	 if (!xdr_u_int (xdrs, &objp->num_frag_returned))
		 return FALSE;
	 if (!xdr_pointer (xdrs, (char **)&objp->frag_offsets, sizeof (u_int), (xdrproc_t) xdr_u_int))
		 return FALSE;
	 if (!xdr_pointer (xdrs, (char **)&objp->frag_lenghts, sizeof (u_int), (xdrproc_t) xdr_u_int))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_dtn_bpq_extension_block_data_t (XDR *xdrs, dtn_bpq_extension_block_data_t *objp)
{
	register int32_t *buf;

	 if (!xdr_u_int (xdrs, &objp->kind))
		 return FALSE;
	 if (!xdr_u_int (xdrs, &objp->matching_rule))
		 return FALSE;
	 if (!xdr_dtn_bpq_ext_block_original_id_t (xdrs, &objp->original_id))
		 return FALSE;
	 if (!xdr_dtn_bpq_ext_block_query_t (xdrs, &objp->query))
		 return FALSE;
	 if (!xdr_dtn_bpq_ext_block_fragments_t (xdrs, &objp->fragments))
		 return FALSE;
	return TRUE;
}

/**
 * A Sequence ID is a vector of (EID, counter) values in the following
 * text format:
 *
 *    < (EID1 counter1) (EID2 counter2) ... >
 */

bool_t
xdr_dtn_sequence_id_t (XDR *xdrs, dtn_sequence_id_t *objp)
{
	register int32_t *buf;

	 if (!xdr_bytes (xdrs, (char **)&objp->data.data_val, (u_int *) &objp->data.data_len, DTN_MAX_BLOCK_LEN))
		 return FALSE;
	return TRUE;
}
/**
 * Extended Class of Service (ECOS) flags bit definitions.
 *
 *     ECOS_FLAG_CRITICAL         - critical - send on every logical path
 *     ECOS_FLAG_STREAMING        - streaming - send on best efforts without retransmission
 *     ECOS_FLAG_FLOW_LABEL       - indicates flow label exists after the ordinal byte
 *     ECOS_FLAG_RELIABLE         - reliable - send on convergence layer that detects data loss and retransmits
 *
 * Use these bit definitions OR-ed together to set the ecos_flags parameter.
 */


bool_t
xdr_dtn_ecos_flags_bits_t (XDR *xdrs, dtn_ecos_flags_bits_t *objp)
{
	register int32_t *buf;

	 if (!xdr_enum (xdrs, (enum_t *) objp))
		 return FALSE;
	return TRUE;
}

/**
 * Bundle metadata. The delivery_regid is ignored when sending
 * bundles, but is filled in by the daemon with the registration
 * id where the bundle was received.
 */

bool_t
xdr_dtn_bundle_spec_t (XDR *xdrs, dtn_bundle_spec_t *objp)
{
	register int32_t *buf;


	if (xdrs->x_op == XDR_ENCODE) {
		 if (!xdr_dtn_endpoint_id_t (xdrs, &objp->source))
			 return FALSE;
		 if (!xdr_dtn_endpoint_id_t (xdrs, &objp->dest))
			 return FALSE;
		 if (!xdr_dtn_endpoint_id_t (xdrs, &objp->replyto))
			 return FALSE;
		 if (!xdr_dtn_bundle_priority_t (xdrs, &objp->priority))
			 return FALSE;
		 if (!xdr_int (xdrs, &objp->dopts))
			 return FALSE;
		 if (!xdr_dtn_timeval_t (xdrs, &objp->expiration))
			 return FALSE;
		 if (!xdr_dtn_timestamp_t (xdrs, &objp->creation_ts))
			 return FALSE;
		 if (!xdr_dtn_reg_id_t (xdrs, &objp->delivery_regid))
			 return FALSE;
		 if (!xdr_dtn_sequence_id_t (xdrs, &objp->sequence_id))
			 return FALSE;
		 if (!xdr_dtn_sequence_id_t (xdrs, &objp->obsoletes_id))
			 return FALSE;
		buf = XDR_INLINE (xdrs, 5 * BYTES_PER_XDR_UNIT);
		if (buf == NULL) {
			 if (!xdr_bool (xdrs, &objp->ecos_enabled))
				 return FALSE;
			 if (!xdr_u_int (xdrs, &objp->ecos_flags))
				 return FALSE;
			 if (!xdr_u_int (xdrs, &objp->ecos_ordinal))
				 return FALSE;
			 if (!xdr_u_int (xdrs, &objp->ecos_flow_label))
				 return FALSE;
			 if (!xdr_u_int (xdrs, &objp->bp_version))
				 return FALSE;

		} else {
		IXDR_PUT_BOOL(buf, objp->ecos_enabled);
		IXDR_PUT_U_LONG(buf, objp->ecos_flags);
		IXDR_PUT_U_LONG(buf, objp->ecos_ordinal);
		IXDR_PUT_U_LONG(buf, objp->ecos_flow_label);
		IXDR_PUT_U_LONG(buf, objp->bp_version);
		}
		 if (!xdr_array (xdrs, (char **)&objp->blocks.blocks_val, (u_int *) &objp->blocks.blocks_len, DTN_MAX_BLOCKS,
			sizeof (dtn_extension_block_t), (xdrproc_t) xdr_dtn_extension_block_t))
			 return FALSE;
		 if (!xdr_array (xdrs, (char **)&objp->metadata.metadata_val, (u_int *) &objp->metadata.metadata_len, DTN_MAX_BLOCKS,
			sizeof (dtn_extension_block_t), (xdrproc_t) xdr_dtn_extension_block_t))
			 return FALSE;
		return TRUE;
	} else if (xdrs->x_op == XDR_DECODE) {
		 if (!xdr_dtn_endpoint_id_t (xdrs, &objp->source))
			 return FALSE;
		 if (!xdr_dtn_endpoint_id_t (xdrs, &objp->dest))
			 return FALSE;
		 if (!xdr_dtn_endpoint_id_t (xdrs, &objp->replyto))
			 return FALSE;
		 if (!xdr_dtn_bundle_priority_t (xdrs, &objp->priority))
			 return FALSE;
		 if (!xdr_int (xdrs, &objp->dopts))
			 return FALSE;
		 if (!xdr_dtn_timeval_t (xdrs, &objp->expiration))
			 return FALSE;
		 if (!xdr_dtn_timestamp_t (xdrs, &objp->creation_ts))
			 return FALSE;
		 if (!xdr_dtn_reg_id_t (xdrs, &objp->delivery_regid))
			 return FALSE;
		 if (!xdr_dtn_sequence_id_t (xdrs, &objp->sequence_id))
			 return FALSE;
		 if (!xdr_dtn_sequence_id_t (xdrs, &objp->obsoletes_id))
			 return FALSE;
		buf = XDR_INLINE (xdrs, 5 * BYTES_PER_XDR_UNIT);
		if (buf == NULL) {
			 if (!xdr_bool (xdrs, &objp->ecos_enabled))
				 return FALSE;
			 if (!xdr_u_int (xdrs, &objp->ecos_flags))
				 return FALSE;
			 if (!xdr_u_int (xdrs, &objp->ecos_ordinal))
				 return FALSE;
			 if (!xdr_u_int (xdrs, &objp->ecos_flow_label))
				 return FALSE;
			 if (!xdr_u_int (xdrs, &objp->bp_version))
				 return FALSE;

		} else {
		objp->ecos_enabled = IXDR_GET_BOOL(buf);
		objp->ecos_flags = IXDR_GET_U_LONG(buf);
		objp->ecos_ordinal = IXDR_GET_U_LONG(buf);
		objp->ecos_flow_label = IXDR_GET_U_LONG(buf);
		objp->bp_version = IXDR_GET_U_LONG(buf);
		}
		 if (!xdr_array (xdrs, (char **)&objp->blocks.blocks_val, (u_int *) &objp->blocks.blocks_len, DTN_MAX_BLOCKS,
			sizeof (dtn_extension_block_t), (xdrproc_t) xdr_dtn_extension_block_t))
			 return FALSE;
		 if (!xdr_array (xdrs, (char **)&objp->metadata.metadata_val, (u_int *) &objp->metadata.metadata_len, DTN_MAX_BLOCKS,
			sizeof (dtn_extension_block_t), (xdrproc_t) xdr_dtn_extension_block_t))
			 return FALSE;
	 return TRUE;
	}

	 if (!xdr_dtn_endpoint_id_t (xdrs, &objp->source))
		 return FALSE;
	 if (!xdr_dtn_endpoint_id_t (xdrs, &objp->dest))
		 return FALSE;
	 if (!xdr_dtn_endpoint_id_t (xdrs, &objp->replyto))
		 return FALSE;
	 if (!xdr_dtn_bundle_priority_t (xdrs, &objp->priority))
		 return FALSE;
	 if (!xdr_int (xdrs, &objp->dopts))
		 return FALSE;
	 if (!xdr_dtn_timeval_t (xdrs, &objp->expiration))
		 return FALSE;
	 if (!xdr_dtn_timestamp_t (xdrs, &objp->creation_ts))
		 return FALSE;
	 if (!xdr_dtn_reg_id_t (xdrs, &objp->delivery_regid))
		 return FALSE;
	 if (!xdr_dtn_sequence_id_t (xdrs, &objp->sequence_id))
		 return FALSE;
	 if (!xdr_dtn_sequence_id_t (xdrs, &objp->obsoletes_id))
		 return FALSE;
	 if (!xdr_bool (xdrs, &objp->ecos_enabled))
		 return FALSE;
	 if (!xdr_u_int (xdrs, &objp->ecos_flags))
		 return FALSE;
	 if (!xdr_u_int (xdrs, &objp->ecos_ordinal))
		 return FALSE;
	 if (!xdr_u_int (xdrs, &objp->ecos_flow_label))
		 return FALSE;
	 if (!xdr_u_int (xdrs, &objp->bp_version))
		 return FALSE;
	 if (!xdr_array (xdrs, (char **)&objp->blocks.blocks_val, (u_int *) &objp->blocks.blocks_len, DTN_MAX_BLOCKS,
		sizeof (dtn_extension_block_t), (xdrproc_t) xdr_dtn_extension_block_t))
		 return FALSE;
	 if (!xdr_array (xdrs, (char **)&objp->metadata.metadata_val, (u_int *) &objp->metadata.metadata_len, DTN_MAX_BLOCKS,
		sizeof (dtn_extension_block_t), (xdrproc_t) xdr_dtn_extension_block_t))
		 return FALSE;
	return TRUE;
}

/**
 * Type definition for a unique bundle identifier. Returned from dtn_send
 * after the daemon has assigned the creation_secs and creation_subsecs,
 * in which case orig_length and frag_offset are always zero, and also in
 * status report data in which case they may be set if the bundle is
 * fragmented.
 */

bool_t
xdr_dtn_bundle_id_t (XDR *xdrs, dtn_bundle_id_t *objp)
{
	register int32_t *buf;

	 if (!xdr_dtn_endpoint_id_t (xdrs, &objp->source))
		 return FALSE;
	 if (!xdr_dtn_timestamp_t (xdrs, &objp->creation_ts))
		 return FALSE;
	 if (!xdr_u_int (xdrs, &objp->frag_offset))
		 return FALSE;
	 if (!xdr_u_int (xdrs, &objp->orig_length))
		 return FALSE;
	return TRUE;
}
/**
 * Bundle Status Report "Reason Code" flags
 */

bool_t
xdr_dtn_status_report_reason_t (XDR *xdrs, dtn_status_report_reason_t *objp)
{
	register int32_t *buf;

	 if (!xdr_enum (xdrs, (enum_t *) objp))
		 return FALSE;
	return TRUE;
}
/**
 * Bundle Status Report status flags that indicate which timestamps in
 * the status report structure are valid.
 */

bool_t
xdr_dtn_status_report_flags_t (XDR *xdrs, dtn_status_report_flags_t *objp)
{
	register int32_t *buf;

	 if (!xdr_enum (xdrs, (enum_t *) objp))
		 return FALSE;
	return TRUE;
}

/**
 * Type definition for a bundle status report.
 */

bool_t
xdr_dtn_bundle_status_report_t (XDR *xdrs, dtn_bundle_status_report_t *objp)
{
	register int32_t *buf;

	 if (!xdr_dtn_bundle_id_t (xdrs, &objp->bundle_id))
		 return FALSE;
	 if (!xdr_dtn_status_report_reason_t (xdrs, &objp->reason))
		 return FALSE;
	 if (!xdr_dtn_status_report_flags_t (xdrs, &objp->flags))
		 return FALSE;
	 if (!xdr_dtn_timestamp_t (xdrs, &objp->receipt_ts))
		 return FALSE;
	 if (!xdr_dtn_timestamp_t (xdrs, &objp->custody_ts))
		 return FALSE;
	 if (!xdr_dtn_timestamp_t (xdrs, &objp->forwarding_ts))
		 return FALSE;
	 if (!xdr_dtn_timestamp_t (xdrs, &objp->delivery_ts))
		 return FALSE;
	 if (!xdr_dtn_timestamp_t (xdrs, &objp->deletion_ts))
		 return FALSE;
	 if (!xdr_dtn_timestamp_t (xdrs, &objp->ack_by_app_ts))
		 return FALSE;
	return TRUE;
}

/**
 * The payload of a bundle can be sent or received either in a file,
 * in which case the payload structure contains the filename, or in
 * memory where the struct contains the data in-band, in the 'buf'
 * field.
 *
 * When sending a bundle, if the location specifies that the payload
 * is in a temp file, then the daemon assumes ownership of the file
 * and should have sufficient permissions to move or rename it.
 *
 * When receiving a bundle that is a status report, then the
 * status_report pointer will be non-NULL and will point to a
 * dtn_bundle_status_report_t structure which contains the parsed fields
 * of the status report.
 *
 *     DTN_PAYLOAD_MEM                       - payload contents in memory
 *     DTN_PAYLOAD_FILE                      - payload contents in file
 *     DTN_PAYLOAD_TEMP_FILE                 - in file, assume ownership (send only)
 *     DTN_PAYLOAD_VARIABLE                  - payload contents in memory if up to the max allowed else in file
 *     DTN_PAYLOAD_RELEASED_DB_FILE          - payload contents as original database file released to the app
 *     DTN_PAYLOAD_VARIABLE_RELEASED_DB_FILE - payload contents in memory if up to the max allowed else as
 *                                             original database file released to the app
 */

bool_t
xdr_dtn_bundle_payload_location_t (XDR *xdrs, dtn_bundle_payload_location_t *objp)
{
	register int32_t *buf;

	 if (!xdr_enum (xdrs, (enum_t *) objp))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_dtn_bundle_payload_t (XDR *xdrs, dtn_bundle_payload_t *objp)
{
	register int32_t *buf;

	 if (!xdr_dtn_bundle_payload_location_t (xdrs, &objp->location))
		 return FALSE;
	 if (!xdr_bytes (xdrs, (char **)&objp->filename.filename_val, (u_int *) &objp->filename.filename_len, DTN_MAX_PATH_LEN))
		 return FALSE;
	 if (!xdr_bytes (xdrs, (char **)&objp->buf.buf_val, (u_int *) &objp->buf.buf_len, DTN_MAX_BUNDLE_MEM))
		 return FALSE;
	 if (!xdr_pointer (xdrs, (char **)&objp->status_report, sizeof (dtn_bundle_status_report_t), (xdrproc_t) xdr_dtn_bundle_status_report_t))
		 return FALSE;
	 if (!xdr_uint64_t (xdrs, &objp->size_of_payload))
		 return FALSE;
	return TRUE;
}
