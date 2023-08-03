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

const uint8_t HARDCODED_DEVICE_ID[DEVICE_ID_LENGTH] = {
    0x10, 0xaf, 0x26, 0x43, 0x74, 0xed, 0x83, 0x43, 0x02, 0xae, 0xb9, 0x84};

const uint8_t HARDCODED_PRIVATE_KEY[PRIVATE_KEY_LENGTH] = {
    0x30, 0x77, 0x02, 0x01, 0x01, 0x04, 0x20, 0x74, 0xfb, 0x02, 0x49, 0x14,
    0x64, 0x28, 0x11, 0xcd, 0xcc, 0xbc, 0xab, 0xf0, 0x1c, 0xef, 0x6b, 0xac,
    0x3a, 0xf2, 0x5e, 0xdd, 0x4e, 0xa6, 0x15, 0xfd, 0xe0, 0x08, 0x9a, 0xa0,
    0xbd, 0x33, 0x0a, 0xa0, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d,
    0x03, 0x01, 0x07, 0xa1, 0x44, 0x03, 0x42, 0x00, 0x04, 0xc5, 0x5f, 0xe5,
    0xa8, 0xe0, 0xc6, 0xb3, 0xc7, 0xe6, 0x6f, 0x34, 0x18, 0xd7, 0x18, 0xdc,
    0x2c, 0xd7, 0x6f, 0x43, 0xdc, 0x47, 0x96, 0x31, 0x4d, 0xb2, 0xd6, 0x4e,
    0xab, 0xa5, 0x80, 0x4a, 0xd1, 0xd6, 0x9b, 0x18, 0x25, 0x0c, 0x90, 0xc1,
    0x07, 0x9b, 0xfd, 0x92, 0x3a, 0x12, 0x85, 0x29, 0x8f, 0x87, 0x8e, 0x01,
    0x82, 0x4e, 0xe6, 0x5c, 0xd8, 0x0e, 0xdf, 0x54, 0x6d, 0xa5, 0xba, 0x76,
    0xd6};

#endif
