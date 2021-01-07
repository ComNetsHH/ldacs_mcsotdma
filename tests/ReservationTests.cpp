//
// Created by Sebastian Lindner on 04.11.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../Reservation.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

class ReservationTests : public CppUnit::TestFixture {
	private:
		Reservation* reservation;
		int id = 42;
		MacId owner = MacId(id);
	
	public:
		void setUp() override {
			reservation = new Reservation(owner);
		}
		
		void tearDown() override {
			delete reservation;
		}
		
		void testConstructors() {
			Reservation res1 = Reservation();
			Reservation res2 = Reservation(owner);
			Reservation res3 = Reservation(owner, Reservation::Action::IDLE);
			std::vector<Reservation> vec = std::vector<Reservation>(10000);
		}
		
		void testBasics() {
			CPPUNIT_ASSERT(reservation->getTarget() == owner);
			CPPUNIT_ASSERT(reservation->getTarget() != MacId(id + 1));
			CPPUNIT_ASSERT_EQUAL(true, reservation->isIdle());
			reservation->setAction(Reservation::Action::TX);
			CPPUNIT_ASSERT_EQUAL(false, reservation->isIdle());
			CPPUNIT_ASSERT_EQUAL(true, reservation->isTx());
		}
		
		void testEqualityOperator() {
			reservation->setAction(Reservation::Action::RX);
			Reservation other_reservation = Reservation(owner, Reservation::Action::RX);
			CPPUNIT_ASSERT(other_reservation == *reservation);
			other_reservation.setAction(Reservation::Action::TX);
			CPPUNIT_ASSERT(other_reservation != *reservation);
			other_reservation.setAction(Reservation::Action::RX);
			Reservation another_reservation = Reservation(MacId(id+1), Reservation::Action::RX);
			CPPUNIT_ASSERT(another_reservation != *reservation);
		}
		
	CPPUNIT_TEST_SUITE(ReservationTests);
		CPPUNIT_TEST(testConstructors);
		CPPUNIT_TEST(testBasics);
		CPPUNIT_TEST(testEqualityOperator);
	CPPUNIT_TEST_SUITE_END();
};