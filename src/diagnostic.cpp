#include "diagnostic.h"
#include <map>
#include <vector>
#include <math.h>
#include <esp_log.h>

std::map<int16_t, int32_t> diagnostic_array;

/**
 * It converts a floating point number to a fixed point number
 *
 * @param value the value to be converted
 * @param shift_bytes the number of bytes to shift the integer part of the float value.
 *
 * @return a 32-bit integer.
 */
int32_t trans_float_diag(double value, int shift_bytes)
{
	double fractpart, intpart;
	fractpart = modf(value, &intpart);
	int32_t result = 0;
	if (shift_bytes == 1)
	{
		result = ((int32_t)(intpart) << 8) | ((int32_t)(fractpart * 256) & 0xff);
	}
	else if (shift_bytes == 2)
	{
		result = ((int32_t)(intpart) << 16) | ((int32_t)(fractpart * 256 * 256) & 0xffff);
	}
	return result;
}

void addDiagnostic(int key, int32_t value)
{
	ESP_LOGI("DIAG", "addDiagnostic %d: %d", key, value);
	diagnostic_array[key] = value;
}

int32_t getDiagnostic(int key)
{
	return diagnostic_array[key];
}

namespace diagnostic
{

	void diagnosticCloud(Cloud key, double value)
	{
		int32_t right_value = (int32_t)value;
		if (key == CLOUD_DISCONNECTS || key == CLOUD_CONNECTION_ATTEMPTS || key == CLOUD_UNACKNOWLEDGED_MESSAGES)
		{
			// must increase current value
			right_value = getDiagnostic(key) + value;
		}
		addDiagnostic(key, (int32_t)right_value);
	}

	void diagnosticSystem(System key, double value)
	{
		int32_t right_value = (int32_t)value;
		if (key == SYSTEM_BATTERY_CHARGE)
		{
			right_value = trans_float_diag(value, 1);
		}
		addDiagnostic(key, right_value);
	}

	void diagnosticNetwork(Network key, double value)
	{
		int32_t right_value = (int32_t)value;
		if (key == NETWORK_COUNTRY_CODE)
		{
			if (right_value < 100)
				right_value = right_value * -1; // fix 2 digit format mnc
		}
		else if (key == NETWORK_RSSI || key == NETWORK_SIGNAL_STRENGTH || key == NETWORK_SIGNAL_QUALITY)
		{
			right_value = trans_float_diag(value, 1);
		}
		else if (key == NETWORK_SIGNAL_STRENGTH_VALUE || key == NETWORK_SIGNAL_QUALITY_VALUE)
		{
			right_value = trans_float_diag(value, 2);
		}
		else if (key == NETWORK_DISCONNECTS || key == NETWORK_CONNECTION_ATTEMPTS)
		{
			// must increase current value
			right_value = getDiagnostic(key) + value;
		}
		addDiagnostic(key, right_value);
	}

	bool appendMetrics(appender_fn appender, void *append, uint32_t flags, uint32_t page, void *reserved)
	{
		std::map<int16_t, int32_t>::iterator itr;
		for (itr = diagnostic_array.begin(); itr != diagnostic_array.end(); ++itr)
		{
			((Appender *)append)->append((char)(itr->first >> 0 & 0xff));
			((Appender *)append)->append((char)(itr->first >> 8 & 0xff));
			((Appender *)append)->append((char)(itr->second >> 0 & 0xff));
			((Appender *)append)->append((char)(itr->second >> 8 & 0xff));
			((Appender *)append)->append((char)(itr->second >> 16 & 0xff));
			((Appender *)append)->append((char)(itr->second >> 24 & 0xff));
		}

		return true;
	}
}