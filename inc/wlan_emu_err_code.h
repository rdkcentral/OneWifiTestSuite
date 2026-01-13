/**
 * Copyright 2026 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ERR_REASON_H
#define _ERR_REASON_H

#define ENONE               0    /**< No failure */

// File handling errors
#define EFOPEN              3    /**< File open failure */
#define EFREAD              4    /**< File read failure */
#define EFWRITE             5    /**< File write failure */
#define EFPRESENCE          6    /**< File presence check failure */
#define EFCOPY              7    /**< File copy failure */
#define EFDELETE            8    /**< File delete failure */
#define ESYSOPS             9    /**< System operations failure */

// Memory, Buffer, Allocation errors
#define ESNPRINTF           10   /**< String formatting failure */
#define EMALLOC             11   /**< Memory allocation failure */
#define EQALLOC             12   /**< Queue allocation failure */
#define ENULL               13   /**< Null failure */

// Webconfig and JSON errors
#define EWEBDECODE          14   /**< Web configuration decode failure */
#define EWEBSET             15   /**< Web configuration setup failure */
#define EJSONPARSE          16   /**< JSON parsing failure */

// Radio, client and Neighbor stats error
#define ERADIOSTATS         17   /**< Radio statistics failure */
#define ECLIENTSTATS        18   /**< Client statistics failure */
#define ERADIOTEMP          19   /**< Radio temperature stats failure */
#define ENEIGHSTATS         20   /**< Neighbor statistics failure */
#define EWIFISTATS          21   /**< WiFi stats failure */

// Queue and Pipe error
#define EPOPEN              22   /**< Pipe open failure */
#define EQPEEK              23   /**< Queue peek failure */
#define EQPOP               24   /**< Queue pop failure */

// Time, scheduler and thread error
#define ETIMEOUT            25   /**< Operation timeout failure */
#define ETIMESTAMP          26   /**< Current time fetch error failure */
#define ETHREAD             27   /**< Thread creation failure */
#define ESTEPTIMEOUT        28   /**< Step timeout failure */

// Steps workflow error
#define ESTEP               29   /**< Generic step failure */
#define ESTEPSTOPPED        30   /**< Invalid stop step failure */
#define EPROCSTATUS         31   /**< Process status failure */
#define EDMLRESET           32   /**< DML reset failure */
#define EENCODE             33   /**< Encode failure */

// Unknown handling error
#define EUNKNOWNTYPE        34   /**< Unknown data type failure */
#define EUNKNOWNSCAN        35   /**< Unknown scan type failure */
#define EUNSUPPORTEDSUBDOC  36   /**< Unsupported subdoc type failure */

// Bus and interface error
#define EBUS                37   /**< Bus send/subscribe failure */
#define EBRLANCFG           38   /**< brlan0 configuration update failure */
#define EBSSID              39   /**< BSSID updation failure */
#define EFRAMECAP           40   /**< Frame capture failure */
#define EEXTAGENT           41   /**< External agent failure */

// Test result handling error
#define EDNLDTSTRESFILE     42   /**< Test result download file failure */
#define EPUSHTSTRESFILE     43   /**< Test result upload file failure */

//Curl
#define ECURLGET            44   /**< CURL GET failure */
#define ECURLPOST           45   /**< CURL POST failure */

#endif // _ERR_REASON_H
