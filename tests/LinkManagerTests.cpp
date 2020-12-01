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

namespace TUHH_INTAIRNET_MCSOTDMA {
	class LinkManagerTests : public CppUnit::TestFixture {
		private:
			LinkManager* link_manager;
			ReservationManager* reservation_manager;
			MCSOTDMA_Mac* mac;
			MacId id = MacId(0);
			uint32_t planning_horizon = 128;
			uint64_t center_frequency1 = 1000, center_frequency2 = 2000, center_frequency3 = 3000, bc_frequency = 4000, bandwidth = 500;
			
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
			};
			ARQLayer* arq_layer;
			
			class RLCLayer : public IRlc {
				public:
					virtual ~RLCLayer() {
						for (auto* packet : injections)
							delete packet;
					}
					
					void receiveFromUpper(L3Packet* data) override {
						throw std::runtime_error("not implemented");
					}
					
					void receiveInjectionFromLower(L2Packet* packet) override {
						injections.push_back(packet);
					}
					
					L2Packet* requestSegment(unsigned int num_bits, const MacId& mac_id) override {
						throw std::runtime_error("not implemented");
					}
					
					std::vector<L2Packet*> injections;
			};
			RLCLayer* rlc_layer;
		
		public:
			void setUp() override {
				reservation_manager = new ReservationManager(planning_horizon);
				reservation_manager->addFrequencyChannel(false, bc_frequency, bandwidth);
				reservation_manager->addFrequencyChannel(true, center_frequency1, bandwidth);
				reservation_manager->addFrequencyChannel(true, center_frequency2, bandwidth);
				reservation_manager->addFrequencyChannel(true, center_frequency3, bandwidth);
				mac = new MCSOTDMA_Mac(reservation_manager);
				link_manager = new LinkManager(id, reservation_manager, mac);
				arq_layer = new ARQLayer();
				mac->setUpperLayer(arq_layer);
				arq_layer->setLowerLayer(mac);
				rlc_layer = new RLCLayer();
				rlc_layer->setLowerLayer(arq_layer);
				arq_layer->setUpperLayer(rlc_layer);
			}
			
			void tearDown() override {
				delete reservation_manager;
				delete mac;
				delete link_manager;
				delete arq_layer;
				delete rlc_layer;
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
				unsigned long num_bits = 1024;
				link_manager->notifyOutgoing(num_bits);
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
				link_manager->computeProposal(request);
			}
		
		CPPUNIT_TEST_SUITE(LinkManagerTests);
			CPPUNIT_TEST(testTrafficEstimate);
			CPPUNIT_TEST(testNewLinkEstablishment);
			CPPUNIT_TEST(testComputeProposal);
		CPPUNIT_TEST_SUITE_END();
	};
}