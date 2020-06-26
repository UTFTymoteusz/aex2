#pragma once

constexpr auto PS2_IO_DATA    = 0x60;
constexpr auto PS2_IO_COMMAND = 0x64;
constexpr auto PS2_IO_STATUS  = PS2_IO_COMMAND;

constexpr auto PS2_CTRL_CMD_READ_CFG_BYTE  = 0x20;
constexpr auto PS2_CTRL_CMD_WRITE_CFG_BYTE = 0x60;

constexpr auto PS2_CTRL_CMD_DISABLE_PORT1 = 0xA7;
constexpr auto PS2_CTRL_CMD_ENABLE_PORT1  = 0xA8;

constexpr auto PS2_CTRL_CMD_TEST = 0xAA;

constexpr auto PS2_CTRL_CMD_DISABLE_PORT0 = 0xAD;
constexpr auto PS2_CTRL_CMD_ENABLE_PORT0  = 0xAE;

constexpr auto PS2_STATUS_OUTPUT_FULL  = 1 << 0;
constexpr auto PS2_STATUS_INPUT_FULL   = 1 << 1;
constexpr auto PS2_STATUS_SYSTEM_FLAG  = 1 << 2;
constexpr auto PS2_STATUS_COMMAND_DATA = 1 << 3;