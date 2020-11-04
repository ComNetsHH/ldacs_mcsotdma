//
// Created by Sebastian Lindner on 04.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_USERID_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_USERID_HPP

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	/**
	 * A user's ID uniquely identifies a network user.
	 */
	class UserId {
		public:
			explicit UserId(int id) : id(id) {}
			
			virtual bool operator==(const UserId& other) const {
				return this->id == other.id;
			}
			
			virtual bool operator!=(const UserId& other) const {
				return !(*this == other);
			}
		
		protected:
			int id;
	};
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_USERID_HPP
