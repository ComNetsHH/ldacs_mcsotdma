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
		LinkId* owner;
		int id = 42;
	
	public:
		void setUp() override {
			owner = new LinkId(id);
			reservation = new Reservation(*owner);
		}
		
		void tearDown() override {
			delete reservation;
			delete owner;
		}
		
		void testBasics() {
			CPPUNIT_ASSERT(reservation->getOwner() == *owner);
			CPPUNIT_ASSERT(reservation->getOwner() != LinkId(id+1));
			CPPUNIT_ASSERT(reservation->getAction() == Reservation::Action::UNSET);
			reservation->setAction(Reservation::Action::TX);
			CPPUNIT_ASSERT(reservation->getAction() != Reservation::Action::UNSET);
			CPPUNIT_ASSERT(reservation->getAction() == Reservation::Action::TX);
		}
		
	CPPUNIT_TEST_SUITE(ReservationTests);
		CPPUNIT_TEST(testBasics);
	CPPUNIT_TEST_SUITE_END();
};