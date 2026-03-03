# BM13xx Unified Refactoring Plan

This plan covers refactoring for all BM13xx ASIC driver files:
- `bm1366.c`
- `bm1368.c`
- `bm1370.c`
- `bm1397.c`

## Current State Analysis

All four files have nearly identical code patterns with the same issues:
1. **Repetitive byte manipulation** - Manual `& 0xFF` masking and bit shifting
2. **Inconsistent array initialization** - Mix of compound literals, pre-initialized arrays
3. **Magic numbers** - Register addresses hardcoded without clear naming
4. **No abstraction for register operations**

---

## Common Protocol Pattern

All chips use the same 6-byte command structure:
```
{ chip_addr (1B), register_addr (1B), data0 (1B), data1 (1B), data2 (1B), data3 (1B) }
```

Header: `TYPE_CMD | GROUP_SINGLE/GROUP_ALL | CMD_WRITE/CMD_READ/CMD_SETADDRESS/CMD_INACTIVE`

---

## BM1370 (Completed Analysis)

### Call Patterns Found:
- **Static Register Writes**: 11 instances
- **Per-Chip Writes**: 5 instances (in loop)
- **Register Reads**: 2 instances
- **Dynamic Data**: 4 functions with computed values
- **Job Packets**: 1 instance

### Functions with Byte Masking:
| Function | Line | Issue |
|----------|------|-------|
| `BM1370_set_hash_counting_number` | 145 | `(hcn >> 24) & 0xFF` etc - UNNECESSARY |
| `BM1370_set_version_mask` | 136 | `(versions_to_roll >> 8)` - UNNECESSARY |
| `_send_BM1370` internal | 110 | CRC masking - REQUIRED |

---

## BM1366 (Analysis)

### Call Patterns Found:
- **Static Register Writes** (via `_send_simple`): 9 instances
- **Static Register Writes** (via `_send_BM1366`): 3 instances
- **Per-Chip Writes**: 5 instances (in loop)
- **Register Reads**: 1 instance
- **Dynamic Data**: 4 functions with computed values

### Functions with Byte Masking:
| Function | Line | Issue |
|----------|------|-------|
| `BM1366_set_hash_counting_number` | 150 | `(hcn >> 24) & 0xFF` etc - UNNECESSARY |
| `BM1366_set_version_mask` | 141 | `(versions_to_roll >> 8)` - UNNECESSARY |
| `_send_BM1366` internal | 108 | CRC masking - REQUIRED |

---

## BM1368 (Analysis)

### Call Patterns Found:
- **Static Register Writes**: 10 instances (using init_cmds array)
- **Per-Chip Writes**: 5 instances per chip (in loop)
- **Register Reads**: 1 instance
- **Dynamic Data**: 4 functions with computed values

### Functions with Byte Masking:
| Function | Line | Issue |
|----------|------|-------|
| `BM1368_set_hash_counting_number` | 127 | `(hcn >> 24) & 0xFF` etc - UNNECESSARY |
| `BM1368_set_version_mask` | 118 | `(versions_to_roll >> 8)` - UNNECESSARY |
| `_send_BM1368` internal | 96 | CRC masking - REQUIRED |

---

## BM1397 (Analysis)

### Call Patterns Found:
- **Static Register Writes**: 10 instances
- **Per-Chip Writes**: 0 instances
- **Register Reads**: 1 instance

### Functions with Byte Masking:
| Function | Line | Issue |
|----------|------|-------|
| `BM1397_set_hash_counting_number` | 152 | PLACEHOLDER (empty) |
| `BM1397_set_version_mask` | 148 | PLACEHOLDER (empty) |
| `_send_BM1397` internal | 113 | CRC masking - REQUIRED |

### Note: BM1397 already has some register constants defined:
```c
#define CLOCK_ORDER_CONTROL_0 0x80
#define CLOCK_ORDER_CONTROL_1 0x84
#define ORDERED_CLOCK_ENABLE 0x20
#define CORE_REGISTER_CONTROL 0x3C
#define PLL3_PARAMETER 0x68
#define FAST_UART_CONFIGURATION 0x28
#define MISC_CONTROL 0x18
```

---

## Proposed Unified Refactoring

### 1. Register Address Constants (per file)

Add to each file:
```c
// BM1370 specific
#define REG_HASH_COUNTING   0x10
#define REG_VERSION_MASK    0xA4
#define REG_A8              0xA8
#define REG_ANALOG_MUX      0x54
#define REG_IO_DRIVER       0x58
#define REG_B9              0xB9
```

### 2. Helper Functions (per file)

```c
// Write 32-bit value to register (big-endian)
static void _write_reg_32(uint8_t header, uint8_t chip_addr, uint8_t reg, uint32_t value) {
    uint8_t cmd[6] = {chip_addr, reg};
    uint32_t be_value = htonl(value);
    memcpy(&cmd[2], &be_value, 4);
    _send_BM1370(header, cmd, 6, BM1370_SERIALTX_DEBUG);  // adjust function name per file
}

// Write raw 4 bytes
static void _write_reg_bytes(uint8_t header, uint8_t chip_addr, uint8_t reg, 
                              uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
    uint8_t cmd[6] = {chip_addr, reg, b0, b1, b2, b3};
    _send_BM1370(header, cmd, 6, BM1370_SERIALTX_DEBUG);
}
```

### 3. Convenience Macros (per file)

```c
#define write_reg_all_32(reg, value) \
    _write_reg_32(TYPE_CMD | GROUP_ALL | CMD_WRITE, 0x00, reg, value)

#define write_reg_all_bytes(reg, b0, b1, b2, b3) \
    _write_reg_bytes(TYPE_CMD | GROUP_ALL | CMD_WRITE, 0x00, reg, b0, b1, b2, b3)

#define write_reg_single_bytes(chip, reg, b0, b1, b2, b3) \
    _write_reg_bytes(TYPE_CMD | GROUP_SINGLE | CMD_WRITE, chip, reg, b0, b1, b2, b3)
```

### 4. Refactored Functions (example for BM1370)

#### `BM1370_set_hash_counting_number`:
```c
// BEFORE (6 lines + masking):
void BM1370_set_hash_counting_number(uint32_t hcn) {
    uint8_t set_10_hash_counting[6] = {0x00, 0x10, 0x00, 0x00, 0x00, 0x00};
    set_10_hash_counting[2] = (hcn >> 24) & 0xFF;
    set_10_hash_counting[3] = (hcn >> 16) & 0xFF;
    set_10_hash_counting[4] = (hcn >> 8) & 0xFF;
    set_10_hash_counting[5] = hcn & 0xFF;
    _send_BM1370((TYPE_CMD | GROUP_ALL | CMD_WRITE), set_10_hash_counting, 6, BM1370_SERIALTX_DEBUG);
}

// AFTER (1 line, no masking):
void BM1370_set_hash_counting_number(uint32_t hcn) {
    write_reg_all_32(REG_HASH_COUNTING, hcn);
}
```

#### `BM1370_set_version_mask`:
```c
// BEFORE:
void BM1370_set_version_mask(uint32_t version_mask) {
    int versions_to_roll = version_mask >> 13;
    uint8_t version_byte0 = (versions_to_roll >> 8);
    uint8_t version_byte1 = (versions_to_roll & 0xFF); 
    uint8_t version_cmd[] = {0x00, 0xA4, 0x90, 0x00, version_byte0, version_byte1};
    _send_BM1370(TYPE_CMD | GROUP_ALL | CMD_WRITE, version_cmd, 6, BM1370_SERIALTX_DEBUG);
}

// AFTER:
void BM1370_set_version_mask(uint32_t version_mask) {
    uint16_t versions_to_roll = version_mask >> 13;
    write_reg_all_bytes(REG_VERSION_MASK, 0x90, 0x00, 
                       (versions_to_roll >> 8), versions_to_roll & 0xFF);
}
```

---

## Implementation Priority

1. **Phase 1**: Add register constants to all 4 files
2. **Phase 2**: Add helper functions to all 4 files
3. **Phase 3**: Refactor `*_set_hash_counting_number()` - eliminates most byte masking
4. **Phase 4**: Refactor `*_set_version_mask()` functions
5. **Phase 5**: Refactor `*_send_hash_frequency()` functions
6. **Phase 6**: Clean up init functions in `*_init()`

---

## Files to Modify

| File | Status |
|------|--------|
| `components/asic/bm1366.c` | To be refactored |
| `components/asic/bm1368.c` | To be refactored |
| `components/asic/bm1370.c` | To be refactored |
| `components/asic/bm1397.c` | To be refactored |
