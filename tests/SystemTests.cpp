//
// Created by seba on 2/24/21.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "MockLayers.hpp"
#include "../LinkManager.hpp"
#include "../PPLinkManager.hpp"
#include "../SHLinkManager.hpp"


namespace TUHH_INTAIRNET_MCSOTDMA {
	/**
	 * These tests aim at both sides of a communication link, so that e.g. link renewal can be properly tested,
	 * ensuring that both sides are in valid states at all times.
	 */
	class SystemTests : public CppUnit::TestFixture {
	private:
		TestEnvironment *env_me, *env_you;

		MacId own_id, partner_id;
		uint32_t planning_horizon;
		uint64_t center_frequency1, center_frequency2, center_frequency3, sh_frequency, bandwidth;
		NetworkLayer* net_layer_me, * net_layer_you;
		RLCLayer* rlc_layer_me, * rlc_layer_you;
		ARQLayer* arq_layer_me, * arq_layer_you;
		MACLayer* mac_layer_me, * mac_layer_you;
		PHYLayer* phy_layer_me, * phy_layer_you;
		size_t num_outgoing_bits;

		PPLinkManager *pp_me, *pp_you;
		SHLinkManager *sh_me, *sh_you;

	public:
		void setUp() override {
			own_id = MacId(42);
			partner_id = MacId(43);
			env_me = new TestEnvironment(own_id, partner_id);
			env_you = new TestEnvironment(partner_id, own_id);

			center_frequency1 = env_me->p2p_freq_1;
			center_frequency2 = env_me->p2p_freq_2;
			center_frequency3 = env_me->p2p_freq_3;
			sh_frequency = env_me->sh_frequency;
			bandwidth = env_me->bandwidth;
			planning_horizon = env_me->planning_horizon;

			net_layer_me = env_me->net_layer;
			net_layer_you = env_you->net_layer;
			rlc_layer_me = env_me->rlc_layer;
			rlc_layer_you = env_you->rlc_layer;
			arq_layer_me = env_me->arq_layer;
			arq_layer_you = env_you->arq_layer;
			mac_layer_me = env_me->mac_layer;
			mac_layer_you = env_you->mac_layer;
			phy_layer_me = env_me->phy_layer;
			phy_layer_you = env_you->phy_layer;

			phy_layer_me->connected_phys.push_back(phy_layer_you);
			phy_layer_you->connected_phys.push_back(phy_layer_me);

			num_outgoing_bits = 512;
			pp_me = (PPLinkManager*) mac_layer_me->getLinkManager(partner_id);
			pp_you = (PPLinkManager*) mac_layer_you->getLinkManager(own_id);
			sh_me = (SHLinkManager*) mac_layer_me->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
			sh_you = (SHLinkManager*) mac_layer_you->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
		}

		void tearDown() override {
			delete env_me;
			delete env_you;
		}

		void testLinkEstablishment() {
//			coutd.setVerbose(true);
			// Single message.
			rlc_layer_me->should_there_be_more_p2p_data = false;
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			// New data for communication partner.
			mac_layer_me->notifyOutgoing(512, partner_id);
			size_t num_slots = 0, max_slots = 200;
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac_layer_you->stat_num_packets_rcvd.get());
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac_layer_me->stat_num_packets_rcvd.get());
			while (mac_layer_me->stat_num_broadcasts_sent.get() < 1 && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);
						
			// Reservation timeout should still be default.
			CPPUNIT_ASSERT_EQUAL(mac_layer_me->getDefaultPPLinkTimeout(), pp_me->getRemainingTimeout());			

			// Increment time until status is 'link_established'.
			num_slots = 0;
			while (((LinkManager*) mac_layer_me->getLinkManager(partner_id))->link_status != LinkManager::link_established && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);
			// Link reply + first data tx should've arrived, so *our* link should be established...
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, mac_layer_me->getLinkManager(partner_id)->link_status);			
			// ... and *their* link should also be established
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, mac_layer_you->getLinkManager(own_id)->link_status);			
		}		

		/** Before introducing the onSlotEnd() function, success depended on the order of the execute() calls (which is of course terrible),
		 * so this test ensures that the order in which user actions are executed doesn't matter.
		 */
		void testCommunicateInOtherDirection() {
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			// Do link establishment.
			size_t num_slots = 0, max_num_slots = 100;			
			// Other guy tries to communicate with us.
			mac_layer_you->notifyOutgoing(512, own_id);
			while (pp_you->link_status != LinkManager::Status::link_established && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT_LESS(max_num_slots, num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, pp_you->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, pp_me->link_status);
		}

		/** Before introducing the onSlotEnd() function, success depended on the order of the execute() calls (which is of course terrible),
		 * so this test ensures that the order in which user actions are executed doesn't matter.
		 */
		void testCommunicateReverseOrder() {
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			// Do link establishment.
			size_t num_slots = 0, max_num_slots = 100;			
			mac_layer_me->notifyOutgoing(512, partner_id);
			while (pp_me->link_status != LinkManager::Status::link_established && num_slots++ < max_num_slots) {
				// you first, then me
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT_LESS(max_num_slots, num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, pp_you->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, pp_me->link_status);
		}		

		void testSimultaneousRequests() {
//			coutd.setVerbose(true);
			mac_layer_me->notifyOutgoing(512, partner_id);
			mac_layer_you->notifyOutgoing(512, own_id);
			size_t num_slots = 0, max_num_slots = 15000;
			while ((pp_me->link_status != LinkManager::link_established || pp_you->link_status != LinkManager::link_established) && num_slots++ < max_num_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT_LESS(max_num_slots, num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, pp_me->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, pp_you->link_status);
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_layer_me->stat_num_requests_rcvd.get() + (size_t) mac_layer_you->stat_num_requests_rcvd.get());
			CPPUNIT_ASSERT((size_t) mac_layer_me->stat_num_requests_sent.get() + (size_t) mac_layer_you->stat_num_requests_sent.get() >= 1); // due to collisions, several attempts may be required
		}		

		/**
		 * Tests that two users can re-establish a link many times.
		 */
		void testManyReestablishments() {
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_you->should_there_be_more_p2p_data = false;
			pp_me->notifyOutgoing(512);
			size_t num_reestablishments = 5, num_slots = 0, max_slots = 10000;
//			coutd.setVerbose(true);
			while (((int) mac_layer_me->stat_num_pp_links_established.get()) != num_reestablishments && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
				CPPUNIT_ASSERT(!(pp_me->link_status == LinkManager::link_established && pp_you->link_status == LinkManager::link_not_established));
			}
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);
			CPPUNIT_ASSERT_EQUAL(num_reestablishments, size_t(mac_layer_me->stat_num_pp_links_established.get()));
			CPPUNIT_ASSERT_EQUAL(num_reestablishments, size_t(mac_layer_me->stat_num_pp_links_established.get()));
		}

		void testMACDelays() {
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			rlc_layer_me->num_remaining_broadcast_packets = 4;			
			sh_me->notifyOutgoing(1);			
			// proceed until the first broadcast's been received
			size_t num_broadcasts = 1;
			size_t num_slots = 0, max_slots = 100;
			while (rlc_layer_you->receptions.size() < num_broadcasts && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();				
			}
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);
			// check statistics
			CPPUNIT_ASSERT_GREATEREQUAL(num_broadcasts, (size_t) mac_layer_me->stat_num_broadcasts_sent.get());
			CPPUNIT_ASSERT_GREATEREQUAL(num_broadcasts, (size_t) mac_layer_you->stat_num_broadcasts_rcvd.get());			
			CPPUNIT_ASSERT_GREATER(0.0, mac_layer_me->stat_broadcast_selected_candidate_slots.get());
			CPPUNIT_ASSERT_GREATER(0.0, mac_layer_me->stat_broadcast_mac_delay.get());			
			// proceed further
			num_broadcasts = 3;			
			num_slots = 0; 
			max_slots = 1000;			
			while (rlc_layer_you->receptions.size() < num_broadcasts && num_slots++ < max_slots) {
				sh_me->notifyOutgoing(1);
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();				
			}
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);
			// check statistics
			CPPUNIT_ASSERT_GREATEREQUAL(num_broadcasts, (size_t) mac_layer_me->stat_num_broadcasts_sent.get());
			CPPUNIT_ASSERT_GREATEREQUAL(num_broadcasts, (size_t) mac_layer_you->stat_num_broadcasts_rcvd.get());			
		}				
				
		/**
		 * From issue 102: https://collaborating.tuhh.de/e-4/research-projects/intairnet-collection/mc-sotdma/-/issues/102
		 * */
		void testMissedLastLinkEstablishmentOpportunity() {
			// don't attempt to re-establish
			rlc_layer_me->should_there_be_more_p2p_data = false;
			rlc_layer_you->should_there_be_more_p2p_data = false;
			pp_me->notifyOutgoing(512);
			size_t num_slots = 0, max_slots = 500;
			while (pp_you->link_status != LinkManager::link_established && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_layer_me->stat_num_requests_sent.get());
			CPPUNIT_ASSERT_EQUAL(LinkManager::awaiting_reply, pp_me->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, pp_you->link_status);
			// packet drops from now on
			phy_layer_me->connected_phys.clear();
			phy_layer_you->connected_phys.clear();
			num_slots = 0;
			for (size_t t = 0; t < max_slots; t++) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			// both sides should've entered link establishment by now						
			CPPUNIT_ASSERT(pp_me->link_status == LinkManager::awaiting_request_generation || pp_me->link_status == LinkManager::awaiting_reply || pp_me->link_status == LinkManager::link_not_established);						
			CPPUNIT_ASSERT_GREATER(size_t(1), (size_t) mac_layer_me->stat_num_requests_sent.get());
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac_layer_you->stat_num_requests_sent.get());
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac_layer_me->stat_num_requests_rcvd.get());
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_layer_you->stat_num_requests_rcvd.get());			
		}		

		void testDutyCycleContributions() {
			// initially with nothing scheduled, no duty cycle contributions should be present
			auto pair = mac_layer_me->getUsedPPDutyCycleBudget();
			auto &duty_cycle_contrib = pair.first;
			CPPUNIT_ASSERT_EQUAL(true, duty_cycle_contrib.empty());						
			env_me->rlc_layer->should_there_be_more_broadcast_data = true;
			CPPUNIT_ASSERT_EQUAL(false, sh_me->next_broadcast_scheduled);			
			// schedule broadcast			
			sh_me->notifyOutgoing(1);
			CPPUNIT_ASSERT_EQUAL(true, sh_me->next_broadcast_scheduled);
			// SH should not contribute to the number of transmissions
			pair = mac_layer_me->getUsedPPDutyCycleBudget();
			duty_cycle_contrib = pair.first;
			CPPUNIT_ASSERT_EQUAL(true, duty_cycle_contrib.empty());			
			// establish PP link
			mac_layer_me->notifyOutgoing(512, partner_id);			
			size_t num_slots = 0, max_slots = 512;			
			while (mac_layer_me->stat_num_pp_links_established.get() < 1.0 && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();								
			}
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);
			CPPUNIT_ASSERT_EQUAL(size_t(1), size_t(mac_layer_me->stat_num_pp_links_established.get()));
			// now the PP should contribute to the number of transmissions
			pair = mac_layer_me->getUsedPPDutyCycleBudget();		
			duty_cycle_contrib = pair.first;
			CPPUNIT_ASSERT_EQUAL(false, duty_cycle_contrib.empty());			
			double used_budget = 0.0;
			for (auto d : duty_cycle_contrib)
				used_budget += d;
			CPPUNIT_ASSERT_GREATER(0.0, used_budget);
		}

		void testDutyCyclePeriodicityPP() {			
			mac_layer_me->setMinNumSupportedPPLinks(4);
			unsigned int duty_cycle_periodicity = 100;
			double max_duty_cycle = 0.1;
			mac_layer_me->setDutyCycle(100, 0.1, 4);			
			mac_layer_me->setDutyCycleBudgetComputationStrategy(DutyCycleBudgetStrategy::DYNAMIC);
			std::vector<double> duty_cycle_contribs;
			std::vector<int> timeouts;
			double used_sh_budget = 0.0;
			int sh_slot_offset = -1;
			auto min_offset_and_period = mac_layer_me->getDutyCycle().getPeriodicityPP(duty_cycle_contribs, timeouts, used_sh_budget, sh_slot_offset);
			CPPUNIT_ASSERT_EQUAL(0, min_offset_and_period.second);
		}

		void testDutyCyclePeriodicityPPOnlyOneLinkNeeded() {
			unsigned int duty_cycle_periodicity = 100;
			double max_duty_cycle = 0.1;
			mac_layer_me->setDutyCycle(100, 0.1, 1);		
			mac_layer_me->setDutyCycleBudgetComputationStrategy(DutyCycleBudgetStrategy::DYNAMIC);	
			std::vector<double> duty_cycle_contribs;
			std::vector<int> timeouts;
			double used_sh_budget = 0.02;
			int sh_slot_offset = 5;
			auto min_offset_and_period = mac_layer_me->getDutyCycle().getPeriodicityPP(duty_cycle_contribs, timeouts, used_sh_budget, sh_slot_offset);
			auto &min_offset = min_offset_and_period.first;
			auto &period = min_offset_and_period.second;
			CPPUNIT_ASSERT_EQUAL(0, min_offset);
			CPPUNIT_ASSERT_EQUAL(1, period);
		}

		void testDutyCycleSHBudgetOnlyOneLinkNeeded() {
			unsigned int duty_cycle_periodicity = 100;
			double max_duty_cycle = 0.1;
			mac_layer_me->setDutyCycle(100, 0.1, 1);
			mac_layer_me->setDutyCycleBudgetComputationStrategy(DutyCycleBudgetStrategy::DYNAMIC);
			auto contributions_and_timeouts = mac_layer_me->getUsedPPDutyCycleBudget();
			const std::vector<double> &used_pp_duty_cycle_budget = contributions_and_timeouts.first;			
			const std::vector<int>& timeouts = contributions_and_timeouts.second;
			double sh_budget = mac_layer_me->getDutyCycle().getSHBudget(used_pp_duty_cycle_budget);			
			auto min_offset_and_period = mac_layer_me->getDutyCycle().getPeriodicityPP(used_pp_duty_cycle_budget, timeouts, sh_budget, 1);
			auto &period = min_offset_and_period.second;
			CPPUNIT_ASSERT_EQUAL(1, period);			
		}

		void testDutyCyclePeriodicityPPOneLinkUsed() {
			unsigned int duty_cycle_periodicity = 100;
			double max_duty_cycle = 0.1;
			mac_layer_me->setDutyCycle(100, 0.1, 4);			
			mac_layer_me->setDutyCycleBudgetComputationStrategy(DutyCycleBudgetStrategy::DYNAMIC);
			std::vector<double> duty_cycle_contribs;
			duty_cycle_contribs.push_back(0.02);
			std::vector<int> timeouts;
			timeouts.push_back(100);
			double used_sh_budget = 0.0;
			int sh_slot_offset = -1;
			auto min_offset_and_period = mac_layer_me->getDutyCycle().getPeriodicityPP(duty_cycle_contribs, timeouts, used_sh_budget, sh_slot_offset);
			CPPUNIT_ASSERT_EQUAL(1, min_offset_and_period.second);
		}

		void testDutyCyclePeriodicityPPTwoLinksUsed() {
			unsigned int duty_cycle_periodicity = 100;
			double max_duty_cycle = 0.1;
			mac_layer_me->setDutyCycle(100, 0.1, 4);			
			mac_layer_me->setDutyCycleBudgetComputationStrategy(DutyCycleBudgetStrategy::DYNAMIC);
			std::vector<double> duty_cycle_contribs;
			duty_cycle_contribs.push_back(0.02);
			duty_cycle_contribs.push_back(0.02);
			std::vector<int> timeouts;
			timeouts.push_back(100);
			timeouts.push_back(101);
			double used_sh_budget = 0.0;
			int sh_slot_offset = -1;
			auto min_offset_and_period = mac_layer_me->getDutyCycle().getPeriodicityPP(duty_cycle_contribs, timeouts, used_sh_budget, sh_slot_offset);
			CPPUNIT_ASSERT_GREATEREQUAL(1, min_offset_and_period.second);
			CPPUNIT_ASSERT_LESSEQUAL(2, min_offset_and_period.second);			
		}

		void testDutyCyclePeriodicityPPThreeLinksUsed() {
			unsigned int duty_cycle_periodicity = 100;
			double max_duty_cycle = 0.1;
			mac_layer_me->setDutyCycle(100, 0.1, 4);			
			mac_layer_me->setDutyCycleBudgetComputationStrategy(DutyCycleBudgetStrategy::DYNAMIC);
			std::vector<double> duty_cycle_contribs;
			duty_cycle_contribs.push_back(0.02);
			duty_cycle_contribs.push_back(0.02);
			duty_cycle_contribs.push_back(0.02);
			std::vector<int> timeouts;
			timeouts.push_back(100);
			timeouts.push_back(101);
			timeouts.push_back(102);
			double used_sh_budget = 0.0;
			int sh_slot_offset = -1;
			auto min_offset_and_period = mac_layer_me->getDutyCycle().getPeriodicityPP(duty_cycle_contribs, timeouts, used_sh_budget, sh_slot_offset);
			CPPUNIT_ASSERT_GREATEREQUAL(1, min_offset_and_period.second);
			CPPUNIT_ASSERT_LESSEQUAL(3, min_offset_and_period.second);			
		}

		void testDutyCyclePeriodicityPPThreeLinksUsedFromCrash() {
			unsigned int duty_cycle_periodicity = 100;
			double max_duty_cycle = 0.1;
			mac_layer_me->setDutyCycle(100, 0.1, 4);			
			mac_layer_me->setDutyCycleBudgetComputationStrategy(DutyCycleBudgetStrategy::DYNAMIC);
			std::vector<double> duty_cycle_contribs;
			duty_cycle_contribs.push_back(0.025);
			duty_cycle_contribs.push_back(0.0125);
			duty_cycle_contribs.push_back(0.0125);
			std::vector<int> timeouts;
			timeouts.push_back(100);
			timeouts.push_back(101);
			timeouts.push_back(102);
			double used_sh_budget = 0.02;
			int sh_slot_offset = 5;
			auto min_offset_and_period = mac_layer_me->getDutyCycle().getPeriodicityPP(duty_cycle_contribs, timeouts, used_sh_budget, sh_slot_offset);
			CPPUNIT_ASSERT_GREATEREQUAL(1, min_offset_and_period.second);
			CPPUNIT_ASSERT_LESSEQUAL(3, min_offset_and_period.second);			
		}

		void testDutyCyclePeriodicityPPFourLinksUsed() {
			unsigned int duty_cycle_periodicity = 100;
			double max_duty_cycle = 0.1;
			mac_layer_me->setDutyCycle(100, 0.1, 4);			
			mac_layer_me->setDutyCycleBudgetComputationStrategy(DutyCycleBudgetStrategy::DYNAMIC);
			std::vector<double> duty_cycle_contribs;
			duty_cycle_contribs.push_back(0.02);
			duty_cycle_contribs.push_back(0.02);
			duty_cycle_contribs.push_back(0.02);			
			duty_cycle_contribs.push_back(0.02);
			std::vector<int> timeouts;
			timeouts.push_back(100);
			timeouts.push_back(101);
			timeouts.push_back(102);
			timeouts.push_back(103);
			double used_sh_budget = 0.0;
			int sh_slot_offset = -1;
			auto min_offset_and_period = mac_layer_me->getDutyCycle().getPeriodicityPP(duty_cycle_contribs, timeouts, used_sh_budget, sh_slot_offset);
			CPPUNIT_ASSERT_EQUAL(101, min_offset_and_period.first);
			CPPUNIT_ASSERT_EQUAL(3, min_offset_and_period.second);
		}

		void testDutyCyclePeriodicityPPNoBudget() {
			unsigned int duty_cycle_periodicity = 100;
			double max_duty_cycle = 0.1;
			mac_layer_me->setDutyCycle(100, 0.1, 4);			
			mac_layer_me->setDutyCycleBudgetComputationStrategy(DutyCycleBudgetStrategy::DYNAMIC);
			std::vector<double> duty_cycle_contribs;
			duty_cycle_contribs.push_back(0.025);
			duty_cycle_contribs.push_back(0.025);
			duty_cycle_contribs.push_back(0.025);			
			duty_cycle_contribs.push_back(0.025);
			std::vector<int> timeouts;
			timeouts.push_back(100);
			timeouts.push_back(101);
			timeouts.push_back(102);
			timeouts.push_back(103);
			double used_sh_budget = 0.0;
			int sh_slot_offset = -1;
			auto min_offset_and_period = mac_layer_me->getDutyCycle().getPeriodicityPP(duty_cycle_contribs, timeouts, used_sh_budget, sh_slot_offset);
			CPPUNIT_ASSERT_EQUAL(102, min_offset_and_period.first);
			CPPUNIT_ASSERT_GREATEREQUAL(2, min_offset_and_period.second);
		}

		void testDutyCycleLastLink() {
			unsigned int duty_cycle_periodicity = 100;
			double max_duty_cycle = 0.1;
			mac_layer_me->setDutyCycle(100, 0.1, 4);			
			mac_layer_me->setDutyCycleBudgetComputationStrategy(DutyCycleBudgetStrategy::DYNAMIC);
			std::vector<double> duty_cycle_contribs;
			// 3% used
			duty_cycle_contribs.push_back(0.01);
			duty_cycle_contribs.push_back(0.01);
			duty_cycle_contribs.push_back(0.01);						
			std::vector<int> timeouts;
			timeouts.push_back(100);
			timeouts.push_back(101);
			timeouts.push_back(102);
			double used_sh_budget = 0.0;
			int sh_slot_offset = -1;			
			auto min_offset_and_period = mac_layer_me->getDutyCycle().getPeriodicityPP(duty_cycle_contribs, timeouts, used_sh_budget, sh_slot_offset);
			CPPUNIT_ASSERT_EQUAL(0, min_offset_and_period.first);
			// last link can use remaining budget			
			CPPUNIT_ASSERT_LESS(50, min_offset_and_period.second);
		}

		void testDutyCycleSHTakesAll() {
			unsigned int duty_cycle_periodicity = 100;
			double max_duty_cycle = 0.1;
			mac_layer_me->setDutyCycle(100, 0.1, 4);			
			mac_layer_me->setDutyCycleBudgetComputationStrategy(DutyCycleBudgetStrategy::DYNAMIC);
			std::vector<double> duty_cycle_contribs;
			// PP uses 2%
			duty_cycle_contribs.push_back(0.02);
			std::vector<int> timeouts;
			timeouts.push_back(100);
			// SH uses 8%
			double used_sh_budget = 0.08;
			int sh_slot_offset = 5;
			auto min_offset_and_period = mac_layer_me->getDutyCycle().getPeriodicityPP(duty_cycle_contribs, timeouts, used_sh_budget, sh_slot_offset);
			CPPUNIT_ASSERT_EQUAL(6, min_offset_and_period.first);
			// last link can use remaining budget			
			CPPUNIT_ASSERT_LESS(50, min_offset_and_period.second);
		}

		void testDutyCycleGetSHOffsetNoPPLinks() {
			unsigned int duty_cycle_periodicity = 100;
			double max_duty_cycle = 0.1;
			mac_layer_me->setDutyCycle(100, 0.1, 4);			
			mac_layer_me->setDutyCycleBudgetComputationStrategy(DutyCycleBudgetStrategy::DYNAMIC);
			std::vector<double> duty_cycle_contribs;
			std::vector<int> timeouts;
			int sh_offset = mac_layer_me->getDutyCycle().getOffsetSH(duty_cycle_contribs);
			// SH can use 8% as it must leave 2% for next PP link
			CPPUNIT_ASSERT_EQUAL((int) (1.0/0.08), sh_offset);
		}

		void testDutyCycleGetSHOffsetOnePPLinks() {
			unsigned int duty_cycle_periodicity = 100;
			double max_duty_cycle = 0.1;
			mac_layer_me->setDutyCycle(100, 0.1, 4);			
			mac_layer_me->setDutyCycleBudgetComputationStrategy(DutyCycleBudgetStrategy::DYNAMIC);
			std::vector<double> duty_cycle_contribs;
			duty_cycle_contribs.push_back(.02);
			std::vector<int> timeouts;
			timeouts.push_back(100);
			int sh_offset = mac_layer_me->getDutyCycle().getOffsetSH(duty_cycle_contribs);
			// SH can use 6% as it must leave 2% for next PP link
			CPPUNIT_ASSERT_EQUAL((int) (1.0/0.06), sh_offset);
		}

		void testDutyCycleGetSHOffsetTwoPPLinks() {
			unsigned int duty_cycle_periodicity = 100;
			double max_duty_cycle = 0.1;
			mac_layer_me->setDutyCycle(100, 0.1, 4);			
			mac_layer_me->setDutyCycleBudgetComputationStrategy(DutyCycleBudgetStrategy::DYNAMIC);
			std::vector<double> duty_cycle_contribs;
			duty_cycle_contribs.push_back(.02);
			duty_cycle_contribs.push_back(.02);
			std::vector<int> timeouts;
			timeouts.push_back(100);
			timeouts.push_back(100);
			int sh_offset = mac_layer_me->getDutyCycle().getOffsetSH(duty_cycle_contribs);
			// SH can use 4% as it must leave 2% for next PP link
			CPPUNIT_ASSERT_EQUAL((int) (1.0/0.04), sh_offset);
		}

		void testDutyCycleGetSHOffsetThreePPLinks() {
			unsigned int duty_cycle_periodicity = 100;
			double max_duty_cycle = 0.1;
			mac_layer_me->setDutyCycle(100, 0.1, 4);			
			mac_layer_me->setDutyCycleBudgetComputationStrategy(DutyCycleBudgetStrategy::DYNAMIC);
			std::vector<double> duty_cycle_contribs;
			duty_cycle_contribs.push_back(.02);
			duty_cycle_contribs.push_back(.02);
			duty_cycle_contribs.push_back(.02);
			std::vector<int> timeouts;
			timeouts.push_back(100);
			timeouts.push_back(100);
			timeouts.push_back(100);
			int sh_offset = mac_layer_me->getDutyCycle().getOffsetSH(duty_cycle_contribs);
			// SH can use 2% as it must leave 2% for next PP link
			CPPUNIT_ASSERT_EQUAL((int) (1.0/0.02), sh_offset);
		}

		void testDutyCycleGetSHOffsetFourPPLinks() {
			unsigned int duty_cycle_periodicity = 100;
			double max_duty_cycle = 0.1;
			mac_layer_me->setDutyCycle(100, 0.1, 4);			
			mac_layer_me->setDutyCycleBudgetComputationStrategy(DutyCycleBudgetStrategy::DYNAMIC);
			std::vector<double> duty_cycle_contribs;
			duty_cycle_contribs.push_back(.02);
			duty_cycle_contribs.push_back(.02);
			duty_cycle_contribs.push_back(.02);
			duty_cycle_contribs.push_back(.02);
			std::vector<int> timeouts;
			timeouts.push_back(100);
			timeouts.push_back(100);
			timeouts.push_back(100);
			timeouts.push_back(100);
			int sh_offset = mac_layer_me->getDutyCycle().getOffsetSH(duty_cycle_contribs);
			// SH can use 2% 
			CPPUNIT_ASSERT_EQUAL((int) (1.0/0.02), sh_offset);
		}

		void testDutyCycleGetSHOffsetFourPPLinksFromCrash() {
			unsigned int duty_cycle_periodicity = 100;
			double max_duty_cycle = 0.1;
			mac_layer_me->setDutyCycle(100, 0.1, 4);			
			mac_layer_me->setDutyCycleBudgetComputationStrategy(DutyCycleBudgetStrategy::DYNAMIC);
			std::vector<double> duty_cycle_contribs;
			duty_cycle_contribs.push_back(.025);
			duty_cycle_contribs.push_back(.0125);
			duty_cycle_contribs.push_back(.0125);
			duty_cycle_contribs.push_back(.05);
			std::vector<int> timeouts;
			timeouts.push_back(100);
			timeouts.push_back(100);
			timeouts.push_back(100);
			timeouts.push_back(100);
			CPPUNIT_ASSERT_THROW(mac_layer_me->getDutyCycle().getOffsetSH(duty_cycle_contribs), std::runtime_error);
		}

		void testMeasureTimeInbetweenBeaconReceptions() {
			size_t num_slots = 0, max_slots = 3000;
			while (mac_layer_me->stat_num_broadcasts_rcvd.get() < 10.0 && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT_GREATEREQUAL(2.0, mac_layer_me->stat_num_broadcasts_rcvd.get());
			CPPUNIT_ASSERT_GREATER(0.0, mac_layer_me->getNeighborObserver().avg_last_seen.at(partner_id).get());
			CPPUNIT_ASSERT_EQUAL(mac_layer_me->getNeighborObserver().avg_last_seen.at(partner_id).get(), mac_layer_me->getNeighborObserver().getAvgBeaconDelay());
		}

		void testDontReportMissingSHPacketToArq() {
			size_t num_slots = 0, max_slots = 250;
			// wait until the "I" have noticed "you"
			while (mac_layer_me->neighbor_observer.getNumActiveNeighbors() == 0 && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);		
			CPPUNIT_ASSERT_EQUAL(size_t(1), mac_layer_me->neighbor_observer.getNumActiveNeighbors());			
			max_slots = 1000;
			num_slots = 0;
			bool expect_missing_packet = false;
			// drop all packets going A->B
			env_you->phy_layer->connected_phys.clear();
			while (!expect_missing_packet && num_slots++ < max_slots) {			
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				if (mac_layer_me->getReservationManager()->getBroadcastReservationTable()->getReservation(0).isRx())				
					expect_missing_packet = true;				
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
				if (expect_missing_packet) 					
					CPPUNIT_ASSERT_EQUAL(false, sh_me->reported_missing_packet_to_arq);											
			}		
			CPPUNIT_ASSERT_EQUAL(true, expect_missing_packet);
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		}

	CPPUNIT_TEST_SUITE(SystemTests);
		CPPUNIT_TEST(testLinkEstablishment);		
		CPPUNIT_TEST(testCommunicateInOtherDirection);
		CPPUNIT_TEST(testCommunicateReverseOrder);
		CPPUNIT_TEST(testSimultaneousRequests);		
		CPPUNIT_TEST(testManyReestablishments);				
		CPPUNIT_TEST(testMACDelays);								
		CPPUNIT_TEST(testMissedLastLinkEstablishmentOpportunity);										
		CPPUNIT_TEST(testDutyCycleContributions);
		CPPUNIT_TEST(testDutyCyclePeriodicityPP);
		CPPUNIT_TEST(testDutyCyclePeriodicityPPOnlyOneLinkNeeded);	
		CPPUNIT_TEST(testDutyCycleSHBudgetOnlyOneLinkNeeded);		
		CPPUNIT_TEST(testDutyCyclePeriodicityPPOneLinkUsed);
		CPPUNIT_TEST(testDutyCyclePeriodicityPPTwoLinksUsed);
		CPPUNIT_TEST(testDutyCyclePeriodicityPPThreeLinksUsed);
		CPPUNIT_TEST(testDutyCyclePeriodicityPPThreeLinksUsedFromCrash);	
		CPPUNIT_TEST(testDutyCyclePeriodicityPPFourLinksUsed);		
		CPPUNIT_TEST(testDutyCyclePeriodicityPPNoBudget);
		CPPUNIT_TEST(testDutyCycleLastLink);
		CPPUNIT_TEST(testDutyCycleSHTakesAll);		
		CPPUNIT_TEST(testDutyCycleGetSHOffsetNoPPLinks);				
		CPPUNIT_TEST(testDutyCycleGetSHOffsetOnePPLinks);						
		CPPUNIT_TEST(testDutyCycleGetSHOffsetTwoPPLinks);
		CPPUNIT_TEST(testDutyCycleGetSHOffsetThreePPLinks);
		CPPUNIT_TEST(testDutyCycleGetSHOffsetFourPPLinks);	
		CPPUNIT_TEST(testDutyCycleGetSHOffsetFourPPLinksFromCrash);	
		CPPUNIT_TEST(testMeasureTimeInbetweenBeaconReceptions);			
		CPPUNIT_TEST(testDontReportMissingSHPacketToArq);			
	CPPUNIT_TEST_SUITE_END();
	};
}