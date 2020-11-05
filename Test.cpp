/*
    This file is part of SX128x Linux driver.
    Copyright (C) 2020 ReimuNotMoe

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "SX128x_Linux.hpp"

#include <cstdio>
#include <cstring>

int main(int argc, char **argv) {
	if (argc < 4) {
		printf("Usage: SX128x_Test <tx|rx> <lora|flrc> <freq in MHz>\n");
		return 1;
	}

	int modmode = 0;

	if (strcmp(argv[2], "flrc") == 0) {
		modmode = 1;
	}

	// Customize these pins by yourself
	SX128x_Linux Radio("/dev/spidev0.0", 0,
			   {
				   27, 17, 5,
				   22, -1, -1,
				   4, 6
			   }
	);

	// Assume we're running on a high-end Raspberry Pi,
	// so we set the SPI clock speed to the maximum value supported by the chip
	Radio.SetSpiSpeed(8000000);

	Radio.Init();
	puts("Init done");
	Radio.SetStandby(SX128x::STDBY_XOSC);
	puts("SetStandby done");
	Radio.SetRegulatorMode(static_cast<SX128x::RadioRegulatorModes_t>(0));
	puts("SetRegulatorMode done");
	Radio.SetLNAGainSetting(SX128x::LNA_HIGH_SENSITIVITY_MODE);
	puts("SetLNAGainSetting done");
	Radio.SetTxParams(0, SX128x::RADIO_RAMP_20_US);
	puts("SetTxParams done");

	Radio.SetBufferBaseAddresses(0x00, 0x00);
	puts("SetBufferBaseAddresses done");


	SX128x::ModulationParams_t ModulationParams;
	SX128x::PacketParams_t PacketParams;

	if (modmode == 0) {
		ModulationParams.PacketType = SX128x::PACKET_TYPE_LORA;
		ModulationParams.Params.LoRa.CodingRate = SX128x::LORA_CR_4_8;
		ModulationParams.Params.LoRa.Bandwidth = SX128x::LORA_BW_1600;
		ModulationParams.Params.LoRa.SpreadingFactor = SX128x::LORA_SF7;

		PacketParams.PacketType = SX128x::PACKET_TYPE_LORA;
		auto &l = PacketParams.Params.LoRa;
		l.PayloadLength = 253;
		l.HeaderType = SX128x::LORA_PACKET_FIXED_LENGTH;
		l.PreambleLength = 12;
		l.Crc = SX128x::LORA_CRC_ON;
		l.InvertIQ = SX128x::LORA_IQ_NORMAL;

		Radio.SetPacketType(SX128x::PACKET_TYPE_LORA);
	} else {
		ModulationParams.PacketType = SX128x::PACKET_TYPE_FLRC;
		auto &p = ModulationParams.Params.Flrc;
		p.CodingRate = SX128x::FLRC_CR_1_2;
		p.BitrateBandwidth = SX128x::FLRC_BR_0_325_BW_0_3;
		p.ModulationShaping = SX128x::RADIO_MOD_SHAPING_BT_OFF;


		PacketParams.PacketType = SX128x::PACKET_TYPE_FLRC;
		auto &l = PacketParams.Params.Flrc;
		l.PayloadLength = 127;
		l.HeaderType = SX128x::RADIO_PACKET_VARIABLE_LENGTH;
		l.PreambleLength = SX128x::PREAMBLE_LENGTH_32_BITS;
		l.CrcLength = SX128x::RADIO_CRC_OFF;
		l.SyncWordLength = SX128x::FLRC_SYNCWORD_LENGTH_4_BYTE;
		l.SyncWordMatch = SX128x::RADIO_RX_MATCH_SYNCWORD_1;
		l.Whitening = SX128x::RADIO_WHITENING_OFF;

		Radio.SetPacketType(SX128x::PACKET_TYPE_FLRC);
	}

	puts("SetPacketType done");
	Radio.SetModulationParams(ModulationParams);
	puts("SetModulationParams done");
	Radio.SetPacketParams(PacketParams);
	puts("SetPacketParams done");

	auto freq = strtol(argv[3], nullptr, 10);
	Radio.SetRfFrequency(freq * 1000000UL);
	puts("SetRfFrequency done");

	if (modmode == 1) {
		// only used in GFSK, FLRC (4 bytes max) and BLE mode
		uint8_t sw[] = {0xDD, 0xA0, 0x96, 0x69, 0xDD};
		Radio.SetSyncWord(1, sw);
		// only used in GFSK, FLRC
		uint8_t crcSeedLocal[2] = {0x45, 0x67};
		Radio.SetCrcSeed(crcSeedLocal);
		Radio.SetCrcPolynomial(0x0123);
//		Radio.SetWhiteningSeed(0x22);
	}

	std::cout << Radio.GetFirmwareVersion() << "\n";

	Radio.callbacks.txDone = []{
		puts("Wow TX done");
	};

	size_t pkt_count = 0;

	Radio.callbacks.rxDone = [&] {
		puts("Wow RX done");


		SX128x::PacketStatus_t ps;
		Radio.GetPacketStatus(&ps);

		uint8_t recv_buf[253];
		uint8_t rsz;
		Radio.GetPayload(recv_buf, &rsz, 253);

		uint8_t err_count = 0;

		for (size_t i=0; i<rsz; i++) {
			uint8_t correct_value;
			if (i % 2)
				correct_value = 0x55;
			else
				correct_value = 0xaa;

			if (recv_buf[i] != correct_value)
				err_count++;
		}

//		for (size_t i=0; i<rsz; i++) {
//			printf("%02x ", recv_buf[i]);
//		}
//
//		puts("");

		pkt_count++;
		printf("Packet count: %ld\n", pkt_count);

		printf("corrupted bytes: %u/%u, BER: %f%%\n", err_count, rsz, (double)err_count/rsz*100);

		if (ps.packetType == SX128x::PACKET_TYPE_LORA) {
			int8_t noise = ps.LoRa.RssiPkt - ps.LoRa.SnrPkt;
			int8_t rscp = ps.LoRa.RssiPkt + ps.LoRa.SnrPkt;
			printf("recvd %u bytes, RSCP: %d, RSSI: %d, Noise: %d, SNR: %d\n", rsz, rscp, ps.LoRa.RssiPkt, noise, ps.LoRa.SnrPkt);
		} else if (ps.packetType == SX128x::PACKET_TYPE_FLRC) {
			printf("recvd %u bytes, RSSI: %d\n", rsz, ps.Flrc.RssiSync);

		}
	};

	auto IrqMask = SX128x::IRQ_RX_DONE | SX128x::IRQ_TX_DONE | SX128x::IRQ_RX_TX_TIMEOUT;
	Radio.SetDioIrqParams(IrqMask, IrqMask, SX128x::IRQ_RADIO_NONE, SX128x::IRQ_RADIO_NONE);
	puts("SetDioIrqParams done");

	Radio.StartIrqHandler();
	puts("StartIrqHandler done");


	auto pkt_ToA = Radio.GetTimeOnAir();

	if (strcmp(argv[1], "tx") == 0) {
		uint8_t buf[253];

		for (size_t i=0; i<sizeof(buf); i++) {
			if (i % 2)
				buf[i] = 0x55;
			else
				buf[i] = 0xaa;
		}

		while (1) {


			Radio.SendPayload(buf, modmode == 0 ? 253 : 127, {SX128x::RADIO_TICK_SIZE_1000_US, 1000});
			puts("SendPayload done");

			usleep((pkt_ToA + 20) * 1000);
		}
	} else {
		Radio.SetRx({SX128x::RADIO_TICK_SIZE_1000_US, 0xFFFF});
		puts("SetRx done");

		while (1) {
			sleep(1);
		}
	}

}