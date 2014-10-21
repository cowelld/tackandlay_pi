/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Tack Angle and Lay Line Plugin
 * Author:  	Dave Cowell
 *
 ***************************************************************************
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

#include "wx/wxprec.h"

#ifndef  WX_PRECOMP      //precompiled headers
  #include "wx/wx.h"
#endif

#include "nmea0183.h"
#include <wx/mstream.h>         // for icon
#include "ocpn_plugin.h"

#include "tackandlay_pi.h"
using namespace std;

#ifdef __WXMSW__
    #include "GL/glu.h"
#endif


;
enum {										// process ID's
        ID_OK,
        ID_OVERLAYDISPLAYOPTION,
        ID_DISPLAYTYPE
};

double tnl_overlay_transparency;
int  tnl_polar_calibration[180][6];		// polar diagram represents 180 derees and 6,8,10,12,16,20 knots of wind
bool   tnl_bpos_set;
double tnl_ownship_lat, tnl_ownship_lon;
double cur_lat, cur_lon;
double tnl_hdm, tnl_hdt;
double tnl_sog, tnl_cog;
double tnl_mark_brng, tnl_mark_rng;
double hdt_last_message, sog_last_message;
double tack_angle, lay_angle, wind_dir_true, wind_speed;
double lgth_line;

bool  tnl_shown_dc_message;
wxTextCtrl        *plogtc;
wxDialog          *plogcontainer;

wxPoint boat_center, mark_center;
double temp_mark_lat = 0.0,temp_mark_lon = 0.0;

PlugIn_Route *m_pRoute = NULL;
PlugIn_Waypoint *m_pMark = NULL;


extern "C" DECL_EXP opencpn_plugin* create_pi(void *ppimgr)
{
    return new tackandlay_pi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p)
{
    delete p;
}

//---------------------------------------------------------------------------------------------------------
//
//    TnL PlugIn Implementation
//
//---------------------------------------------------------------------------------------------------------

tackandlay_pi::tackandlay_pi(void *ppimgr)
      :opencpn_plugin_18(ppimgr)
{
    wxMemoryInputStream _tnl("\211PNG\r\n\032\n\000\000\000\rIHDR\000\000\000 \000\000\000 \b\006\000\000\000szz\364\000\000\000\004sBIT\b\b\b\b|\bd\210\000\000\000\011pHYs\000\000\r\327\000\000\r\327\001B(\233x\000\000\000\031tEXtSoftware\000www.inkscape.org\233\356<\032\000\000\000\304IDATX\205\355\3261\016AA\020\306\361\377\274\347\002^\247{W\320\211\003(\034@\245Qm'q\004\275J\245\323+\204F\341\006\016!\032y\255V\"\214\013X\317n\304*f\352\315|\277d2\331\021U%eeI\323\r`\000\240Q\373\302I\013\030\002\035\240\004\n\340\014\234Pv\334\330\260\324k,@\274k8\225\214\212\0310\006\3627=.\b#\026\272\215\001\370GP1\a&5\341\000M\2245Nz1\200\327#\030HNA\027\345\370q'\241\017\354\277\003X\351\035'\355\240N\017\312\320p\370\203-0\200\001\014`\000\003\370\357\001%\364{=\304\000\374\367\300\217*\371\b\014`\000\003$\a<\001z\344&\362\346\013\372\337\000\000\000\000IEND\256B`\202", 327);
    m_pdeficon = new wxBitmap(_tnl);
}
tackandlay_pi::~tackandlay_pi(void)
{
}

int tackandlay_pi::Init(void)
{

  tnl_overlay_transparency = .50; 

//********************************************************************************************************
//   Logbook window
    m_plogwin = new wxLogWindow(NULL, _T("TnL Event Log"));
    plogcontainer = new wxDialog(NULL, -1, _T("TnL Log"), wxPoint(0,0), wxSize(600,400),
				    wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER );

    plogtc = new wxTextCtrl(plogcontainer, -1, _T(""), wxPoint(0,0), wxSize(600,400), wxTE_MULTILINE );

    wxLogMessage(_T("TnL log opened"));

    ::wxDisplaySize(&m_display_width, &m_display_height);

//****************************************************************************************
    m_pconfig = GetOCPNConfigObject();
    m_parent_window = GetOCPNCanvasWindow();

//      LoadConfig();

      //    PlugIn toolbar icon
    m_tool_id  = InsertPlugInTool(_T(""), m_pdeficon, m_pdeficon, wxITEM_NORMAL,
            _T("TnL Line"), _T(""), NULL,
            TACKANDLAY_TOOL_POSITION, 0, this);

//            CacheSetToolbarToolBitmaps( 1, 1);

  // Context menue for making marks    
    m_pmenu = new wxMenu(); 
    // this is a dummy menu required by Windows as parent to item created
    wxMenuItem *pmi = new wxMenuItem( m_pmenu, -1, _T("Set Mark Position..."));
    int miid = AddCanvasContextMenuItem(pmi, this);
    SetCanvasContextMenuItemViz(miid, true);

    return (
            WANTS_DYNAMIC_OPENGL_OVERLAY_CALLBACK |
            WANTS_OPENGL_OVERLAY_CALLBACK |
            WANTS_OVERLAY_CALLBACK     |
            WANTS_CURSOR_LATLON        |
            WANTS_TOOLBAR_CALLBACK     |
            INSTALLS_TOOLBAR_TOOL      |
            INSTALLS_CONTEXTMENU_ITEMS |
            WANTS_CONFIG               |
            WANTS_NMEA_SENTENCES       |
            WANTS_NMEA_EVENTS          |
            WANTS_PREFERENCES
            );
}

bool tackandlay_pi::DeInit(void)
{
        //      SaveConfig();
    if (m_pRoute){
        m_pRoute->pWaypointList->DeleteContents(true);
        DeletePlugInRoute( m_pRoute->m_GUID );
    }
        RemovePlugInTool( m_tool_id );
        return true;
}

int tackandlay_pi::GetAPIVersionMajor()
{
      return MY_API_VERSION_MAJOR;
}

int tackandlay_pi::GetAPIVersionMinor()
{
      return MY_API_VERSION_MINOR;
}

int tackandlay_pi::GetPlugInVersionMajor()
{
      return PLUGIN_VERSION_MAJOR;
}

int tackandlay_pi::GetPlugInVersionMinor()
{
      return PLUGIN_VERSION_MINOR;
}

wxBitmap *tackandlay_pi::GetPlugInBitmap()
{
      return m_pdeficon;
}

wxString tackandlay_pi::GetCommonName()
{
      return _T("TnL Lines");
}


wxString tackandlay_pi::GetShortDescription()
{
      return _("T and L Lines PlugIn");
}


wxString tackandlay_pi::GetLongDescription()
{
      return _T(" Tack and Lay Lines PlugIn for OpenCPN\n");

}


void tackandlay_pi::SetDefaults(void)  //This will be called upon enabling a PlugIn via the user Dialog
                                          //It gives a chance to setup any default options and behavior
{
      // If the config somehow says NOT to show the icon, override it so the user gets good feedback
      if(!m_bShowIcon)
      {
            m_bShowIcon = true;

            m_tool_id  = InsertPlugInTool(_T(""), m_pdeficon, m_pdeficon, wxITEM_CHECK,
                  _T("TnL Lines"), _T(""), NULL, TACKANDLAY_TOOL_POSITION, 0, this);
      }

}

void tackandlay_pi::ShowPreferencesDialog(  wxWindow* parent )
{
		m_pOptionsDialog = new TnLDisplayOptionsDialog;
        m_pOptionsDialog->Create(m_parent_window, this);
		m_pOptionsDialog->Show();
}
//*******************************************************************************
// ToolBar Actions

int tackandlay_pi::GetToolbarToolCount(void)
{
      return 1;
}

void tackandlay_pi::OnToolbarToolCallback(int id)
{
    if ( id == m_tool_id )
    {
       if(!m_pRoute->pWaypointList->empty())
       {
               m_pRoute->pWaypointList->DeleteContents(true);
               delete m_pRoute;
       }
    }
}
/*****************************************************************************

bool tackandlay_pi::LoadConfig(void)
{

      wxFileConfig *pConf = (wxFileConfig *)m_pconfig;

      if(pConf)
      {
            pConf->Read ( _T( "TnLTransparency" ),  &tnl_overlay_transparency, .50 );
            pConf->Read ( _T( "TnL Polar Calibration" ),  &tnl_polar_calibration, 1.0 );
            return true;
      }
      else
            return false;

}

bool tackandlay_pi::SaveConfig(void)
{
      wxFileConfig *pConf = (wxFileConfig *)m_pconfig;

      if(pConf)
      {
            pConf->Write ( _T( "TnLTransparency" ),  &tnl_overlay_transparency, .50 );
            pConf->Write ( _T( "TnL Polar Calibration" ),  &tnl_polar_calibration, 1.0 );
            return true;
      }
      else
            return false;

      return true;
}
*/
//***************************/

//Cursor position data 
void tackandlay_pi::SetCursorLatLon(double lat, double lon)
{
cur_lat = lat;
cur_lon = lon;
}
// Set MARK Position
void tackandlay_pi::OnContextMenuItemCallback(int id)
{
    if (m_pRoute == NULL)
    {
        m_pRoute = new PlugIn_Route();
        m_pRoute->m_NameString = _T("TNL Route");
        m_pRoute->m_GUID = _T("TNL ") + wxDateTime::Now().Format();
    }

    m_pRoute->pWaypointList->Clear();
    PlugIn_Waypoint *m_pMark = new PlugIn_Waypoint(cur_lat,cur_lon,_T("triangle"),_T("TnL Mark"),_T("TNL"));
    temp_mark_lat = cur_lat;
    temp_mark_lon = cur_lon;
    m_pMark->m_CreateTime = wxDateTime::Now();
    m_pRoute->pWaypointList->Append(m_pMark);

    if(!UpdatePlugInRoute(m_pRoute))
    {
        AddPlugInRoute(m_pRoute,false);
    }
 
}

// Boat Position Data passed from NMEA to plugin
void tackandlay_pi::SetPositionFix(PlugIn_Position_Fix &pfix)
{
      tnl_ownship_lat = pfix.Lat;
      tnl_ownship_lon = pfix.Lon;

      tnl_bpos_set = true;

}
void tackandlay_pi::SetPositionFixEx(PlugIn_Position_Fix_Ex &pfix)
{
      tnl_ownship_lat = pfix.Lat;
      tnl_ownship_lon = pfix.Lon;

      tnl_hdt = pfix.Hdt;
      if(wxIsNaN(tnl_hdt))
		  tnl_hdt = pfix.Cog;
      if(wxIsNaN(tnl_hdt)) tnl_hdt = 0.;
      if(tnl_hdt != hdt_last_message)
		  hdt_last_message = tnl_hdt;

	  tnl_sog = pfix.Sog;     
      if(wxIsNaN(tnl_sog)) tnl_sog = 0.;
      if(tnl_sog != sog_last_message)
		  sog_last_message = tnl_sog;


      tnl_bpos_set = true;

}

void tackandlay_pi::SetNMEASentence( wxString &sentence )
{
    m_NMEA0183 << sentence;
    if( m_NMEA0183.Parse()  == true)
    {
        if( m_NMEA0183.LastSentenceIDParsed == _T("MWV") )
        {
            double wind_speed = m_NMEA0183.Mwv.WindSpeed;

            if(  wind_speed < 100. )
            {               
                if (m_NMEA0183.Mwv.WindSpeedUnits == _T("K"))
                {
                    wind_speed = wind_speed / 1.852;
                }
                else if (m_NMEA0183.Mwv.WindSpeedUnits == _T("M"))
                {
                    wind_speed = wind_speed / 1.1515;
                }

                if(m_NMEA0183.Mwv.Reference == _T("R"))
                {
                   wind_dir_true = rad2deg(BTW(wind_speed, deg2rad(m_NMEA0183.Mwv.WindAngle), tnl_sog)) ;
                }
                else
                {
                   wind_dir_true = int(m_NMEA0183.Mwv.WindAngle) % 360;
                }
            }
        }
    }
}

bool tackandlay_pi::RenderOverlay(wxDC &dc, PlugIn_ViewPort *vp)
{
    if(0 == tnl_shown_dc_message) {

//        ::wxMessageBox(message, _T("br24radar message"), wxICON_INFORMATION | wxOK, GetOCPNCanvasWindow());

        tnl_shown_dc_message = 1;

    wxString message(_("The Tack and Layline PlugIn requires OpenGL mode to be activated in Toolbox->Settings"));
	wxMessageDialog dlg(GetOCPNCanvasWindow(),  message, _T("TnL message"), wxOK);
	dlg.ShowModal();

    }

    return false;

}
// Called by Plugin Manager on main system process cycle

bool tackandlay_pi::RenderGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp)
{
   tnl_shown_dc_message = 0;             // show message box if RenderOverlay() is called again
		   wxPoint pp;
   // this is expected to be called at least once per second

	   if (tnl_bpos_set)
	   {
		   GetCanvasPixLL(vp, &pp, tnl_ownship_lat, tnl_ownship_lon);
		   boat_center = pp;										
	   }

      glPushAttrib(GL_COLOR_BUFFER_BIT | GL_LINE_BIT | GL_HINT_BIT);      //Save state
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glPushMatrix();

      glTranslated( boat_center.x, boat_center.y, 0);

      glRotatef(tnl_hdt - 90, 0, 0, 1);        //correction for boat heading -90 for base north

      // scaling...
//      double scale_factor =  v_scale_ppm / radar_pixels_per_meter;	// screen pix/radar pix

//      glScaled( scale_factor, scale_factor, 1. );

	   GLubyte red(0), green(255), blue(0), alpha(255);

		tack_angle = 40;
		lgth_line = 100;
		lay_angle = 40;

		glColor4ub(0, 255, 0, 255);	// red, green, blue,  alpha
		Draw_Line(wind_dir_true + tack_angle, lgth_line);  // angle, lgth_line
		Draw_Line(wind_dir_true - tack_angle, lgth_line);  // angle, lgth_line


        glPopMatrix();
        glPushMatrix();
		if(temp_mark_lat > 0.0)
            {
                GetCanvasPixLL(vp, &mark_center, temp_mark_lat, temp_mark_lon);
                Draw_Wind_Barb(mark_center, wind_dir_true, wind_speed);
		        glTranslated( mark_center.x, mark_center.y, 0);
                glRotatef(- 90, 0, 0, 1);
                glColor4ub(255, 0, 0, 255);	// red, blue, green, alpha
                Draw_Line(wind_dir_true + tack_angle, lgth_line);
                Draw_Line(wind_dir_true - tack_angle, lgth_line);
            }

      glPopMatrix();
      glPopAttrib();

	  return true;
}

void tackandlay_pi::Draw_Line(int angle,int lgth_line)
{
	double x = cos(angle * PI / 180. ) * lgth_line;
	double y = sin(angle * PI / 180.) * lgth_line;

    glBegin(GL_LINES);
    glVertex2d(0.0, 0.0);
    glVertex2d(x,y);
    glEnd();

}

void tackandlay_pi::Draw_Wind_Barb(wxPoint pp, double true_wind, double speed)
{
    double rad_angle;
    double shaft_x, shaft_y;
    double barb_0_x, barb_0_y, barb_1_x, barb_1_y;
    double barb_2_x, barb_2_y ;
    double barb_legnth_0_x, barb_legnth_0_y, barb_legnth_1_x, barb_legnth_1_y;
    double barb_legnth_2_x, barb_legnth_2_y;
    double barb_legnth[18]= {
        0,0,5,                  // 5 knots
        0,0,10,
        0,5,10,
        0,10,10,                // 20 knots
        5,10,10,
        10,10,10                // 30 knots
    };

    int p = int(speed / 5);
    if (p > 5) p = 5;
    p = 3*p;

	glColor4ub(0, 0, 0, 255);	// red, green, blue,  alpha (byte values)
    rad_angle = deg2rad(true_wind) - PI/2;
    
    shaft_x = cos(rad_angle) * 30;
	shaft_y = sin(rad_angle) * 30;

    barb_0_x = pp.x + .6 * shaft_x;
    barb_0_y = pp.y + .6 * shaft_y;
    barb_1_x = pp.x + .8 * shaft_x;
    barb_1_y = pp.y + .8 * shaft_y;
    barb_2_x = pp.x + shaft_x;
    barb_2_y = pp.y + shaft_y;

    
    barb_legnth_0_x = cos(rad_angle + PI/4) * barb_legnth[p];
    barb_legnth_0_y = sin(rad_angle + PI/4) * barb_legnth[p];
    barb_legnth_1_x = cos(rad_angle + PI/4) * barb_legnth[p+1];
    barb_legnth_1_y = sin(rad_angle + PI/4) * barb_legnth[p+1];
    barb_legnth_2_x = cos(rad_angle + PI/4) * barb_legnth[p+2];
    barb_legnth_2_y = sin(rad_angle + PI/4) * barb_legnth[p+2];


      glBegin(GL_LINES);
        glVertex2d(pp.x, pp.y);
        glVertex2d(pp.x + shaft_x, pp.y + shaft_y);
        glVertex2d(barb_0_x, barb_0_y);
        glVertex2d(barb_0_x + barb_legnth_0_x, barb_0_y + barb_legnth_0_y);
        glVertex2d(barb_1_x, barb_1_y);
        glVertex2d(barb_1_x + barb_legnth_1_x, barb_1_y + barb_legnth_1_y);
        glVertex2d(barb_2_x, barb_2_y);
        glVertex2d(barb_2_x + barb_legnth_2_x, barb_2_y + barb_legnth_2_y);
      glEnd();
}

//*********************************************************************************
// Display Preferences Dialog
//*********************************************************************************
IMPLEMENT_CLASS ( TnLDisplayOptionsDialog, wxDialog )

BEGIN_EVENT_TABLE ( TnLDisplayOptionsDialog, wxDialog )

    EVT_CLOSE ( TnLDisplayOptionsDialog::OnClose )
    EVT_BUTTON ( ID_OK, TnLDisplayOptionsDialog::OnIdOKClick )

END_EVENT_TABLE()

TnLDisplayOptionsDialog::TnLDisplayOptionsDialog()
{
	Init();
}

 TnLDisplayOptionsDialog::~TnLDisplayOptionsDialog()
{
}

void TnLDisplayOptionsDialog::Init()
{
}

bool TnLDisplayOptionsDialog::Create( wxWindow *parent, tackandlay_pi *ppi )
{
	pParent = parent;
    pPlugIn = ppi;

//	wxDialog *PreferencesDialog = new wxDialog( parent, wxID_ANY, _("BR24 Target Display Preferences"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE );
	if ( !wxDialog::Create ( parent, wxID_ANY, _T("TnL  Characteristics"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE ) )
        return false;

    int border_size = 4;
	wxBoxSizer* topSizer = new wxBoxSizer ( wxVERTICAL );
    SetSizer ( topSizer );

    wxBoxSizer* DisplayOptionsBox = new wxBoxSizer(wxVERTICAL);
    topSizer->Add (DisplayOptionsBox, 0, wxALIGN_CENTER_HORIZONTAL|wxALL|wxEXPAND, 2 );

// Accept/Reject buttoen	
    wxStdDialogButtonSizer* DialogButtonSizer = wxDialog::CreateStdDialogButtonSizer(wxOK|wxCANCEL);
    DisplayOptionsBox->Add(DialogButtonSizer, 0, wxALIGN_RIGHT|wxALL, 5);

    DimeWindow(this);
    Fit();
    SetMinSize(GetBestSize());

    return true;
}

void TnLDisplayOptionsDialog::OnClose ( wxCloseEvent& event )
{
//     pPlugIn->SaveConfig();
}


void TnLDisplayOptionsDialog::OnIdOKClick ( wxCommandEvent& event )
{
//+     pPlugIn->SaveConfig();
}


/********************************************************************************************************/
//   Distance measurement for simple sphere
/********************************************************************************************************/
double twoPI = 2 * PI;

static double deg2rad(double deg)
{
    return (deg * PI / 180.0);
}

static double rad2deg(double rad)
{
    return (rad * 180.0 / PI);
}

static double local_distance (double lat1, double lon1, double lat2, double lon2) {
	// Spherical Law of Cosines
	double theta, dist; 

	theta = lon2 - lon1; 
	dist = sin(deg2rad(lat1)) * sin(deg2rad(lat2)) + cos(deg2rad(lat1)) * cos(deg2rad(lat2)) * cos(deg2rad(theta)); 
	dist = acos(dist);		// radians
	dist = rad2deg(dist); 
    dist = fabs(dist) * 60    ;    // nautical miles/degree
	return (dist); 
}

static double local_bearing (double lat1, double lon1, double lat2, double lon2) //FES
{
double angle = atan2 ( deg2rad(lat2-lat1), (deg2rad(lon2-lon1) * cos(deg2rad(lat1))));

    angle = rad2deg(angle) ;
    angle = 90.0 - angle;
    if (angle < 0) angle = 360 + angle;
	return (angle);
}

double VTW(double VAW, double BAW, double SOG)
{
    double VTW_value = sqrt(pow((VAW * sin(BAW)),2) + pow((VAW * cos(BAW)- SOG),2));
    return VTW_value;
}

double BTW(double VAW, double BAW, double SOG)
{
    double BTW_value = atan((VAW * sin(BAW))/(VAW * cos(BAW)-SOG));
    if (BAW < PI){
        if (BTW_value < 0){
            BTW_value = PI + BTW_value;
        }
    }
    if (BAW > PI){
        if (BTW_value > 0){
            BTW_value = PI + BTW_value;
        }
        if (BTW_value < 0){
            BTW_value = PI * 2 + BTW_value;
        }
    }
    return BTW_value;
}

double VAW(double VTW, double BTW, double SOG)
{
    double VAW_value = sqrt(pow((VTW * sin(BTW)),2) + pow((VTW * cos(BTW)+ SOG), 2));
    return VAW_value;
}

double BAW(double VTW, double BTW,double SOG)
{
    double BAW_value = atan((VTW * sin(BTW))/(VTW * cos(BTW)+ SOG));
    if (BTW < PI){
        if (BAW_value < 0){
            BAW_value = PI + BAW_value;
        }
    }
    if (BTW > 3.1416){
        if (BAW_value > 0){
            BAW_value = PI + BAW_value;
        }
        if (BAW_value < 0){
            BAW_value = PI * 2 + BAW_value;
        }
    }
    return BAW_value;
}
