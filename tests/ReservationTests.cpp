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
		IcaoId owner = IcaoId(id);
	
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
			CPPUNIT_ASSERT(reservation->getOwner() == owner);
			CPPUNIT_ASSERT(reservation->getOwner() != IcaoId(id+1));
			CPPUNIT_ASSERT(reservation->getAction() == Reservation::Action::IDLE);
			reservation->setAction(Reservation::Action::TX);
			CPPUNIT_ASSERT(reservation->getAction() != Reservation::Action::IDLE);
			CPPUNIT_ASSERT(reservation->getAction() == Reservation::Action::TX);
		}
		
	CPPUNIT_TEST_SUITE(ReservationTests);
		CPPUNIT_TEST(testConstructors);
		CPPUNIT_TEST(testBasics);
	CPPUNIT_TEST_SUITE_END();
};