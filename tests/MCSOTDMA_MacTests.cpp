//
// Created by Sebastian Lindner on 04.12.20.
//

//
// Created by Sebastian Lindner on 04.11.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../MCSOTDMA_Mac.hpp"
#include "../coutdebug.hpp"
#include <IPhy.hpp>
#include <IArq.hpp>
#include <IRlc.hpp>

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	class MCSOTDMA_MacTests : public CppUnit::TestFixture {
		private:
			class PHYLayer : public IPhy {
				public:
					void receiveFromUpper(L2Packet* data, unsigned int center_frequency) override {
						throw std::runtime_error("not implemented");
					}
					
					unsigned long getCurrentDatarate() const override {
						return 0;
					}
			};
			PHYLayer* phy;
			
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
						coutd << "ARQ::notifyOutgoing(" << num_bits << ", " << mac_id.getId() << ")" << std::endl;
					}
					
					L2Packet* requestSegment(unsigned int num_bits, const MacId& mac_id) override {
						coutd << "ARQ::requestSegment" << std::endl;
						return nullptr;
					}
					
					bool shouldLinkBeArqProtected(const MacId& mac_id) const override {
						return false;
					}
					
					void notifyAboutNewLink(const MacId& id) override {
					
					}
					
					void notifyAboutRemovedLink(const MacId& id) override {
					
					}
				
				protected:
					void processIncomingHeader(L2Packet* incoming_packet) override {
					
					}
			};
			ARQLayer* arq;
			
			class RLCLayer : public IRlc {
				public:
					void receiveFromUpper(L3Packet* data, MacId dest, PacketPriority priority) override {
					
					}
					
					void receiveFromLower(L2Packet* packet) override {
						coutd << "RLC received packet." << std::endl;
					}
					
					void receiveInjectionFromLower(L2Packet* packet, PacketPriority priority) override {
						coutd << "RLC received injection." << std::endl;
					}
					
					L2Packet* requestSegment(unsigned int num_bits, const MacId& mac_id) override {
						return nullptr;
					}
			};
			RLCLayer* rlc;
			
			uint32_t planning_horizon = 128;
			uint64_t p2p_frequency1 = 1000, p2p_frequency2 = 2000, p2p_frequency3 = 3000, bc_frequency = 4000, bandwidth = 500;
			ReservationManager* reservation_manager;
			MacId communication_partner_id = MacId(42);
		
		public:
			void setUp() override {
				reservation_manager = new ReservationManager(planning_horizon);
				reservation_manager->addFrequencyChannel(false, bc_frequency, bandwidth);
				reservation_manager->addFrequencyChannel(true, p2p_frequency1, bandwidth);
				reservation_manager->addFrequencyChannel(true, p2p_frequency2, bandwidth);
				reservation_manager->addFrequencyChannel(true, p2p_frequency3, bandwidth);
				mac = new MACLayer(reservation_manager);
				// PHY
				phy = new PHYLayer();
				mac->setLowerLayer(phy);
				phy->setUpperLayer(mac);
				// ARQ
				arq = new ARQLayer();
				arq->setLowerLayer(mac);
				mac->setUpperLayer(arq);
				// RLC
				rlc = new RLCLayer();
				arq->setUpperLayer(rlc);
				rlc->setLowerLayer(arq);
			}
			
			void tearDown() override {
				delete reservation_manager;
				delete mac;
				delete phy;
				delete arq;
				delete rlc;
			}
			
			void testLinkManagerCreation() {
//				coutd.setVerbose(true);
				CPPUNIT_ASSERT_EQUAL(size_t(0), mac->link_managers.size());
				MacId id = MacId(42);
				mac->notifyOutgoing(1024, id);
				CPPUNIT_ASSERT_EQUAL(size_t(1), mac->link_managers.size());
				LinkManager* link_manager = mac->link_managers.at(id);
				CPPUNIT_ASSERT(link_manager);
				CPPUNIT_ASSERT(id == link_manager->getLinkId());
//				coutd.setVerbose(false);
			}
			
			void testMakeReservation() {
				mac->notifyOutgoing(1024, communication_partner_id);
				CPPUNIT_ASSERT(mac->getLinkManager(communication_partner_id)->link_establishment_status == LinkManager::Status::awaiting_reply);
			}
			
			void testUpdate() {
				testMakeReservation();
				coutd.setVerbose(true);
				reservation_manager->getReservationTable(0)->mark(1, Reservation(MacId(42), Reservation::Action::TX));
				mac->update(1);
				coutd.setVerbose(false);
			}
		
		
		CPPUNIT_TEST_SUITE(MCSOTDMA_MacTests);
			CPPUNIT_TEST(testLinkManagerCreation);
			CPPUNIT_TEST(testMakeReservation);
			CPPUNIT_TEST(testUpdate);
		CPPUNIT_TEST_SUITE_END();
	};
	
}