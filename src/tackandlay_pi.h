/******************************************************************************
 * $Id: tackandlay_pi.h,v 1.8 plagiarized by D Cowell
 *
 * Project:  OpenCPN
 * Purpose:  tackandlay Plugin
 *
 ***************************************************************************
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
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************
 */

#ifndef _tackandlayPI_H_
#define _tackandlayPI_H_

#include "wx/wxprec.h"

#include "version.h"
#include "wx/wx.h"
#include <wx/glcanvas.h>
#include "ocpnDC.h"

#ifndef  WX_PRECOMP
  #include "wx/wx.h"
#endif //precompiled headers

#define     MY_API_VERSION_MAJOR    1
#define     MY_API_VERSION_MINOR    11

#define     PLUGIN_VERSION_MAJOR    0
#define     PLUGIN_VERSION_MINOR    1

#ifndef PI
      #define PI        3.1415926535897931160E0      /* pi */
#endif

//    Forward definitions

class TnLDisplayOptionsDialog;

//----------------------------------------------------------------------------------------------------------
//    The PlugIn Class Definition
//----------------------------------------------------------------------------------------------------------

#define TACKANDLAY_TOOL_POSITION    -1          // Request default positioning of toolbar tool

class tackandlay_pi : public opencpn_plugin_111
{
public:
      tackandlay_pi(void *ppimgr);
      ~tackandlay_pi(void);

//    The required PlugIn Methods
      int Init(void);
      bool DeInit(void);

      int GetAPIVersionMajor();
      int GetAPIVersionMinor();
      int GetPlugInVersionMajor();
      int GetPlugInVersionMinor();

      wxBitmap *GetPlugInBitmap();
      wxString GetCommonName();
      wxString GetShortDescription();
      wxString GetLongDescription();
	  void ShowPreferencesDialog( wxWindow* parent );

//    To override PlugIn Methods
      void SetCursorLatLon(double lat, double lon);
      void OnContextMenuItemCallback(int id);

      void SetPositionFix(PlugIn_Position_Fix &pfix);
      void SetPositionFixEx(PlugIn_Position_Fix_Ex &pfix);
      void SetNMEASentence( wxString &sentence );

      void SetDefaults(void);
      int GetToolbarToolCount(void);
      void OnToolbarToolCallback(int id);

      double Calc_VMG(double TWA, double SOG);
      double Calc_VMC(double COG, double SOG, double BTM);
      double Polar_SOG (double TWS, double TWA);
      double Max_VMG_TWA(double TWS);
      void Draw_Line(int angle,int legnth);
      void Draw_Wind_Barb(wxPoint pp,  double true_wind, double speed);

      bool RenderOverlay(wxDC &dc, PlugIn_ViewPort *vp);
      bool RenderGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp);
      

//      void OnTnLControlDialogClose();         // Control dialog
//      void UpdateDisplayParameters(void);

	TnLDisplayOptionsDialog     *m_pOptionsDialog;
private:
//	  bool LoadConfig(void);
//      bool SaveConfig(void);

//      void CacheSetToolbarToolBitmaps(int bm_id_normal, int bm_id_rollover);
    wxFileConfig     *m_pconfig;
    wxWindow         *m_parent_window;
    wxMenu           *m_pmenu;

    int              m_display_width, m_display_height;
    int              m_tool_id;
    bool              m_bShowIcon;
    wxBitmap          *m_pdeficon;
    NMEA0183         m_NMEA0183;

//    wxLogWindow		*m_plogwin;


};
//----------------------------------------------------------------------------------------------------------
//    Tack and Lay Line DisplayOptions Dialog Specification
//----------------------------------------------------------------------------------------------------------

class TnLDisplayOptionsDialog: public wxDialog
{
      DECLARE_CLASS( TnLDisplayOptionsDialog )
      DECLARE_EVENT_TABLE()

      public:
            TnLDisplayOptionsDialog( );
            ~TnLDisplayOptionsDialog( );

            bool Create(  wxWindow *parent, tackandlay_pi *ppi);

            void load_POL_file();
            void Render_Polar();
	        void createDiagram(wxDC& dc);
            void OnPaintPolar( wxPaintEvent& event );
		    void OnSizePolar( wxSizeEvent& event );
            void OnSizePolarDlg( wxSizeEvent& event );

	        void createSpeedBullets();

      private:
            void OnClose(wxCloseEvent& event);
            void OnIdOKClick( wxCommandEvent& event );

            wxWindow        *pParent;
            tackandlay_pi   *pPlugIn;

            wxColour		windColour[10];
            wxBoxSizer      *bSizerNotebook;
            wxNotebook      *m_notebook;
            wxPanel         *m_panelPolar;
            wxDC            *dc;

            wxSize			max_dimension;
            wxPoint         center;
	        int				radius;
	        double			image_pixel_height[24];                 // display height in pixels
	        int             display_speed;
            double			pixels_knot_ratio;
            // DisplayOptions
	  wxTextCtrl        *pText_Tack_Angle_Polar;
};


static double deg2rad(double deg);
static double rad2deg(double rad);
static double local_distance (double lat1, double lon1, double lat2, double lon2);
static double local_bearing (double lat1, double lon1, double lat2, double lon2);
double VTW(double VAW, double BAW, double SOG);
double BTW(double VAW, double BAW, double SOG);
double VAW(double VTW, double BTW, double SOG);
double BAW(double VTW, double BTW,double SOG);

#endif