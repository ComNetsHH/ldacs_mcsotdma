// The L-Band Digital Aeronautical Communications System (LDACS) Multi Channel Self-Organized TDMA (TDMA) Library provides an implementation of Multi Channel Self-Organized TDMA (MCSOTDMA) for the LDACS Air-Air Medium Access Control simulator.
// Copyright (C) 2023  Sebastian Lindner, Konrad Fuger, Musab Ahmed Eltayeb Ahmed, Andreas Timm-Giel, Institute of Communication Networks, Hamburg University of Technology, Hamburg, Germany
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
		Reservation another_reservation = Reservation(MacId(id + 1), Reservation::Action::RX);
		CPPUNIT_ASSERT(another_reservation != *reservation);
	}

CPPUNIT_TEST_SUITE(ReservationTests);
		CPPUNIT_TEST(testConstructors);
		CPPUNIT_TEST(testBasics);
		CPPUNIT_TEST(testEqualityOperator);
	CPPUNIT_TEST_SUITE_END();
};