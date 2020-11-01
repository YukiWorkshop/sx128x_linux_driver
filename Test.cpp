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

int main() {
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
	ModulationParams2.Params.LoRa.SpreadingFactor = SX128x::LORA_SF8;

	SX128x::PacketParams_t PacketParams2;
	PacketParams2.PacketType = SX128x::PACKET_TYPE_LORA;
	auto &l = PacketParams2.Params.LoRa;
	l.PayloadLength = 253;
	l.HeaderType = SX128x::LORA_PACKET_FIXED_LENGTH;
	l.PreambleLength = 12;
	l.Crc = SX128x::LORA_CRC_ON;
	l.InvertIQ = SX128x::LORA_IQ_NORMAL;

	Radio.SetPacketType(SX128x::PACKET_TYPE_LORA);
	puts("SetPacketType done");
	Radio.SetModulationParams(&ModulationParams2);
	puts("SetModulationParams done");
	Radio.SetPacketParams(&PacketParams2);
	puts("SetPacketParams done");

	Radio.SetRfFrequency(2486 * 1000000UL);
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

	auto IrqMask = SX128x::IRQ_RX_DONE | SX128x::IRQ_TX_DONE | SX128x::IRQ_RX_TX_TIMEOUT;
	Radio.SetDioIrqParams(IrqMask, IrqMask, SX128x::IRQ_RADIO_NONE, SX128x::IRQ_RADIO_NONE);
	puts("SetDioIrqParams done");

	Radio.StartIrqHandler();
	puts("StartIrqHandler done");


	while (1) {
		char buf[253] = "12345678\n";

		Radio.SendPayload((uint8_t *) buf, 64, {SX128x::RADIO_TICK_SIZE_1000_US, 1000});

		puts("SendPayload done");
		usleep(200 * 1000);
	}

}