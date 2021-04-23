//
// Created by seba on 4/21/21.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_LINKINFO_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_LINKINFO_HPP

#include <MacId.hpp>
#include <ostream>

namespace TUHH_INTAIRNET_MCSOTDMA {

	/**
	 * Encodes a point-to-point link.
	 */
	class LinkInfo {
	public:
		/**
		 * @param tx_id Identifier of the initiator of a link.
		 * @param rx_id Identifier of the recipient of a link.
		 * @param p2p_channel_center_freq Identifier of the frequency channel.
		 * @param offset Offset to the beginning of the next transmission burst.
		 * @param timeout Number of remaining bursts until the link expires.
		 * @param burst_length Number of slots a burst occupies.
		 * @param burst_length_tx Number of slots the link initiator will transmit.
		 */
		LinkInfo(const MacId& tx_id, const MacId& rx_id, uint64_t p2p_channel_center_freq, int offset, unsigned int timeout, unsigned int burst_length, unsigned int burst_length_tx);
		LinkInfo();
		LinkInfo(const LinkInfo &other);

		/**
		 * @return Identifier of the initiator of a link.
		 */
		const MacId& getTxId() const;

		/**
		 * @return Identifier of the recipient of a link.
		 */
		const MacId& getRxId() const;

		/**
		 * @return Identifier of the frequency channel.
		 */
		uint64_t getP2PChannelCenterFreq() const;

		/**
		 * @return Offset to the beginning of the next transmission burst.
		 */
		int getOffset() const;

		/**
		 * @return Number of remaining bursts until the link expires.
		 */
		unsigned int getTimeout() const;

		/**
		 * @return Number of slots a burst occupies.
		 */
		unsigned int getBurstLength() const;

		/**
		 * @return Number of slots the link initiator will transmit.
		 */
		unsigned int getBurstLengthTx() const;

		unsigned int getBits() const;

	protected:
		/** Identifier of the initiator of a link. */
		MacId tx_id;
		/** Identifier of the recipient of a link. */
		MacId rx_id;
		/** Identifier of the frequency channel. */
		uint64_t p2p_channel_center_freq;
		/** Offset to the beginning of the next transmission burst. */
		int offset;
		/** Number of remaining bursts until the link expires. */
		unsigned int timeout;
		/** Number of slots a burst occupies. */
		unsigned int burst_length;
		/** Number of slots the link initiator will transmit. */
		unsigned int burst_length_tx;
	};

	inline std::ostream& operator<<(std::ostream& stream, const LinkInfo& info) {
		return stream << "f=" << info.getP2PChannelCenterFreq() << " " << info.getTxId() << "<->" << info.getRxId() << "@" << info.getOffset() << ":" << info.getBurstLengthTx() << ":" << info.getBurstLength() << "x" << info.getTimeout();
	}
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_LINKINFO_HPP
