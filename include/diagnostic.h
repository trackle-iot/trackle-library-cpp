/**
 ******************************************************************************
  Copyright (c) 2022 IOTREADY S.r.l.

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

#include "defines.h"
#include "appender.h"

namespace diagnostic
{
	/**
	 * @brief This takes a cloud diagnostic key and a value, and adds the key and value to the diagnostic buffer
	 *
	 * @param key The key of the diagnostic.
	 * @param value The value of the diagnostic.
	 */
	void diagnosticCloud(Cloud key, double value);

	/**
	 * @brief This takes a system diagnostic key and a value, and adds the key and value to the diagnostic buffer
	 *
	 * @param key The system to be diagnosed.
	 * @param value The value of the diagnostic.
	 */
	void diagnosticSystem(System key, double value);

	/**
	 * @brief This takes a network diagnostic key and a value, and adds the key and value to the diagnostic buffer
	 *
	 * @param key The key of the diagnostic parameter.
	 * @param value The value of the parameter.
	 */
	void diagnosticNetwork(Network key, double value);

	/**
	 * It takes the diagnostic map and appends it to the appender
	 *
	 * @param appender A function pointer to the appender function.
	 * @param append This is the appender object that is passed in. It is used to append the metrics to the
	 * buffer.
	 * @param flags 0x00
	 * @param page The page number of the metrics.
	 * @param reserved reserved for future use
	 *
	 * @return A boolean value.
	 */
	bool appendMetrics(appender_fn appender, void *append, uint32_t flags, uint32_t page, void *reserved);
}