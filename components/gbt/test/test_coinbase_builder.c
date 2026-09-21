#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "coinbase_builder.h"
#include "utils.h"

TEST_CASE("Coinbase address validation and decoding - P2PKH", "[coinbase_builder]")
{
    // Satoshi genesis address (mainnet P2PKH)
    const char *addr_mainnet = "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa";
    TEST_ASSERT_TRUE(coinbase_validate_address(addr_mainnet));

    uint8_t script[COINBASE_MAX_SCRIPT_PUBKEY_LEN];
    size_t script_len = 0;
    esp_err_t err = coinbase_address_to_script(addr_mainnet, script, sizeof(script), &script_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(25, script_len);
    TEST_ASSERT_EQUAL_HEX8(0x76, script[0]);  // OP_DUP
    TEST_ASSERT_EQUAL_HEX8(0xa9, script[1]);  // OP_HASH160
    TEST_ASSERT_EQUAL_HEX8(0x14, script[2]);  // OP_PUSHDATA(20)
    TEST_ASSERT_EQUAL_HEX8(0x88, script[23]); // OP_EQUALVERIFY
    TEST_ASSERT_EQUAL_HEX8(0xac, script[24]); // OP_CHECKSIG

    // Testnet P2PKH
    const char *addr_testnet = "mipcBbFg9gMiCh81Kj8tqqdgoZub1ZJRfn";
    TEST_ASSERT_TRUE(coinbase_validate_address(addr_testnet));
    err = coinbase_address_to_script(addr_testnet, script, sizeof(script), &script_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(25, script_len);
}

TEST_CASE("Coinbase address validation and decoding - P2SH", "[coinbase_builder]")
{
    // Mainnet P2SH
    const char *addr_mainnet = "3J98t1WpEZ73CNmQviecrnyiWrnqRhWNLy";
    TEST_ASSERT_TRUE(coinbase_validate_address(addr_mainnet));

    uint8_t script[COINBASE_MAX_SCRIPT_PUBKEY_LEN];
    size_t script_len = 0;
    esp_err_t err = coinbase_address_to_script(addr_mainnet, script, sizeof(script), &script_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(23, script_len);
    TEST_ASSERT_EQUAL_HEX8(0xa9, script[0]);  // OP_HASH160
    TEST_ASSERT_EQUAL_HEX8(0x14, script[1]);  // OP_PUSHDATA(20)
    TEST_ASSERT_EQUAL_HEX8(0x87, script[22]); // OP_EQUAL

    // Testnet P2SH
    const char *addr_testnet = "2MzQwSSnBHWHq3431up74QX3sVUMej84Dbu";
    TEST_ASSERT_TRUE(coinbase_validate_address(addr_testnet));
    err = coinbase_address_to_script(addr_testnet, script, sizeof(script), &script_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(23, script_len);
}

TEST_CASE("Coinbase address validation and decoding - SegWit v0 P2WPKH / P2WSH", "[coinbase_builder]")
{
    // Mainnet P2WPKH (20-byte witness program)
    const char *p2wpkh = "bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4";
    TEST_ASSERT_TRUE(coinbase_validate_address(p2wpkh));

    uint8_t script[COINBASE_MAX_SCRIPT_PUBKEY_LEN];
    size_t script_len = 0;
    esp_err_t err = coinbase_address_to_script(p2wpkh, script, sizeof(script), &script_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(22, script_len);
    TEST_ASSERT_EQUAL_HEX8(0x00, script[0]);  // OP_0
    TEST_ASSERT_EQUAL_HEX8(0x14, script[1]);  // OP_PUSHDATA(20)

    // Mainnet P2WSH (32-byte witness program)
    const char *p2wsh = "bc1qrp33g0q5c5txsp9arysrx4k6zdkfs4nce4xj0gdcccefvpysxf3qccfmv3";
    TEST_ASSERT_TRUE(coinbase_validate_address(p2wsh));
    err = coinbase_address_to_script(p2wsh, script, sizeof(script), &script_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(34, script_len);
    TEST_ASSERT_EQUAL_HEX8(0x00, script[0]);  // OP_0
    TEST_ASSERT_EQUAL_HEX8(0x20, script[1]);  // OP_PUSHDATA(32)

    // Testnet P2WPKH
    const char *tb_p2wpkh = "tb1qw508d6qejxtdg4y5r3zarvary0c5xw7kxpjzsx";
    TEST_ASSERT_TRUE(coinbase_validate_address(tb_p2wpkh));
}

TEST_CASE("Coinbase address validation and decoding - SegWit v1 Taproot P2TR", "[coinbase_builder]")
{
    // Mainnet P2TR (32-byte witness program)
    const char *p2tr = "bc1p0xlxvlhemja6c4dqv22uapctqupfhlxm9h8z3k2e72q4k9hcz7vqzk5jj0";
    TEST_ASSERT_TRUE(coinbase_validate_address(p2tr));

    uint8_t script[COINBASE_MAX_SCRIPT_PUBKEY_LEN];
    size_t script_len = 0;
    esp_err_t err = coinbase_address_to_script(p2tr, script, sizeof(script), &script_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(34, script_len);
    TEST_ASSERT_EQUAL_HEX8(0x51, script[0]);  // OP_1
    TEST_ASSERT_EQUAL_HEX8(0x20, script[1]);  // OP_PUSHDATA(32)
}

TEST_CASE("Coinbase address validation - invalid addresses", "[coinbase_builder]")
{
    TEST_ASSERT_FALSE(coinbase_validate_address(NULL));
    TEST_ASSERT_FALSE(coinbase_validate_address(""));
    TEST_ASSERT_FALSE(coinbase_validate_address("invalid_address_string"));
    TEST_ASSERT_FALSE(coinbase_validate_address("1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNb")); // Bad checksum
    TEST_ASSERT_FALSE(coinbase_validate_address("bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t5"));
}

TEST_CASE("Coinbase scriptSig construction and BIP34 height", "[coinbase_builder]")
{
    uint8_t scriptsig[COINBASE_MAX_SCRIPTSIG_LEN];
    size_t scriptsig_len = 0;
    uint8_t extranonce[8] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };

    // Block 840,000 (0x0cd140)
    uint32_t height = 840000;
    esp_err_t err = coinbase_build_scriptsig(height, extranonce, sizeof(extranonce), "/AxeOS/", scriptsig, &scriptsig_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_TRUE(scriptsig_len >= 2 && scriptsig_len <= 100);

    // BIP34: first byte is height len (3), followed by 0x40, 0xd1, 0x0c
    TEST_ASSERT_EQUAL_HEX8(0x03, scriptsig[0]);
    TEST_ASSERT_EQUAL_HEX8(0x40, scriptsig[1]);
    TEST_ASSERT_EQUAL_HEX8(0xd1, scriptsig[2]);
    TEST_ASSERT_EQUAL_HEX8(0x0c, scriptsig[3]);

    // Next is tag push: opcode 0x07, "/AxeOS/"
    TEST_ASSERT_EQUAL_HEX8(0x07, scriptsig[4]);
    TEST_ASSERT_EQUAL_MEMORY("/AxeOS/", scriptsig + 5, 7);

    // Next is extranonce push: opcode 0x08, extranonce bytes
    TEST_ASSERT_EQUAL_HEX8(0x08, scriptsig[12]);
    TEST_ASSERT_EQUAL_MEMORY(extranonce, scriptsig + 13, 8);
    TEST_ASSERT_EQUAL(21, scriptsig_len);
}

TEST_CASE("Coinbase scriptSig configurable tag length and boundaries", "[coinbase_builder]")
{
    uint8_t scriptsig[COINBASE_MAX_SCRIPTSIG_LEN];
    size_t scriptsig_len = 0;
    uint8_t extranonce[8] = { 0 };

    // Max 64-byte tag
    char long_tag[65];
    memset(long_tag, 'A', 64);
    long_tag[64] = '\0';

    esp_err_t err = coinbase_build_scriptsig(840000, extranonce, sizeof(extranonce), long_tag, scriptsig, &scriptsig_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    // Height (4) + Tag (1 + 64 = 65) + Extranonce (1 + 8 = 9) = 78 bytes
    TEST_ASSERT_EQUAL(78, scriptsig_len);
    TEST_ASSERT_TRUE(scriptsig_len <= 100);
}

TEST_CASE("Coinbase full transaction construction", "[coinbase_builder]")
{
    uint8_t coinbase_tx[COINBASE_MAX_TX_LEN];
    size_t coinbase_tx_len = 0;
    uint8_t coinbase_txid[32];
    uint8_t extranonce[8] = { 0xaa, 0xbb, 0xcc, 0xdd, 0x11, 0x22, 0x33, 0x44 };

    const char *payout = "bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4";
    // 3.125 BTC in satoshis = 312,500,000 = 0x12a05f20
    uint64_t reward = 312500000ULL;
    const char *witness_commitment = "aa21a9ed6f2334f5904d9c7adca3630f9a5658e45cc23588933b9f3feab9058b";

    esp_err_t err = coinbase_build_tx(840000, reward, payout, witness_commitment,
                                     extranonce, sizeof(extranonce), "/AxeOS/",
                                     coinbase_tx, sizeof(coinbase_tx), &coinbase_tx_len,
                                     coinbase_txid);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_TRUE(coinbase_tx_len > 0);

    // Check version = 1 or 2 (4 bytes little-endian)
    uint32_t version = coinbase_tx[0] | (coinbase_tx[1] << 8) | (coinbase_tx[2] << 16) | (coinbase_tx[3] << 24);
    TEST_ASSERT_TRUE(version == 1 || version == 2);

    // Check txid is non-zero
    uint8_t zero_hash[32] = { 0 };
    TEST_ASSERT_NOT_EQUAL(0, memcmp(coinbase_txid, zero_hash, 32));
}

TEST_CASE("Coinbase Merkle root computation and fast rolling", "[coinbase_builder]")
{
    uint8_t coinbase_txid[32];
    memset(coinbase_txid, 0x01, 32);

    // Synthetic block with 3 transactions (coinbase + 2 txs)
    uint8_t tx_hashes[2 * 32];
    memset(tx_hashes, 0x02, 32);
    memset(tx_hashes + 32, 0x03, 32);

    uint8_t merkle_path[COINBASE_MAX_MERKLE_BRANCHES][32];
    size_t merkle_path_count = 0;
    uint8_t merkle_root1[32];

    esp_err_t err = coinbase_compute_merkle_path(coinbase_txid, tx_hashes, 2,
                                                merkle_path, &merkle_path_count,
                                                merkle_root1);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_TRUE(merkle_path_count > 0);

    // Roll extranonce: update coinbase txid
    uint8_t updated_coinbase_txid[32];
    memset(updated_coinbase_txid, 0x05, 32);

    uint8_t rolled_root[32];
    coinbase_update_merkle_root(updated_coinbase_txid, merkle_path, merkle_path_count, rolled_root);

    // Verify rolling matches recalculating from scratch
    uint8_t scratch_root[32];
    uint8_t dummy_path[COINBASE_MAX_MERKLE_BRANCHES][32];
    size_t dummy_count = 0;
    err = coinbase_compute_merkle_path(updated_coinbase_txid, tx_hashes, 2,
                                      dummy_path, &dummy_count,
                                      scratch_root);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_MEMORY(scratch_root, rolled_root, 32);
}
