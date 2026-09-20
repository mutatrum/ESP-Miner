#include "coinbase_builder.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "libbase58.h"
#include "segwit_addr.h"
#include "utils.h"

static const char *TAG = "coinbase_builder";

#define OP_0                0x00
#define OP_1                0x51
#define OP_DUP              0x76
#define OP_EQUAL            0x87
#define OP_EQUALVERIFY      0x88
#define OP_HASH160          0xa9
#define OP_CHECKSIG         0xac
#define OP_RETURN           0x6a
#define OP_PUSHDATA_20      0x14
#define OP_PUSHDATA_32      0x20

#define COINBASE_VERSION 2
#define SEQUENCE_FINAL 0xffffffffU

static bool base58_sha256(void *digest, const void *data, size_t datasz)
{
    sha256_bin((const uint8_t *)data, datasz, (uint8_t *)digest);
    return true;
}

static void ensure_base58_initialized(void)
{
    if (b58_sha256_impl == NULL) {
        b58_sha256_impl = base58_sha256;
    }
}

static esp_err_t decode_segwit_address(const char *address, uint8_t *script, size_t script_cap, size_t *script_len)
{
    static const char *hrps[] = { "bc", "tb", "bcrt" };
    uint8_t program[40];
    size_t program_len = 0;
    int version = 0;

    for (size_t i = 0; i < sizeof(hrps) / sizeof(hrps[0]); i++) {
        program_len = sizeof(program);
        if (!segwit_addr_decode(&version, program, &program_len, hrps[i], address)) {
            continue;
        }

        if (version < 0 || version > 16 || program_len < 2 || program_len > 40) {
            return ESP_ERR_INVALID_ARG;
        }
        // Version 0 must be 20 bytes (P2WPKH) or 32 bytes (P2WSH)
        if (version == 0 && program_len != 20 && program_len != 32) {
            return ESP_ERR_INVALID_ARG;
        }
        // Version 1 (Taproot) must be 32 bytes (P2TR)
        if (version == 1 && program_len != 32) {
            return ESP_ERR_INVALID_ARG;
        }
        if (script_cap < program_len + 2) {
            return ESP_ERR_INVALID_SIZE;
        }

        script[0] = version == 0 ? OP_0 : (uint8_t)(OP_1 + version - 1);
        script[1] = (uint8_t)program_len;
        memcpy(script + 2, program, program_len);
        *script_len = program_len + 2;
        return ESP_OK;
    }

    return ESP_ERR_INVALID_ARG;
}

static esp_err_t decode_base58_address(const char *address, uint8_t *script, size_t script_cap, size_t *script_len)
{
    uint8_t decoded[25];
    size_t decoded_len = sizeof(decoded);

    ensure_base58_initialized();
    if (!b58tobin(decoded, &decoded_len, address, 0) || decoded_len != sizeof(decoded)) {
        return ESP_ERR_INVALID_ARG;
    }

    int version = b58check(decoded, decoded_len, address, 0);
    if (version < 0) {
        return ESP_ERR_INVALID_CRC;
    }

    const uint8_t *hash160 = decoded + 1;
    // P2PKH (0x00 mainnet, 0x6f testnet/regtest)
    if (version == 0x00 || version == 0x6f) {
        if (script_cap < 25) {
            return ESP_ERR_INVALID_SIZE;
        }
        script[0] = OP_DUP;
        script[1] = OP_HASH160;
        script[2] = OP_PUSHDATA_20;
        memcpy(script + 3, hash160, 20);
        script[23] = OP_EQUALVERIFY;
        script[24] = OP_CHECKSIG;
        *script_len = 25;
        return ESP_OK;
    }

    // P2SH (0x05 mainnet, 0xc4 testnet/regtest)
    if (version == 0x05 || version == 0xc4) {
        if (script_cap < 23) {
            return ESP_ERR_INVALID_SIZE;
        }
        script[0] = OP_HASH160;
        script[1] = OP_PUSHDATA_20;
        memcpy(script + 2, hash160, 20);
        script[22] = OP_EQUAL;
        *script_len = 23;
        return ESP_OK;
    }

    return ESP_ERR_NOT_SUPPORTED;
}

bool coinbase_validate_address(const char *address)
{
    if (!address || address[0] == '\0') {
        return false;
    }
    uint8_t script[COINBASE_MAX_SCRIPT_PUBKEY_LEN];
    size_t script_len = 0;
    return coinbase_address_to_script(address, script, sizeof(script), &script_len) == ESP_OK;
}

esp_err_t coinbase_address_to_script(const char *address,
                                     uint8_t *script,
                                     size_t script_cap,
                                     size_t *script_len)
{
    if (!address || !address[0] || !script || !script_len) {
        return ESP_ERR_INVALID_ARG;
    }
    *script_len = 0;

    esp_err_t err = decode_segwit_address(address, script, script_cap, script_len);
    if (err == ESP_OK) {
        return ESP_OK;
    }

    return decode_base58_address(address, script, script_cap, script_len);
}

static esp_err_t build_bip34_height_script(uint32_t height, uint8_t *height_script, size_t *height_script_len)
{
    if (!height_script || !height_script_len) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t encoded[5];
    size_t len = 0;
    uint32_t value = height;
    while (value > 0) {
        encoded[len++] = value & 0xff;
        value >>= 8;
    }
    if (len == 0) {
        encoded[len++] = 0;
    }
    if (encoded[len - 1] & 0x80) {
        encoded[len++] = 0;
    }
    if (len > 5) {
        return ESP_ERR_INVALID_ARG;
    }

    height_script[0] = (uint8_t)len;
    memcpy(height_script + 1, encoded, len);
    *height_script_len = len + 1;
    return ESP_OK;
}

esp_err_t coinbase_build_scriptsig(uint32_t height,
                                  const uint8_t *extranonce,
                                  size_t extranonce_len,
                                  const char *tag,
                                  uint8_t *scriptsig,
                                  size_t *scriptsig_len)
{
    if (!scriptsig || !scriptsig_len || extranonce_len > 16) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *effective_tag = (tag && tag[0] != '\0') ? tag : COINBASE_DEFAULT_TAG;
    size_t tag_len = strlen(effective_tag);
    if (tag_len > COINBASE_MAX_TAG_LEN) {
        tag_len = COINBASE_MAX_TAG_LEN;
    }

    uint8_t height_script[6];
    size_t height_script_len = 0;
    ESP_RETURN_ON_ERROR(build_bip34_height_script(height, height_script, &height_script_len),
                        TAG, "build BIP34 height script");

    size_t pos = 0;

    // 1. BIP 34 height push
    memcpy(scriptsig + pos, height_script, height_script_len);
    pos += height_script_len;

    // 2. Miner tag push (opcode + data)
    scriptsig[pos++] = (uint8_t)tag_len;
    memcpy(scriptsig + pos, effective_tag, tag_len);
    pos += tag_len;

    // 3. Extranonce push (opcode + data)
    if (extranonce_len > 0) {
        scriptsig[pos++] = (uint8_t)extranonce_len;
        if (extranonce) {
            memcpy(scriptsig + pos, extranonce, extranonce_len);
        } else {
            memset(scriptsig + pos, 0, extranonce_len);
        }
        pos += extranonce_len;
    }

    // Bitcoin consensus constraint: 2 <= scriptsig_len <= 100
    if (pos < 2 || pos > COINBASE_MAX_SCRIPTSIG_LEN) {
        ESP_LOGE(TAG, "Coinbase scriptSig len %zu exceeds bounds (2..100)", pos);
        return ESP_ERR_INVALID_SIZE;
    }

    *scriptsig_len = pos;
    return ESP_OK;
}

static size_t put_varint(uint8_t *buf, uint64_t val)
{
    if (val < 0xfd) {
        if (buf) buf[0] = (uint8_t)val;
        return 1;
    }
    if (val <= 0xffff) {
        if (buf) {
            buf[0] = 0xfd;
            buf[1] = val & 0xff;
            buf[2] = (val >> 8) & 0xff;
        }
        return 3;
    }
    if (val <= 0xffffffff) {
        if (buf) {
            buf[0] = 0xfe;
            buf[1] = val & 0xff;
            buf[2] = (val >> 8) & 0xff;
            buf[3] = (val >> 16) & 0xff;
            buf[4] = (val >> 24) & 0xff;
        }
        return 5;
    }
    if (buf) {
        buf[0] = 0xff;
        for (int i = 0; i < 8; i++) {
            buf[1 + i] = (val >> (i * 8)) & 0xff;
        }
    }
    return 9;
}

static size_t serialize_coinbase_internal(uint32_t height,
                                         uint64_t coinbase_value,
                                         const uint8_t *payout_script,
                                         size_t payout_script_len,
                                         const uint8_t *commitment_script,
                                         size_t commitment_script_len,
                                         const uint8_t *extranonce,
                                         size_t extranonce_len,
                                         const char *tag,
                                         bool include_witness,
                                         uint8_t *out,
                                         size_t out_cap)
{
    uint8_t scriptsig[COINBASE_MAX_SCRIPTSIG_LEN];
    size_t scriptsig_len = 0;
    if (coinbase_build_scriptsig(height, extranonce, extranonce_len, tag, scriptsig, &scriptsig_len) != ESP_OK) {
        return 0;
    }

    size_t pos = 0;
    uint8_t zero_hash[32] = { 0 };

    // Version (uint32 LE)
    if (out && pos + 4 <= out_cap) {
        out[pos + 0] = COINBASE_VERSION & 0xff;
        out[pos + 1] = (COINBASE_VERSION >> 8) & 0xff;
        out[pos + 2] = (COINBASE_VERSION >> 16) & 0xff;
        out[pos + 3] = (COINBASE_VERSION >> 24) & 0xff;
    }
    pos += 4;

    // Witness marker and flag (0x00, 0x01)
    if (include_witness) {
        if (out && pos + 2 <= out_cap) {
            out[pos + 0] = 0x00;
            out[pos + 1] = 0x01;
        }
        pos += 2;
    }

    // Input count: 1
    pos += put_varint(out ? out + pos : NULL, 1);

    // Prevout: 32 zero bytes
    if (out && pos + 32 <= out_cap) {
        memcpy(out + pos, zero_hash, 32);
    }
    pos += 32;

    // Prevout index: 0xffffffff
    if (out && pos + 4 <= out_cap) {
        memset(out + pos, 0xff, 4);
    }
    pos += 4;

    // scriptSig length + bytes
    pos += put_varint(out ? out + pos : NULL, scriptsig_len);
    if (out && pos + scriptsig_len <= out_cap) {
        memcpy(out + pos, scriptsig, scriptsig_len);
    }
    pos += scriptsig_len;

    // Sequence: 0xffffffff
    if (out && pos + 4 <= out_cap) {
        memset(out + pos, 0xff, 4);
    }
    pos += 4;

    // Output count: 1 or 2
    uint64_t output_count = (commitment_script_len > 0) ? 2 : 1;
    pos += put_varint(out ? out + pos : NULL, output_count);

    // Output 1: Payout value + script
    if (out && pos + 8 <= out_cap) {
        for (int i = 0; i < 8; i++) {
            out[pos + i] = (coinbase_value >> (i * 8)) & 0xff;
        }
    }
    pos += 8;

    pos += put_varint(out ? out + pos : NULL, payout_script_len);
    if (out && pos + payout_script_len <= out_cap) {
        memcpy(out + pos, payout_script, payout_script_len);
    }
    pos += payout_script_len;

    // Output 2: SegWit commitment (value = 0)
    if (commitment_script_len > 0) {
        if (out && pos + 8 <= out_cap) {
            memset(out + pos, 0, 8);
        }
        pos += 8;

        pos += put_varint(out ? out + pos : NULL, commitment_script_len);
        if (out && pos + commitment_script_len <= out_cap) {
            memcpy(out + pos, commitment_script, commitment_script_len);
        }
        pos += commitment_script_len;
    }

    // Witness stack (if include_witness)
    if (include_witness) {
        // 1 witness stack for input 0
        pos += put_varint(out ? out + pos : NULL, 1); // 1 stack item
        pos += put_varint(out ? out + pos : NULL, 32); // 32 bytes witness reserved value
        if (out && pos + 32 <= out_cap) {
            memset(out + pos, 0, 32);
        }
        pos += 32;
    }

    // Locktime: 0 (uint32 LE)
    if (out && pos + 4 <= out_cap) {
        memset(out + pos, 0, 4);
    }
    pos += 4;

    return pos;
}

esp_err_t coinbase_build_tx(uint32_t height,
                            uint64_t coinbase_value,
                            const char *payout_address,
                            const char *witness_commitment_hex,
                            const uint8_t *extranonce,
                            size_t extranonce_len,
                            const char *tag,
                            uint8_t *coinbase_tx,
                            size_t coinbase_tx_cap,
                            size_t *coinbase_tx_len,
                            uint8_t coinbase_txid[32])
{
    if (!payout_address || !coinbase_tx || !coinbase_tx_len || !coinbase_txid) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t payout_script[COINBASE_MAX_SCRIPT_PUBKEY_LEN];
    size_t payout_script_len = 0;
    ESP_RETURN_ON_ERROR(coinbase_address_to_script(payout_address, payout_script, sizeof(payout_script), &payout_script_len),
                        TAG, "parse payout address");

    uint8_t commitment_script[COINBASE_MAX_SCRIPT_PUBKEY_LEN];
    size_t commitment_script_len = 0;
    if (witness_commitment_hex && witness_commitment_hex[0] != '\0') {
        size_t hex_len = strlen(witness_commitment_hex);
        commitment_script_len = hex_len / 2;
        if (commitment_script_len <= sizeof(commitment_script)) {
            hex2bin(witness_commitment_hex, commitment_script, commitment_script_len);
        } else {
            commitment_script_len = 0;
        }
    }

    bool has_witness = (commitment_script_len > 0);

    // 1. Build witness coinbase tx (full transaction for block serialization)
    size_t full_len = serialize_coinbase_internal(height, coinbase_value,
                                                  payout_script, payout_script_len,
                                                  commitment_script, commitment_script_len,
                                                  extranonce, extranonce_len, tag,
                                                  has_witness, coinbase_tx, coinbase_tx_cap);
    if (full_len == 0 || full_len > coinbase_tx_cap) {
        return ESP_ERR_NO_MEM;
    }
    *coinbase_tx_len = full_len;

    // 2. Build legacy coinbase tx (to compute coinbase_txid without witness data)
    uint8_t legacy_buf[COINBASE_MAX_TX_LEN];
    size_t legacy_len = serialize_coinbase_internal(height, coinbase_value,
                                                    payout_script, payout_script_len,
                                                    commitment_script, commitment_script_len,
                                                    extranonce, extranonce_len, tag,
                                                    false, legacy_buf, sizeof(legacy_buf));
    if (legacy_len == 0) {
        return ESP_FAIL;
    }

    // Double SHA-256 of legacy serialization gives txid
    double_sha256_bin(legacy_buf, legacy_len, coinbase_txid);
    return ESP_OK;
}

esp_err_t coinbase_compute_merkle_path(const uint8_t coinbase_txid[32],
                                       const uint8_t *txid_hashes,
                                       size_t tx_count,
                                       uint8_t merkle_path[COINBASE_MAX_MERKLE_BRANCHES][32],
                                       size_t *merkle_path_count,
                                       uint8_t merkle_root[32])
{
    if (!coinbase_txid || (!txid_hashes && tx_count > 0) || !merkle_path || !merkle_path_count || !merkle_root) {
        return ESP_ERR_INVALID_ARG;
    }

    *merkle_path_count = 0;

    if (tx_count == 0) {
        memcpy(merkle_root, coinbase_txid, 32);
        return ESP_OK;
    }

    size_t leaf_count = tx_count + 1;
    uint8_t *level = NULL;

#if CONFIG_SPIRAM
    if (esp_psram_is_initialized()) {
        level = (uint8_t *)heap_caps_malloc(leaf_count * 32, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
#endif
    if (!level) {
        level = (uint8_t *)malloc(leaf_count * 32);
    }
    if (!level) {
        return ESP_ERR_NO_MEM;
    }

    memcpy(level, coinbase_txid, 32);
    memcpy(level + 32, txid_hashes, tx_count * 32);

    size_t count = leaf_count;
    size_t index = 0;
    size_t path_count = 0;

    while (count > 1) {
        if (path_count >= COINBASE_MAX_MERKLE_BRANCHES) {
            free(level);
            return ESP_ERR_INVALID_SIZE;
        }

        size_t sibling = (index ^ 1);
        if (sibling >= count) {
            sibling = index;
        }
        memcpy(merkle_path[path_count], level + sibling * 32, 32);
        path_count++;

        size_t next_count = 0;
        for (size_t i = 0; i < count; i += 2) {
            uint8_t pair[64];
            size_t right = (i + 1 < count) ? (i + 1) : i;
            memcpy(pair, level + i * 32, 32);
            memcpy(pair + 32, level + right * 32, 32);
            double_sha256_bin(pair, sizeof(pair), level + next_count * 32);
            next_count++;
        }

        index /= 2;
        count = next_count;
    }

    memcpy(merkle_root, level, 32);
    free(level);

    *merkle_path_count = path_count;
    return ESP_OK;
}

void coinbase_update_merkle_root(const uint8_t new_coinbase_txid[32],
                                 const uint8_t merkle_path[COINBASE_MAX_MERKLE_BRANCHES][32],
                                 size_t merkle_path_count,
                                 uint8_t new_merkle_root[32])
{
    if (!new_coinbase_txid || !new_merkle_root) {
        return;
    }

    memcpy(new_merkle_root, new_coinbase_txid, 32);
    for (size_t i = 0; i < merkle_path_count; i++) {
        uint8_t pair[64];
        memcpy(pair, new_merkle_root, 32);
        memcpy(pair + 32, merkle_path[i], 32);
        double_sha256_bin(pair, sizeof(pair), new_merkle_root);
    }
}
