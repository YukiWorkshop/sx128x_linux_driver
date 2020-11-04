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
	if (argc < 3) {
		printf("Usage: SX128x_Test <tx|rx> <freq in MHz>\n");
		return 1;
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

	SX128x::ModulationParams_t ModulationParams2;
	ModulationParams2.PacketType = SX128x::PACKET_TYPE_LORA;
	ModulationParams2.Params.LoRa.CodingRate = SX128x::LORA_CR_4_5;
	ModulationParams2.Params.LoRa.Bandwidth = SX128x::LORA_BW_1600;
	ModulationParams2.Params.LoRa.SpreadingFactor = SX128x::LORA_SF10;

	SX128x::PacketParams_t PacketParams2;
	PacketParams2.PacketType = SX128x::PACKET_TYPE_LORA;
	auto &l = PacketParams2.Params.LoRa;
	l.PayloadLength = 32;
	l.HeaderType = SX128x::LORA_PACKET_FIXED_LENGTH;
	l.PreambleLength = 12;
	l.Crc = SX128x::LORA_CRC_ON;
	l.InvertIQ = SX128x::LORA_IQ_NORMAL;

	Radio.SetPacketType(SX128x::PACKET_TYPE_LORA);
	puts("SetPacketType done");
	Radio.SetModulationParams(ModulationParams2);
	puts("SetModulationParams done");
	Radio.SetPacketParams(PacketParams2);
	puts("SetPacketParams done");

	auto freq = strtol(argv[2], nullptr, 10);
	Radio.SetRfFrequency(freq * 1000000UL);
	puts("SetRfFrequency done");

	// only used in GFSK, FLRC (4 bytes max) and BLE mode
	uint8_t sw[] = { 0xDD, 0xA0, 0x96, 0x69, 0xDD };
	Radio.SetSyncWord( 1, sw);
	// only used in GFSK, FLRC
	uint8_t crcSeedLocal[2] = {0x45, 0x67};
	Radio.SetCrcSeed( crcSeedLocal );
	Radio.SetCrcPolynomial( 0x0123 );

	std::cout << Radio.GetFirmwareVersion() << "\n";

	Radio.callbacks.txDone = []{
		puts("Wow TX done");
	};

	Radio.callbacks.rxDone = [&] {
		puts("Wow RX done");


		SX128x::PacketStatus_t ps;
		Radio.GetPacketStatus(&ps);

		uint8_t recv_buf[253];
		uint8_t rsz;
		Radio.GetPayload(recv_buf, &rsz, 253);

		write(STDOUT_FILENO, recv_buf, 32);

		int8_t noise = ps.LoRa.RssiPkt - ps.LoRa.SnrPkt;
		int8_t rscp = ps.LoRa.RssiPkt + ps.LoRa.SnrPkt;
		printf("recvd %u bytes, RSCP: %d, RSSI: %d, Noise: %d, SNR: %d\n", rsz, rscp, ps.LoRa.RssiPkt, noise, ps.LoRa.SnrPkt);
	};

	auto IrqMask = SX128x::IRQ_RX_DONE | SX128x::IRQ_TX_DONE | SX128x::IRQ_RX_TX_TIMEOUT;
	Radio.SetDioIrqParams(IrqMask, IrqMask, SX128x::IRQ_RADIO_NONE, SX128x::IRQ_RADIO_NONE);
	puts("SetDioIrqParams done");

	Radio.StartIrqHandler();
	puts("StartIrqHandler done");


	auto pkt_ToA = Radio.GetTimeOnAir();

	if (strcmp(argv[1], "tx") == 0) {

		while (1) {
			char buf[253] = "12345678abcdefgh12345678abcdefg\n";

			Radio.SendPayload((uint8_t *) buf, 32, {SX128x::RADIO_TICK_SIZE_1000_US, 1000});
			puts("SendPayload done");

			usleep(pkt_ToA * 1000 + 50);
		}
	} else {
		Radio.SetRx({SX128x::RADIO_TICK_SIZE_1000_US, 0xFFFF});
		puts("SetRx done");

		while (1) {
			sleep(1);
		}
	}

}