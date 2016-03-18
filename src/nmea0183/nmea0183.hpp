/***************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  NMEA0183 Support Classes
 * Author:   Samuel R. Blackburn, David S. Register
 *
 ***************************************************************************
 *   Copyright (C) 2010 by Samuel R. Blackburn, David S Register           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.             *
 ***************************************************************************
 *
 *   S Blackburn's original source license:                                *
 *         "You can use it any way you like."                              *
 *   More recent (2010) license statement:                                 *
 *         "It is BSD license, do with it what you will"                   *
 */


#if ! defined( NMEA_0183_CLASS_HEADER )
#define NMEA_0183_CLASS_HEADER

/*
** Author: Samuel R. Blackburn
** CI$: 76300,326
** Internet: sammy@sed.csc.com
**
** You can use it any way you like.
*/

/*
** General Purpose Classes
*/

#include "sentence.hpp"
#include "response.hpp"
#include "latlong.hpp"
//#include "lorantd.hpp"
//#include "manufact.hpp"
//#include "mlist.hpp"
//#include "omegapar.hpp"
//#include "deccalop.hpp"
//#include "ratiopls.hpp"
//#include "radardat.hpp"
//#include "freqmode.hpp"

/*
** response classes
*/

/*
#include "aam.hpp"
#include "alm.hpp"
#include "apb.hpp"
#include "asd.hpp"
#include "bec.hpp"
#include "bod.hpp"
#include "bwc.hpp"
#include "bwr.hpp"
#include "bww.hpp"
#include "dbt.hpp"
#include "dcn.hpp"
#include "dpt.hpp"
#include "fsi.hpp"
#include "gga.hpp"
#include "glc.hpp"
#include "gll.hpp"
#include "gxa.hpp"
#include "hsc.hpp"
#include "lcd.hpp"
#include "mtw.hpp"
#include "mwv.hpp"
#include "oln.hpp"
#include "osd.hpp"
#include "p.hpp"
#include "rma.hpp"
*/
#include "hdm.hpp"
#include "hdg.hpp"
#include "hdt.hpp"
#include "rmb.hpp"
#include "rmc.hpp"
#include "rsa.hpp"
#include "wpl.hpp"
#include "rte.hpp"
#include "gll.hpp"
#include "vtg.hpp"
#include "gsv.hpp"
#include "gga.hpp"
#include "dbt.hpp"
#include "dpt.hpp"
#include "mta.hpp" //air temperature
#include "mtw.hpp" //water temperature
#include "mda.hpp" //barometric pressure
#include "mwd.hpp"
#include "mwv.hpp"
#include "vhw.hpp"
#include "vwr.hpp"
#include "vwt.hpp"
#include "zda.hpp"
#include "vlw.hpp"
/*
#include "rot.hpp"
#include "rpm.hpp"
#include "rsd.hpp"
#include "sfi.hpp"
#include "stn.hpp"
#include "trf.hpp"
#include "ttm.hpp"
#include "vbw.hpp"
#include "vdr.hpp"
#include "vhw.hpp"
#include "vlw.hpp"
#include "vpw.hpp"
#include "vtg.hpp"
#include "wcv.hpp"
#include "wnc.hpp"
#include "xdr.hpp"
#include "xte.hpp"
#include "xtr.hpp"
#include "zda.hpp"
#include "zfo.hpp"
#include "ztg.hpp"
*/

WX_DECLARE_LIST(RESPONSE, MRL);

class NMEA0183
{

   private:

      SENTENCE sentence;

      void initialize( void );

   protected:

      MRL response_table;

      void set_container_pointers( void );
      void sort_response_table( void );

   public:

      NMEA0183();
      virtual ~NMEA0183();

      /*
      ** NMEA 0183 Sentences we understand
      */

/*
      AAM Aam;
      ALM Alm;
      APB Apb;
      ASD Asd;
      BEC Bec;
      BOD Bod;
      BWC Bwc;
      BWR Bwr;
      BWW Bww;
*/
      DBT Dbt;
/*
      DCN Dcn;
*/
      DPT Dpt;
/*
      FSI Fsi;
*/
      GGA Gga;
/*
      GLC Glc;
*/
      GLL Gll;
      GSV Gsv;
/*
      GXA Gxa;
*/
      HDM Hdm;
      HDG Hdg;
      HDT Hdt;
/*
      HSC Hsc;
      LCD Lcd;
*/
      MTA Mta; //Air temperature
      MTW Mtw;
      MWD Mwd;
      MWV Mwv;
      MDA Mda; //Metrological comopsite	
/*
      OLN Oln;
      OSD Osd;
      P   Proprietary;
      RMA Rma;
*/
      RMB Rmb;
      RMC Rmc;
/*
      ROT Rot;
      RPM Rpm;
*/
      RSA Rsa;
/*
      RSD Rsd;
*/
      RTE Rte;
/*
      SFI Sfi;
      STN Stn;
      TRF Trf;
      TTM Ttm;
      VBW Vbw;
      VDR Vdr;
*/
      VHW Vhw;
	  VLW Vlw;
/*
      
      VPW Vpw;
*/
      VTG Vtg;
      VWR Vwr;
      VWT Vwt;
/*
      WCV Wcv;
      WNC Wnc;
*/
      WPL Wpl;
/*
      XDR Xdr;
      XTE Xte;
      XTR Xtr;
*/
      ZDA Zda;
/*
      ZFO Zfo;
      ZTG Ztg;
*/
      wxString ErrorMessage; // Filled when Parse returns FALSE
      wxString LastSentenceIDParsed; // ID of the lst sentence successfully parsed
      wxString LastSentenceIDReceived; // ID of the last sentence received, may not have parsed successfully

      wxString TalkerID;
      wxString ExpandedTalkerID;

//      MANUFACTURER_LIST Manufacturers;

      bool IsGood( void ) const;
      bool Parse( void );
      bool PreParse( void );

      NMEA0183& operator << ( wxString& source );
      NMEA0183& operator >> ( wxString& destination );
};

#endif // NMEA_0183_CLASS_HEADER
