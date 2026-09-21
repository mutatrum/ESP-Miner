#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "unity.h"
#include "gbt_fsm_parser.h"

static const char s_sample_gbt_json[] =
"{\n"
"  \"result\": {\n"
"    \"capabilities\": [\"proposal\"],\n"
"    \"version\": 536870912,\n"
"    \"previousblockhash\": \"000000000000000000021b7787bf4c2c01fa9596ddc81e9fcf08f88bb8ef8f87\",\n"
"    \"transactions\": [\n"
"      {\n"
"        \"data\": \"02000000010000000000000000000000000000000000000000000000000000000000000000ffffffff0100f2052a01000000160014000000000000000000000000000000000000000000000000\",\n"
"        \"txid\": \"4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b\",\n"
"        \"hash\": \"4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b\",\n"
"        \"fee\": 1000\n"
"      }\n"
"    ],\n"
"    \"coinbaseaux\": {\n"
"      \"flags\": \"\"\n"
"    },\n"
"    \"coinbasevalue\": 312500000,\n"
"    \"longpollid\": \"test_longpoll_id_9876\",\n"
"    \"target\": \"00000000000000000002b8610000000000000000000000000000000000000000\",\n"
"    \"mintime\": 1726870000,\n"
"    \"mutable\": [\"time\", \"transactions\", \"prevblock\"],\n"
"    \"noncerange\": \"00000000ffffffff\",\n"
"    \"sigoplimit\": 80000,\n"
"    \"sizelimit\": 4000000,\n"
"    \"weightlimit\": 4000000,\n"
"    \"curtime\": 1726870500,\n"
"    \"bits\": \"1702b861\",\n"
"    \"height\": 840000,\n"
"    \"default_witness_commitment\": \"aa21a9ed6f2334f5904d9c7adca3630f9a5658e45cc23588933b9f3feab9058b\"\n"
"  },\n"
"  \"error\": null,\n"
"  \"id\": 1\n"
"}\n";

TEST_CASE("GBT FSM Parser - full JSON in single feed", "[gbt_fsm]")
{
    gbt_template_t tmpl = {
        .raw_tx_cap = 16 * 1024,
        .tx_cap = 16
    };
    esp_err_t err = gbt_template_init(&tmpl);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    gbt_fsm_parser_t parser;
    gbt_fsm_parser_init(&parser);

    size_t json_len = strlen(s_sample_gbt_json);
    err = gbt_fsm_parser_feed(&parser, &tmpl, (const uint8_t *)s_sample_gbt_json, json_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    TEST_ASSERT_EQUAL_UINT32(536870912, tmpl.version);
    TEST_ASSERT_EQUAL_UINT32(840000, tmpl.height);
    TEST_ASSERT_EQUAL_UINT32(1726870500, tmpl.curtime);
    TEST_ASSERT_EQUAL_HEX32(0x1702b861, tmpl.nbits);
    TEST_ASSERT_EQUAL_UINT64(312500000ULL, tmpl.coinbase_value);
    TEST_ASSERT_EQUAL_STRING("000000000000000000021b7787bf4c2c01fa9596ddc81e9fcf08f88bb8ef8f87", tmpl.previous_block_hash);
    TEST_ASSERT_EQUAL_STRING("test_longpoll_id_9876", tmpl.longpollid);
    TEST_ASSERT_EQUAL_STRING("aa21a9ed6f2334f5904d9c7adca3630f9a5658e45cc23588933b9f3feab9058b", tmpl.default_witness_commitment);

    TEST_ASSERT_EQUAL(1, tmpl.tx_count);
    TEST_ASSERT_EQUAL(77, tmpl.raw_tx_len);
    TEST_ASSERT_EQUAL(0, tmpl.tx_offsets[0]);
    TEST_ASSERT_EQUAL(77, tmpl.tx_lengths[0]);

    // Verify first 4 bytes of tx data are version 2: 02 00 00 00
    TEST_ASSERT_EQUAL_HEX8(0x02, tmpl.raw_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, tmpl.raw_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x00, tmpl.raw_tx_data[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00, tmpl.raw_tx_data[3]);

    // Verify txid is stored in internal byte order (reversed from RPC display hex)
    // txid: "4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"
    // Internal first byte is 0x3b, second is 0xa3, last is 0x4a
    TEST_ASSERT_EQUAL_HEX8(0x3b, tmpl.txid_hashes[0]);
    TEST_ASSERT_EQUAL_HEX8(0xa3, tmpl.txid_hashes[1]);
    TEST_ASSERT_EQUAL_HEX8(0x4a, tmpl.txid_hashes[31]);

    gbt_template_free(&tmpl);
}

TEST_CASE("GBT FSM Parser - streaming in tiny chunks across token boundaries", "[gbt_fsm]")
{
    gbt_template_t tmpl = {
        .raw_tx_cap = 16 * 1024,
        .tx_cap = 16
    };
    esp_err_t err = gbt_template_init(&tmpl);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    gbt_fsm_parser_t parser;
    gbt_fsm_parser_init(&parser);

    size_t json_len = strlen(s_sample_gbt_json);
    const size_t chunk_size = 7; // prime number to force splits across all possible byte alignments

    for (size_t offset = 0; offset < json_len; offset += chunk_size) {
        size_t len = (offset + chunk_size <= json_len) ? chunk_size : (json_len - offset);
        err = gbt_fsm_parser_feed(&parser, &tmpl, (const uint8_t *)(s_sample_gbt_json + offset), len);
        TEST_ASSERT_EQUAL(ESP_OK, err);
    }

    TEST_ASSERT_EQUAL_UINT32(536870912, tmpl.version);
    TEST_ASSERT_EQUAL_UINT32(840000, tmpl.height);
    TEST_ASSERT_EQUAL_UINT32(1726870500, tmpl.curtime);
    TEST_ASSERT_EQUAL_HEX32(0x1702b861, tmpl.nbits);
    TEST_ASSERT_EQUAL_UINT64(312500000ULL, tmpl.coinbase_value);
    TEST_ASSERT_EQUAL_STRING("000000000000000000021b7787bf4c2c01fa9596ddc81e9fcf08f88bb8ef8f87", tmpl.previous_block_hash);
    TEST_ASSERT_EQUAL_STRING("test_longpoll_id_9876", tmpl.longpollid);
    TEST_ASSERT_EQUAL_STRING("aa21a9ed6f2334f5904d9c7adca3630f9a5658e45cc23588933b9f3feab9058b", tmpl.default_witness_commitment);

    TEST_ASSERT_EQUAL(1, tmpl.tx_count);
    TEST_ASSERT_EQUAL(77, tmpl.raw_tx_len);
    TEST_ASSERT_EQUAL_HEX8(0x3b, tmpl.txid_hashes[0]);
    TEST_ASSERT_EQUAL_HEX8(0x4a, tmpl.txid_hashes[31]);

    gbt_template_free(&tmpl);
}
