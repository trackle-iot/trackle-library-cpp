/**
 ******************************************************************************
  Copyright (c) 2022 IOTREADY S.r.l.
  Copyright (c) 2015 Particle Industries, Inc.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation, either
  version 3 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************
 */

#pragma once

#include "protocol_defs.h"
#include "service_debug.h"

namespace trackle
{
    namespace protocol
    {

        class TimeSyncManager
        {
        public:
            TimeSyncManager()
                : lastSyncMillis_{0},
                  requestSentMillis_{0},
                  lastSyncTime_{0},
                  expectingResponse_{false}
            {
            }

            void reset()
            {
                expectingResponse_ = false;
                requestSentMillis_ = 0;
            }

            template <typename Callback>
            bool send_request(system_tick_t mil, Callback send_time_request)
            {
                if (expectingResponse_)
                {
                    return true;
                }

                requestSentMillis_ = mil;
                expectingResponse_ = true;
                LOG(INFO, "Sending TIME request");
                return send_time_request();
            }

            template <typename Callback>
            bool handle_time_response(time_t tm, system_tick_t mil, Callback set_time)
            {
                LOG(INFO, "Received TIME response: %lu", (unsigned long)tm);
                set_time(tm, 0, NULL);
                expectingResponse_ = false;
                lastSyncTime_ = tm;
                lastSyncMillis_ = mil;
                return true;
            }

            bool is_request_pending() const
            {
                return expectingResponse_;
            }

            system_tick_t last_sync(time_t &tm) const
            {
                tm = lastSyncTime_;
                return lastSyncMillis_;
            }

        private:
            system_tick_t lastSyncMillis_;
            system_tick_t requestSentMillis_;
            time_t lastSyncTime_;
            bool expectingResponse_;
        };

    }
} // namespace trackle::protocol
