/**
 ******************************************************************************
  Copyright (c) 2022 IOTREADY S.r.l.

  This software is free software; you can redistribute it and/or
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

#ifndef TRACKLE_HARDCODED_CREDENTIALS_H
#define TRACKLE_HARDCODED_CREDENTIALS_H

// Standard library includes
#include <inttypes.h>

// Trackle library includes
#include <defines.h>

/**
 * To generate a private key-device ID pair:
 *  - Connect to your Trackle dashboard;
 *  - Click on "Claim di un dispositivo" in the top right corner;
 *  - Click on "Non hai un ID dispositivo?" in the window that opens;
 *  - Click on "Continua" in the next window;
 *  - Annotate the device ID and keep the private key file with ".der" extension that will be downloaded;
 *  - Fill the HARDCODED_DEVICE_ID array with the device ID from the previous step converted in this way: "0f4a12..." -> "0x0f, 0x4a, 0x12, ..."
 *  - Convert the previous private key file to C literal using the command: "cat private_key.der | xxd -i";
 *  - Copy the C literal version of the key (e.g. 0x5f, 0x91, ...) from the previous command output into the HARDCODED_PRIVATE_KEY array.
 *  - Remove the #error directive that prevents the firmware from building successfully.
 */

#error "Did you put the Trackle credentials in 'trackle_hardcoded_credentials.h' ?"

const uint8_t HARDCODED_DEVICE_ID[DEVICE_ID_LENGTH] = {
    // Device ID goes here (12 bytes, e.g. 0x12, 0xF4, etc.)
};

const uint8_t HARDCODED_PRIVATE_KEY[PRIVATE_KEY_LENGTH] = {
    // Private key goes here (121 bytes, e.g. 0x0E, 0x4A, etc.)
};

#endif
