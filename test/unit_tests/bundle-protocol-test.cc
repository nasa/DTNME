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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <oasys/util/UnitTest.h>

#include "bundling/Bundle.h"
#include "bundling/BundleProtocol.h"
#include "bundling/UnknownBlockProcessor.h"
#include "storage/BundleStore.h"
#include "storage/DTNStorageConfig.h"

using namespace oasys;
using namespace dtn;

Bundle*
new_bundle()
{
    static int next_bundleid = 10;
    
    Bundle* b = new Bundle(oasys::Builder::builder());
    b->test_set_bundleid(next_bundleid++);
    b->set_is_fragment(false);
    b->set_is_admin(false);
    b->set_do_not_fragment(false);
    b->set_in_datastore(false);
    b->set_custody_requested(false);
    b->set_local_custody(false);
    b->set_singleton_dest(true);
    b->set_priority(0);
    b->set_receive_rcpt(false);
    b->set_custody_rcpt(false);
    b->set_forward_rcpt(false);
    b->set_delivery_rcpt(false);
    b->set_deletion_rcpt(false);
    b->set_app_acked_rcpt(false);
    b->set_orig_length(0);
    b->set_frag_offset(0);
    b->set_expiration(0);
    b->set_owner("");
    b->set_creation_ts(BundleTimestamp(0,0));
    b->mutable_payload()->init(b->bundleid());
    return b;
}

#define ONE_CHUNK      1
#define ONEBYTE_CHUNKS 2
#define RANDOM_CHUNKS  3

int
protocol_test(Bundle* b1, int chunks)
{
#define AVG_CHUNK 16
    
    u_char payload1[32768];
    u_char payload2[32768];
    u_char buf[32768];
    int encode_len, decode_len;
    int errno_; const char* strerror_;

    Bundle* b2;

    LinkRef link("protocol_test");
    BlockInfoVec* blocks = BundleProtocol::prepare_blocks(b1, link);
    encode_len = BundleProtocol::generate_blocks(b1, blocks, link);
    
    bool complete = false;
    int produce_len = 0;
    do {
        size_t chunk_size;
        switch(chunks) {
        case ONE_CHUNK:
            chunk_size = encode_len;
            ASSERT(encode_len < (int)sizeof(buf));
            break;

        case ONEBYTE_CHUNKS:
            chunk_size = 1;
            break;

        case RANDOM_CHUNKS:
            chunk_size = oasys::Random::rand(AVG_CHUNK);
            break;

        default:
            NOTREACHED;
        }
            
        size_t cc = BundleProtocol::produce(b1, blocks,
                                            &buf[produce_len],
                                            produce_len,
                                            chunk_size,
                                            &complete);
            
        ASSERT((cc == chunk_size) || complete);
        produce_len += cc;
        ASSERT(produce_len <= encode_len);
    } while (!complete);
    
    ASSERT(produce_len == encode_len);
    CHECK(encode_len > 0);
    
    // extract the payload before we swing the payload directory
    b1->payload().read_data(0, b1->payload().length(), payload1);
        
    b2 = new_bundle();

    complete = false;
    decode_len = 0;
    do {
        size_t chunk_size;
        switch(chunks) {
        case ONE_CHUNK:
            chunk_size = encode_len;
            ASSERT(encode_len < (int)sizeof(buf));
            break;

        case ONEBYTE_CHUNKS:
            chunk_size = 1;
            break;

        case RANDOM_CHUNKS:
            chunk_size = oasys::Random::rand(AVG_CHUNK);
            break;

        default:
            NOTREACHED;
        }
            
        int cc = BundleProtocol::consume(b2,
                                         &buf[decode_len],
                                         chunk_size,
                                         &complete);
        
        ASSERT((cc == (int)chunk_size) || complete);
        decode_len += cc;
        ASSERT(decode_len <= encode_len);
    } while (!complete);

    CHECK_EQUAL(decode_len, encode_len);

    CHECK_EQUALSTR(b1->source().c_str(),    b2->source().c_str());
    CHECK_EQUALSTR(b1->dest().c_str(),      b2->dest().c_str());
    CHECK_EQUALSTR(b1->custodian().c_str(), b2->custodian().c_str());
    CHECK_EQUALSTR(b1->replyto().c_str(),   b2->replyto().c_str());
    CHECK_EQUAL(b1->is_fragment(),          b2->is_fragment());
    CHECK_EQUAL(b1->is_admin(),             b2->is_admin());
    CHECK_EQUAL(b1->do_not_fragment(),      b2->do_not_fragment());
    CHECK_EQUAL(b1->custody_requested(),    b2->custody_requested());
    CHECK_EQUAL(b1->priority(),             b2->priority());
    CHECK_EQUAL(b1->receive_rcpt(),         b2->receive_rcpt());
    CHECK_EQUAL(b1->custody_rcpt(),         b2->custody_rcpt());
    CHECK_EQUAL(b1->forward_rcpt(),         b2->forward_rcpt());
    CHECK_EQUAL(b1->delivery_rcpt(),        b2->delivery_rcpt());
    CHECK_EQUAL(b1->deletion_rcpt(),        b2->deletion_rcpt());
    CHECK_EQUAL(b1->creation_ts().seconds_, b2->creation_ts().seconds_);
    CHECK_EQUAL(b1->creation_ts().seqno_,   b2->creation_ts().seqno_);
    CHECK_EQUAL(b1->expiration(),           b2->expiration());
    CHECK_EQUAL(b1->frag_offset(),          b2->frag_offset());
    CHECK_EQUAL(b1->orig_length(),          b2->orig_length());
    CHECK_EQUAL(b1->payload().length(),     b2->payload().length());
    CHECK_EQUALSTR(b1->sequence_id().to_str(),  b2->sequence_id().to_str());
    CHECK_EQUALSTR(b1->obsoletes_id().to_str(), b2->obsoletes_id().to_str());

    b2->payload().read_data(0, b2->payload().length(), payload2);

    bool payload_ok = true;
    for (u_int i = 0; i < b2->payload().length(); ++i) {
        if (payload1[i] != payload2[i]) {
            log_err_p("/test", "payload mismatch at byte %d: 0x%x != 0x%x",
                      i, payload1[i], payload2[i]);
            payload_ok = false;
        }
    }
    CHECK(payload_ok);

    // check extension blocks
    b1->lock()->lock("protocol_test");
    if (b1->recv_blocks().size() != 0)
    {
        for (BlockInfoVec::const_iterator iter = b1->recv_blocks().begin();
             iter != b1->recv_blocks().end();
             ++iter)
        {
            if (iter->type() == BundleProtocol::PRIMARY_BLOCK ||
                iter->type() == BundleProtocol::PAYLOAD_BLOCK)
            {
                continue;
            }

            const BlockInfo* block1 = &*iter;
            const BlockInfo* block2 = b2->recv_blocks().find_block(iter->type());

            CHECK_EQUAL(block1->type(), block2->type());

            u_int64_t flags1 = block1->flags();
            u_int64_t flags2 = block2->flags();
            CHECK((flags2 & BundleProtocol::BLOCK_FLAG_FORWARDED_UNPROCESSED) != 0);
            flags2 &= ~BundleProtocol::BLOCK_FLAG_FORWARDED_UNPROCESSED;
            CHECK_EQUAL_U64(flags1, flags2);
            
            CHECK_EQUAL(block1->eid_list().size(), block2->eid_list().size());
            for (size_t i = 0; i < block1->eid_list().size(); ++i) {
                CHECK_EQUALSTR(block1->eid_list()[i].str(),
                               block2->eid_list()[i].str());
            }
            
            CHECK_EQUAL(block1->data_length(), block2->data_length());
            const u_char* contents1 = block1->contents().buf() + block1->data_offset();
            const u_char* contents2 = block2->contents().buf() + block2->data_offset();
            bool contents_ok = true;
            
            for (u_int i = 0; i < block1->data_length(); ++i) {
                if (contents1[i] != contents2[i]) {
                    log_err_p("/test", "extension block mismatch at byte %d: "
                              "0x%x != 0x%x",
                              i, contents1[i], contents2[i]);
                    contents_ok = false;
                }
            }
            CHECK(contents_ok);
        }
    }
    b1->lock()->unlock();
    
    delete b1;
    delete b2;

    return UNIT_TEST_PASSED;
}

DECLARE_TEST(Init) {
    BundleProtocol::init_default_processors();
    return UNIT_TEST_PASSED;
}

#define DECLARE_BP_TESTS(_what)                                 \
DECLARE_TEST(_what ## OneChunk) {                               \
    return protocol_test(init_ ## _what (), ONE_CHUNK);         \
}                                                               \
                                                                \
DECLARE_TEST(_what ## OneByteChunks) {                          \
    return protocol_test(init_ ## _what (), ONEBYTE_CHUNKS);    \
}                                                               \
                                                                \
DECLARE_TEST(_what ## RandomChunks) {                           \
    return protocol_test(init_ ## _what (), RANDOM_CHUNKS);     \
}                                                               \

#define ADD_BP_TESTS(_what)                     \
ADD_TEST(_what ## OneChunk);                    \
ADD_TEST(_what ## OneByteChunks);               \
ADD_TEST(_what ## RandomChunks);

Bundle* init_Basic()
{
    Bundle* bundle = new_bundle();
    bundle->mutable_payload()->set_data("test payload");
    
    bundle->mutable_source()->assign("dtn://source.dtn/test");
    bundle->mutable_dest()->assign("dtn://dest.dtn/test");
    bundle->mutable_custodian()->assign("dtn:none");
    bundle->mutable_replyto()->assign("dtn:none");
    bundle->set_expiration(1000);
    bundle->set_creation_ts(BundleTimestamp(10101010, 44556677));

    return bundle;
}

DECLARE_BP_TESTS(Basic);

Bundle* init_Fragment()
{
    Bundle* bundle = new_bundle();
    bundle->mutable_payload()->set_data("test payload");
    
    bundle->mutable_source()->assign("dtn://frag.dtn/test");
    bundle->mutable_dest()->assign("dtn://dest.dtn/test");
    bundle->mutable_custodian()->assign("dtn:none");
    bundle->mutable_replyto()->assign("dtn:none");
    
    bundle->set_expiration(30);
    bundle->set_is_fragment(1);
    bundle->set_frag_offset(123456789);
    bundle->set_orig_length(1234567890);
    
    return bundle;
}

DECLARE_BP_TESTS(Fragment);

Bundle* init_AllFlags()
{
    Bundle* bundle = new_bundle();
    bundle->mutable_payload()->set_data("test payload");
    
    bundle->mutable_source()->assign("dtn://source.dtn/test");
    bundle->mutable_dest()->assign("dtn://dest.dtn/test");
    bundle->mutable_custodian()->assign("dtn:none");
    bundle->mutable_replyto()->assign("dtn:none");

    bundle->set_is_admin(true);
    bundle->set_do_not_fragment(true);
    bundle->set_custody_requested(true);
    bundle->set_priority(3);
    bundle->set_receive_rcpt(true);
    bundle->set_custody_rcpt(true);
    bundle->set_forward_rcpt(true);
    bundle->set_delivery_rcpt(true);
    bundle->set_deletion_rcpt(true);
    
    bundle->set_expiration(1000);
    bundle->set_creation_ts(BundleTimestamp(10101010, 44556677));

    return bundle;
}

DECLARE_BP_TESTS(AllFlags);

Bundle* init_RandomPayload()
{
    Bundle* bundle = new_bundle();

    u_char payload[4096];
    for (u_int i = 0; i < sizeof(payload); ++i) {
        payload[i] = oasys::Random::rand(26) + 'a';
    }
    bundle->mutable_payload()->set_data(payload, sizeof(payload));
    
    bundle->mutable_source()->assign("dtn://source.dtn/test");
    bundle->mutable_dest()->assign("dtn://dest.dtn/test");
    bundle->mutable_custodian()->assign("dtn:none");
    bundle->mutable_replyto()->assign("dtn:none");

    return bundle;
}

DECLARE_BP_TESTS(RandomPayload);

Bundle* init_UnknownBlocks()
{
    Bundle* bundle = new_bundle();
    bundle->mutable_payload()->set_data("test payload");
    
    bundle->mutable_source()->assign("dtn://source.dtn/test");
    bundle->mutable_dest()->assign("dtn://dest.dtn/test");
    bundle->mutable_custodian()->assign("dtn:none");
    bundle->mutable_replyto()->assign("dtn:none");

    // fake some blocks arriving off the wire
    BlockProcessor* primary_bp =
        BundleProtocol::find_processor(BundleProtocol::PRIMARY_BLOCK);
    BlockProcessor* payload_bp =
        BundleProtocol::find_processor(BundleProtocol::PAYLOAD_BLOCK);
    BlockProcessor* unknown1_bp = BundleProtocol::find_processor(0xaa);
    BlockProcessor* unknown2_bp = BundleProtocol::find_processor(0xbb);
    BlockProcessor* unknown3_bp = BundleProtocol::find_processor(0xcc);

    ASSERT(unknown1_bp == UnknownBlockProcessor::instance());
    ASSERT(unknown2_bp == UnknownBlockProcessor::instance());
    ASSERT(unknown3_bp == UnknownBlockProcessor::instance());

    BlockInfoVec* recv_blocks = bundle->mutable_recv_blocks();
    BlockInfo *primary, *payload, *unknown1, *unknown2, *unknown3;
    
    // initialize all blocks other than the primary
    primary  = recv_blocks->append_block(primary_bp);
    
    unknown1 = recv_blocks->append_block(unknown1_bp);
    const char* contents = "this is an extension block";
    UnknownBlockProcessor::instance()->
        init_block(unknown1, recv_blocks, NULL, 0xaa, 0x0,
                   (const u_char*)contents, strlen(contents));
    
    unknown2 = recv_blocks->append_block(unknown2_bp);
    UnknownBlockProcessor::instance()->
        init_block(unknown2, recv_blocks, NULL, 0xbb,
                   BundleProtocol::BLOCK_FLAG_REPLICATE, 0, 0);
    
    payload  = recv_blocks->append_block(payload_bp);
    UnknownBlockProcessor::instance()->
        init_block(payload, recv_blocks, NULL,
                   BundleProtocol::PAYLOAD_BLOCK,
                   0, (const u_char*)"test payload", strlen("test payload"));

    unknown3 = recv_blocks->append_block(unknown3_bp);
    UnknownBlockProcessor::instance()->
        init_block(unknown3, recv_blocks, NULL, 0xcc,
                   BundleProtocol::BLOCK_FLAG_REPLICATE |
                   BundleProtocol::BLOCK_FLAG_LAST_BLOCK,
                   (const u_char*)contents, strlen(contents));
    
    return bundle;
}

DECLARE_BP_TESTS(UnknownBlocks);

Bundle* init_BigDictionary()
{
    Bundle* bundle = new_bundle();
    bundle->mutable_payload()->set_data("test payload");
    
    bundle->mutable_source()->assign("dtn://source.dtn/test");
    bundle->mutable_dest()->assign("dtn://dest.dtn/test");
    bundle->mutable_custodian()->assign("dtn:none");
    bundle->mutable_replyto()->assign("dtn:none");

    // fake some blocks arriving off the wire
    BlockProcessor* primary_bp =
        BundleProtocol::find_processor(BundleProtocol::PRIMARY_BLOCK);
    BlockProcessor* payload_bp =
        BundleProtocol::find_processor(BundleProtocol::PAYLOAD_BLOCK);
    BlockProcessor* unknown_bp = BundleProtocol::find_processor(0xaa);

    ASSERT(unknown_bp == UnknownBlockProcessor::instance());

    BlockInfoVec* recv_blocks = bundle->mutable_recv_blocks();
    BlockInfo *primary, *payload, *unknown;

    primary = recv_blocks->append_block(primary_bp);
    unknown = recv_blocks->append_block(unknown_bp);
    
    unknown->add_eid(EndpointID("dtn:none"));
    unknown->add_eid(EndpointID("dtn://something"));
    unknown->add_eid(EndpointID("dtn://host.com/bar"));
    unknown->add_eid(EndpointID("dtnme://other.host.com/bar"));
    unknown->add_eid(EndpointID("dtn3://www.dtnrg.org/code"));
    unknown->add_eid(EndpointID("http://www.google.com/"));
    unknown->add_eid(EndpointID("https://www.yahoo.com/"));
    unknown->add_eid(EndpointID("smtp://mail.you.com/"));
    unknown->add_eid(EndpointID("news://news.google.com/"));
    unknown->add_eid(EndpointID("abcdef:ghijkl"));
    unknown->add_eid(EndpointID("ghijkl:abcdef"));
    unknown->add_eid(EndpointID("ghijklxxx:abcdefyyy"));
    
    const char* contents = "this is an extension block";
    UnknownBlockProcessor::instance()->
        init_block(unknown, recv_blocks, NULL, 0xaa,
                   BundleProtocol::BLOCK_FLAG_EID_REFS,
                   (const u_char*)contents, strlen(contents));

    payload  = recv_blocks->append_block(payload_bp);
    UnknownBlockProcessor::instance()->
        init_block(payload, recv_blocks, NULL,
                   BundleProtocol::PAYLOAD_BLOCK,
                   BundleProtocol::BLOCK_FLAG_LAST_BLOCK,
                   (const u_char*)"test payload", strlen("test payload"));

    return bundle;
}

DECLARE_BP_TESTS(BigDictionary);

Bundle* init_SequenceID()
{
    Bundle* bundle = new_bundle();
    bundle->mutable_payload()->set_data("test payload");
    
    bundle->mutable_source()->assign("dtn://source.dtn/test");
    bundle->mutable_dest()->assign("dtn://dest.dtn/test");
    bundle->mutable_custodian()->assign("dtn:none");
    bundle->mutable_replyto()->assign("dtn:none");
    bundle->set_expiration(1000);
    bundle->set_creation_ts(BundleTimestamp(10101010, 44556677));

    bundle->mutable_sequence_id()->add(EndpointID("dtn://source.dtn/test"), 1);
    bundle->mutable_sequence_id()->add(EndpointID("dtn://dest.dtn/test"), 2);
    bundle->mutable_sequence_id()->add(EndpointID("dtn://other.dtn/test"), 3);
    bundle->mutable_sequence_id()->add(EndpointID("http://foo.com"), 10);

    return bundle;
}

DECLARE_BP_TESTS(SequenceID);

Bundle* init_ObsoletesID()
{
    Bundle* bundle = new_bundle();
    bundle->mutable_payload()->set_data("test payload");
    
    bundle->mutable_source()->assign("dtn://source.dtn/test");
    bundle->mutable_dest()->assign("dtn://dest.dtn/test");
    bundle->mutable_custodian()->assign("dtn:none");
    bundle->mutable_replyto()->assign("dtn:none");
    bundle->set_expiration(1000);
    bundle->set_creation_ts(BundleTimestamp(10101010, 44556677));

    bundle->mutable_obsoletes_id()->add(EndpointID("dtn://source.dtn/test"), 1);
    bundle->mutable_obsoletes_id()->add(EndpointID("dtn://dest.dtn/test"), 2);
    bundle->mutable_obsoletes_id()->add(EndpointID("dtn://other.dtn/test"), 3);
    bundle->mutable_obsoletes_id()->add(EndpointID("http://foo.com"), 10);

    return bundle;
}

DECLARE_BP_TESTS(ObsoletesID);

Bundle* init_SequenceAndObsoletesID()
{
    Bundle* bundle = new_bundle();
    bundle->mutable_payload()->set_data("test payload");
    
    bundle->mutable_source()->assign("dtn://source.dtn/test");
    bundle->mutable_dest()->assign("dtn://dest.dtn/test");
    bundle->mutable_custodian()->assign("dtn:none");
    bundle->mutable_replyto()->assign("dtn:none");
    bundle->set_expiration(1000);
    bundle->set_creation_ts(BundleTimestamp(10101010, 44556677));

    bundle->mutable_sequence_id()->add(EndpointID("dtn://source.dtn/test"), 1);
    bundle->mutable_sequence_id()->add(EndpointID("dtn://dest.dtn/test"), 2);
    bundle->mutable_sequence_id()->add(EndpointID("dtn://other.dtn/test"), 3);
    bundle->mutable_sequence_id()->add(EndpointID("http://foo.com"), 10);

    bundle->mutable_obsoletes_id()->add(EndpointID("dtn://source.dtn/test2"), 1);
    bundle->mutable_obsoletes_id()->add(EndpointID("dtn://dest.dtn/test"), 3);
    bundle->mutable_obsoletes_id()->add(EndpointID("dtn://someoneelse.dtn/test"), 10);
    bundle->mutable_obsoletes_id()->add(EndpointID("http://bar.com"), 10);

    return bundle;
}

DECLARE_BP_TESTS(SequenceAndObsoletesID);

DECLARE_TESTER(BundleProtocolTest) {
    ADD_TEST(Init);
    ADD_BP_TESTS(Basic);
    ADD_BP_TESTS(Fragment);
    ADD_BP_TESTS(AllFlags);
    ADD_BP_TESTS(RandomPayload);
    ADD_BP_TESTS(UnknownBlocks);
    ADD_BP_TESTS(BigDictionary);
    ADD_BP_TESTS(SequenceID);
    ADD_BP_TESTS(ObsoletesID);
    ADD_BP_TESTS(SequenceAndObsoletesID);

    // XXX/demmer add tests for malformed / mangled headers, too long
    // sdnv's, etc
}

int
main(int argc, const char** argv)
{
    BundleProtocolTest t("bundle protocol test");
    t.init(argc, argv, true);
    
    system("rm -rf .bundle-protocol-test");
    system("mkdir  .bundle-protocol-test");

    DTNStorageConfig cfg("", "memorydb", "", "");
    cfg.init_ = true;
    cfg.payload_dir_.assign(".bundle-protocol-test");
    cfg.leave_clean_file_ = false;

    oasys::DurableStore ds("/test/ds");
    ds.create_store(cfg);
    
    BundleStore::init(cfg, &ds);
    
    t.run_tests();

    system("rm -rf .bundle-protocol-test");
}
