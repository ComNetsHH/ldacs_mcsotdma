//
// Created by Sebastian Lindner on 18.11.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../LinkManager.hpp"
#include "../MCSOTDMA_Mac.hpp"
#include "../coutdebug.hpp"
#include <IArq.hpp>
#include <IRlc.hpp>
#include <IPhy.hpp>

namespace TUHH_INTAIRNET_MCSOTDMA {
	class LinkManagerTests : public CppUnit::TestFixture {
		private:
			LinkManager* link_manager;
			ReservationManager* reservation_manager;
			MacId id = MacId(0);
			uint32_t planning_horizon = 128;
			uint64_t center_frequency1 = 1000, center_frequency2 = 2000, center_frequency3 = 3000, bc_frequency = 4000, bandwidth = 500;
			unsigned long num_bits_going_out = 1024;
			
			class MACLayer : public MCSOTDMA_Mac {
				public:
					explicit MACLayer(ReservationManager* manager) : MCSOTDMA_Mac(manager) {}
				
				protected:
					void onReceptionSlot(const FrequencyChannel* channel) override {
						// do nothing.
					}
			};
			MACLayer* mac;
			
			class ARQLayer : public IArq {
				public:
					void notifyOutgoing(unsigned int num_bits, const MacId& mac_id) override {
						throw std::runtime_error("not implemented");
					}
					
					L2Packet* requestSegment(unsigned int num_bits, const MacId& mac_id) override {
						throw std::runtime_error("not implemented");
					}
					
					bool shouldLinkBeArqProtected(const MacId& mac_id) const override {
						return true;
					}
					
					void receiveFromLower(L2Packet* packet) override {
					
					}
					
					void notifyAboutNewLink(const MacId& id) override {
					
					}
					
					void notifyAboutRemovedLink(const MacId& id) override {
					
					}
				
				protected:
					void processIncomingHeader(L2Packet* incoming_packet) override {
					
					}
			};
			ARQLayer* arq_layer;
			
			class RLCLayer : public IRlc {
				public:
					virtual ~RLCLayer() {
						for (auto* packet : injections)
							delete packet;
					}
					
					void receiveFromUpper(L3Packet* data, MacId dest, PacketPriority priority) override {
					
					}
					
					void receiveInjectionFromLower(L2Packet* packet, PacketPriority priority) override {
						injections.push_back(packet);
					}
					
					L2Packet* requestSegment(unsigned int num_bits, const MacId& mac_id) override {
						throw std::runtime_error("not implemented");
					}
					
					void receiveFromLower(L2Packet* packet) override {
					
					}
					
					std::vector<L2Packet*> injections;
			};
			RLCLayer* rlc_layer;
			
			class PHYLayer : public IPhy {
				public:
					explicit PHYLayer(unsigned long datarate) : datarate(datarate) {}
					
					void receiveFromUpper(L2Packet* data, unsigned int center_frequency) override {
					
					}
					
					unsigned long getCurrentDatarate() const override {
						return datarate;
					}
					
					unsigned long datarate;
			};
			PHYLayer* phy_layer;
		
		public:
			void setUp() override {
				reservation_manager = new ReservationManager(planning_horizon);
				reservation_manager->addFrequencyChannel(false, bc_frequency, bandwidth);
				reservation_manager->addFrequencyChannel(true, center_frequency1, bandwidth);
				reservation_manager->addFrequencyChannel(true, center_frequency2, bandwidth);
				reservation_manager->addFrequencyChannel(true, center_frequency3, bandwidth);
				mac = new MACLayer(reservation_manager);
				link_manager = new LinkManager(id, reservation_manager, mac);
				arq_layer = new ARQLayer();
				mac->setUpperLayer(arq_layer);
				arq_layer->setLowerLayer(mac);
				rlc_layer = new RLCLayer();
				rlc_layer->setLowerLayer(arq_layer);
				arq_layer->setUpperLayer(rlc_layer);
				phy_layer = new PHYLayer(num_bits_going_out / 2);
				phy_layer->setUpperLayer(mac);
				mac->setLowerLayer(phy_layer);
			}
			
			void tearDown() override {
				delete reservation_manager;
				delete mac;
				delete link_manager;
				delete arq_layer;
				delete rlc_layer;
				delete phy_layer;
			}
			
			void testTrafficEstimate() {
				CPPUNIT_ASSERT_EQUAL(0.0, link_manager->getCurrentTrafficEstimate());
				unsigned int initial_bits = 10;
				unsigned int num_bits = initial_bits;
				double sum = 0;
				// Fill up the window.
				for (size_t i = 0; i < link_manager->getTrafficEstimateWindowSize(); i++) {
					link_manager->updateTrafficEstimate(num_bits);
					sum += num_bits;
					num_bits += initial_bits;
					CPPUNIT_ASSERT_EQUAL(sum / (i+1), link_manager->getCurrentTrafficEstimate());
				}
				// Now it's full, so the next input will kick out the first value.
				link_manager->updateTrafficEstimate(num_bits);
				sum -= initial_bits;
				sum += num_bits;
				CPPUNIT_ASSERT_EQUAL(sum / (link_manager->getTrafficEstimateWindowSize()), link_manager->getCurrentTrafficEstimate());
			}
			
			void testNewLinkEstablishment() {
				// It must a be a P2P link.
				CPPUNIT_ASSERT(id != SYMBOLIC_LINK_ID_BROADCAST && id != SYMBOLIC_LINK_ID_BEACON);
				// Initially the link should not be established.
				CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_not_established, link_manager->link_establishment_status);
				CPPUNIT_ASSERT_EQUAL(size_t(0), rlc_layer->injections.size());
				// Now inform the LinkManager of new data for this link.
				link_manager->notifyOutgoing(num_bits_going_out);
				// The RLC should've received a link request.
				CPPUNIT_ASSERT_EQUAL(size_t(1), rlc_layer->injections.size());
				CPPUNIT_ASSERT_EQUAL(size_t(2), rlc_layer->injections.at(0)->getHeaders().size());
				CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::base, rlc_layer->injections.at(0)->getHeaders().at(0)->frame_type);
				CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::link_establishment_request, rlc_layer->injections.at(0)->getHeaders().at(1)->frame_type);
				// And the LinkManager status should've updated.
				CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_reply, link_manager->link_establishment_status);
			}
			
			void testComputeProposal() {
				testNewLinkEstablishment();
				L2Packet* request = rlc_layer->injections.at(0);
				LinkManager::ProposalPayload* proposal = link_manager->computeProposal(request);
				CPPUNIT_ASSERT_EQUAL(size_t(2), request->getPayloads().size());
				
				// Should've considered several distinct frequency channels.
				CPPUNIT_ASSERT_EQUAL(size_t(link_manager->num_proposed_channels), proposal->proposed_channels.size());
				for (size_t i = 1; i < proposal->proposed_channels.size(); i++) {
					const FrequencyChannel* channel0 = proposal->proposed_channels.at(i-1);
					const FrequencyChannel* channel1 = proposal->proposed_channels.at(i);
					CPPUNIT_ASSERT(channel1->getCenterFrequency() != channel0->getCenterFrequency());
				}
				
				// Should've considered a number of candidate slots per frequency channel.
				for (auto num_slots_in_this_candidate : proposal->num_candidates) {
					// Since all are idle, we should've found the target number each time.
					CPPUNIT_ASSERT_EQUAL(link_manager->num_proposed_slots, num_slots_in_this_candidate);
				}
				// and so the grand total should be the number of proposed slots times the number of proposed channels.
				CPPUNIT_ASSERT_EQUAL(size_t(link_manager->num_proposed_channels * link_manager->num_proposed_slots), proposal->proposed_slots.size());
				
				// To have a look...
//				coutd.setVerbose(true);
//				for (unsigned int slot : proposal->proposed_slots)
//					coutd << slot << " ";
//				coutd << std::endl;
//				coutd.setVerbose(false);
			}
		
		CPPUNIT_TEST_SUITE(LinkManagerTests);
			CPPUNIT_TEST(testTrafficEstimate);
			CPPUNIT_TEST(testNewLinkEstablishment);
			CPPUNIT_TEST(testComputeProposal);
		CPPUNIT_TEST_SUITE_END();
	};
}