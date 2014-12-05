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

 
#include "wx/wx.h"
#include <wx/glcanvas.h>
#include <wx/notebook.h> 
#include <wx/textfile.h>
#include <wx/tokenzr.h>
#include <wx/wfstream.h> 
#include <wx/txtstrm.h> 
#include "nmea0183/nmea0183.h"
#include <wx/mstream.h>         // for icon
#include "ocpn_plugin.h"
#include <math.h>

#include "tackandlay_pi.h"
using namespace std;

#ifdef __WXMSW__
    #include "GL/glu.h"
#endif


enum {										// process ID's
        ID_OK,
        ID_OVERLAYDISPLAYOPTION,
        ID_DISPLAYTYPE
};

//******************************************************************
//**************** Graphical Entities **********************
double tnl_overlay_transparency;
int  tnl_polar_calibration[180][6];		// polar diagram represents 180 derees and 6,8,10,12,16,20 knots of wind
bool   tnl_bpos_set;
double tnl_ownship_lat, tnl_ownship_lon;
double cur_lat, cur_lon;
double tnl_hdm, tnl_hdt;
double SOG, COG;
double tnl_mark_brng, tnl_mark_rng;
double hdt_last_message, sog_last_message;
double tack_angle, lay_angle;
double lgth_line;

struct wind{
    double TWA;
    double RWA;
    double TWS;
    double RWS;
} Wind;

bool  tnl_shown_dc_message;

wxPoint boat_center, mark_center;
double temp_mark_lat = 0.0,temp_mark_lon = 0.0;

PlugIn_Route *m_pRoute = NULL;
PlugIn_Waypoint *m_pMark = NULL;

wxFrame *frame;

//wxGLContext     *m_context;

//wxTextCtrl        *plogtc;
//wxDialog          *plogcontainer;

struct pol
	{
        double boat_speed[32];       // j_wdir
	}wind_to_boat[10];             // i_wspd 


extern "C" DECL_EXP opencpn_plugin* create_pi(void *ppimgr)
{
    return new tackandlay_pi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p)
{
    delete p;
}

//---------------------------------------------------------------------------------------------------------
//    TnL PlugIn Implementation
//---------------------------------------------------------------------------------------------------------

tackandlay_pi::tackandlay_pi(void *ppimgr)
      :opencpn_plugin_111(ppimgr)
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
/*
//********************************************************************************************************
//   Logbook window
    m_plogwin = new wxLogWindow(NULL, _T("TnL Event Log"));
    plogcontainer = new wxDialog(NULL, -1, _T("TnL Log"), wxPoint(0,0), wxSize(600,400),
				    wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER );

    plogtc = new wxTextCtrl(plogcontainer, -1, _T(""), wxPoint(0,0), wxSize(600,400), wxTE_MULTILINE );

    wxLogMessage(_T("TnL log opened"));

    ::wxDisplaySize(&m_display_width, &m_display_height);
*/
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
//        RemovePlugInTool( m_tool_id );
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

        m_pOptionsDialog->load_POL_file();
        m_pOptionsDialog->Create(m_parent_window, this);


		m_pOptionsDialog->Show();
        m_pOptionsDialog->Refresh();
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
        if (m_pRoute){
           if(!m_pRoute->pWaypointList->empty())
           {
                   m_pRoute->pWaypointList->DeleteContents(true);
                   delete m_pRoute;
           }
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

//***************************
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

// Boat Position from Main program
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

	  SOG = pfix.Sog;     
      if(wxIsNaN(SOG)) SOG = 0.;
      if(SOG != sog_last_message)
		  sog_last_message = SOG;


      tnl_bpos_set = true;

}
// Wind data from NMEA stream
void tackandlay_pi::SetNMEASentence( wxString &sentence )
{
    m_NMEA0183 << sentence;
    if( m_NMEA0183.Parse()  == true)
    {
        if( m_NMEA0183.LastSentenceIDParsed == _T("MWV") )
        {               
            if (m_NMEA0183.Mwv.WindSpeedUnits == _T("K"))
            {
                Wind.RWS = Wind.RWS / 1.852;
            }
            else if (m_NMEA0183.Mwv.WindSpeedUnits == _T("M"))
            {
                Wind.RWS = Wind.RWS / 1.1515;
            }

            if(m_NMEA0183.Mwv.Reference == _T("R"))
            {
                Wind.RWA = m_NMEA0183.Mwv.WindAngle;
                Wind.RWS = m_NMEA0183.Mwv.WindSpeed;
                Wind.TWA = rad2deg(BTW(Wind.RWS, deg2rad(Wind.RWA), SOG)) ;
                Wind.TWS = VTW(Wind.RWS, deg2rad(Wind.RWA), SOG);
            }
            else
            {
                Wind.TWA = m_NMEA0183.Mwv.WindAngle;
                Wind.TWS = m_NMEA0183.Mwv.WindSpeed;
                Wind.RWA = rad2deg(BAW(Wind.TWS, deg2rad(Wind.TWA), SOG));
                Wind.RWS = VAW(Wind.TWS, deg2rad(Wind.TWA), SOG);
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
       if (Wind.TWA != 0 || Wind.TWS != 0)
       {

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

		    tack_angle = Max_VMG_TWA(Wind.TWS);
		    lgth_line = 200;
		    lay_angle = tack_angle;

            if(abs(Wind.TWA - COG) < 80)
            {
		        glColor4ub(0, 255, 0, 255);	// red, green, blue,  alpha
		        Draw_Line(Wind.TWA + tack_angle, lgth_line);  // angle, lgth_line
		        Draw_Line(Wind.TWA - tack_angle, lgth_line);  // angle, lgth_line
            }

            glPopMatrix();
            glPushMatrix();
		    if(temp_mark_lat > 0.0)
                {
                    GetCanvasPixLL(vp, &mark_center, temp_mark_lat, temp_mark_lon);
                    Draw_Wind_Barb(mark_center, Wind.TWA, Wind.TWS);
		            glTranslated( mark_center.x, mark_center.y, 0);
                    glRotatef(- 90, 0, 0, 1);
                    glColor4ub(255, 0, 0, 255);	// red, blue, green, alpha
                    Draw_Line(Wind.TWA + 180 + lay_angle, lgth_line);
                    Draw_Line(Wind.TWA + 180 - lay_angle, lgth_line);
                }

          glPopMatrix();
          glPopAttrib();
          Wind.TWS = 0;
          Wind.TWA = 0;
       }
	  return true;
}

double tackandlay_pi::Calc_VMG( double TWA,  double SOG)
{
    double speed = abs( SOG * cos(deg2rad(TWA)));
    return speed;
}

double tackandlay_pi::Calc_VMC(double COG, double SOG, double BTM)
{
    double speed = SOG * cos(deg2rad(COG-BTM));
    return speed;
}

double tackandlay_pi::Polar_SOG (double TWS, double TWA)
{
    double SOG = 0, SOG2 = 0;
    int j_wdir = int((TWA - 25)/5);
    int i_wspd = int(TWS/5);
    for (i_wspd; i_wspd = 0; i_wspd--)
    {
        SOG = wind_to_boat[i_wspd].boat_speed[j_wdir];
        if (SOG > 0) i_wspd = 0;
    }

    SOG2 = wind_to_boat[i_wspd + 1].boat_speed[j_wdir + 1]; // next data point value
    if (SOG2 > 0)
    {
        SOG += (SOG2-SOG) *( (TWA -(j_wdir*5 + 25))/5); // pro-rate delta
    }
    return SOG;
}

double tackandlay_pi::Max_VMG_TWA(double TWS)
{
    double speed, max_speed = 0;
    double TWA;
    int j_wdir;
    int i_wspd = (int)(TWS/5);

    for (j_wdir = 0; j_wdir < 15; j_wdir++) // 25->90 deg TWA
    {
        speed = wind_to_boat[i_wspd].boat_speed[j_wdir];
        TWA = j_wdir *5 + 25;
        speed = Calc_VMG(TWA, speed);             //VMG to wind
        if (speed > max_speed)
        {
            max_speed = speed;
        }
        else
        {
            j_wdir = 15;
        }
    }
    return TWA;
}

void tackandlay_pi::Draw_Line(int angle,int legnth)
{
	double x = cos(angle * PI / 180. ) * legnth;
	double y = sin(angle * PI / 180.) * legnth;

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
    display_speed = 10;
	windColour[0] = wxTheColourDatabase->Find(_T("YELLOW"));
	windColour[1] = wxTheColourDatabase->Find(_T("CORAL"));
	windColour[2] = wxTheColourDatabase->Find(_T("CYAN"));
	windColour[3] = wxTheColourDatabase->Find(_T("LIGHT BLUE"));
	windColour[4] = wxTheColourDatabase->Find(_T("CORNFLOWER BLUE"));
	windColour[5] = wxTheColourDatabase->Find(_T("GREEN"));
	windColour[6] = wxTheColourDatabase->Find(_T("BROWN"));
	windColour[7] = wxTheColourDatabase->Find(_T("RED"));
	windColour[8] = wxTheColourDatabase->Find(_T("VIOLET RED"));
	windColour[9] = wxTheColourDatabase->Find(_T("VIOLET"));
//	Init();
}

 TnLDisplayOptionsDialog::~TnLDisplayOptionsDialog()
{
    	m_panelPolar->Disconnect( wxEVT_PAINT, wxPaintEventHandler( TnLDisplayOptionsDialog::OnPaintPolar ), NULL, this );
}


bool TnLDisplayOptionsDialog::Create( wxWindow *parent, tackandlay_pi *ppi )
{
	pParent = parent;
    pPlugIn = ppi;

//	wxDialog *PreferencesDialog = new wxDialog( parent, wxID_ANY, _("BR24 Target Display Preferences"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE );
	if ( !wxDialog::Create ( parent, wxID_ANY, _T("TnL  Characteristics"), wxPoint(100,50), wxDefaultSize, wxDEFAULT_DIALOG_STYLE ) )
        return false;

    //this->SetSizeHints( wxDefaultSize, wxDefaultSize );

	wxBoxSizer* topSizer = new wxBoxSizer ( wxVERTICAL );
    SetSizer ( topSizer );
    
	bSizerNotebook = new wxBoxSizer( wxVERTICAL );	
	m_notebook = new wxNotebook( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );

    m_panelPolar = new wxPanel( m_notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );

	m_panelPolar->SetBackgroundColour( wxColour( 255, 255, 255 ) );
	m_panelPolar->SetMinSize( wxSize( 400,600 ) );
    m_notebook->AddPage( m_panelPolar, _("Polar Diagram"), true );

    bSizerNotebook->Add( m_notebook, 1, wxEXPAND | wxALL, 5 );
    topSizer->Add(bSizerNotebook);
   
// Accept/Reject buttoen	
    wxStdDialogButtonSizer* DialogButtonSizer = wxDialog::CreateStdDialogButtonSizer(wxOK|wxCANCEL);
    topSizer->Add(DialogButtonSizer, 0, wxALIGN_RIGHT|wxALL, 5);

	this->Layout();

    DimeWindow(this);
    Fit();
    SetMinSize(GetBestSize());

    m_panelPolar->Connect( wxEVT_PAINT, wxPaintEventHandler( TnLDisplayOptionsDialog::OnPaintPolar ), NULL, this );

    return true;
}

void TnLDisplayOptionsDialog::createDiagram(wxDC& dc)
{
	max_dimension = m_panelPolar->GetSize();
	center.x = max_dimension.x / 2 - 190;
	center.y = max_dimension.y /2;
	image_pixel_height[0] = max_dimension.y /2 -40 ;           // current height for denumerator
	pixels_knot_ratio = image_pixel_height[0] / display_speed;
	for(int i = 0; i < display_speed; i++)
		image_pixel_height[i] = wxRound(pixels_knot_ratio*(i+1));
}

void TnLDisplayOptionsDialog::Render_Polar()
{
//********** Draw Rings and speeds***********************
	int pos_xknt, pos_yknt,neg_yknt;

	for(int i = display_speed-1; i >= 0; i--)
	{
		pos_xknt = wxRound(cos((0-90)*(PI/180.0))*image_pixel_height[i]+center.x);
		pos_yknt = wxRound(sin((0-90)*(PI/180.0))*image_pixel_height[i]+center.y);
		neg_yknt = wxRound(sin((0+90)*(PI/180.0))*image_pixel_height[i]+center.y);
 
        dc->DrawArc(center.x,center.y+image_pixel_height[i],center.x,center.y-image_pixel_height[i],center.x,center.y);
              
            if(display_speed < 20){
			    dc->DrawText(wxString::Format(_T("%i"),i+1),wxPoint(pos_xknt,pos_yknt-10));
                dc->DrawText(wxString::Format(_T("%i"),i+1),wxPoint(pos_xknt,neg_yknt-10));
            }
            else{
				if((display_speed == 20 && ((i+1) % 2) == 0) || (display_speed == 25 && ((i+1) % 3) == 0)){
				    dc->DrawText(wxString::Format(_T("%i"),i+1),wxPoint(pos_xknt,pos_yknt-10));
                    dc->DrawText(wxString::Format(_T("%i"),i+1),wxPoint(pos_xknt,neg_yknt-10));
                }
			}
	}
//********** Draw Spokes *********************************
	int angle = 20;
	for(; angle <= 180; angle += 10)
	{
		int xt = wxRound(cos((angle-90)*(PI/180.0))*(image_pixel_height[(int)display_speed-1]+20)+center.x);
		int yt = wxRound(sin((angle-90)*(PI/180.0))*(image_pixel_height[(int)display_speed-1]+20)+center.y);

		dc->DrawLine(wxPoint(center.x,center.y),wxPoint(xt,yt));
		dc->DrawText(wxString::Format(_T("%i\xB0"),angle), wxPoint(xt,yt));
	}

	createSpeedBullets();
}

void TnLDisplayOptionsDialog::createSpeedBullets()
{
    int selected_i_wspd =  0;      // initially 0
	
	int end_index;
	int xt, yt, bullet_point_count;
	wxPoint ptBullets[180];             // max of 180 degrees

	if(selected_i_wspd != 0) {
        selected_i_wspd -= 1;
        end_index = selected_i_wspd + 1;
    }
	else{
        selected_i_wspd = 0;                  // all speeds
        end_index = 10;
    }

	double speed_in_pixels;
	wxColour Colour,brush;
	wxPen init_pen = dc->GetPen();			// get actual Pen for restoring later

    for(int i_wspd = selected_i_wspd; i_wspd < end_index; i_wspd++)									// go thru all winddirection-structures depending on choiocebox degrees
	{
		bullet_point_count = 0;
		Colour = windColour[i_wspd];
        brush = windColour[i_wspd];

		for(int j_wdir = 0; j_wdir < 33; j_wdir++)          // 0->31
		{
            double cell_value = wind_to_boat[i_wspd].boat_speed[j_wdir];
			if( cell_value > 0) 
                {
			        speed_in_pixels = cell_value * pixels_knot_ratio;

			        xt = wxRound(cos(((j_wdir*5+25)- 90)*PI/180)*speed_in_pixels + center.x);		// calculate the point for the bullet
			        yt = wxRound(sin(((j_wdir*5+25)- 90)*PI/180)*speed_in_pixels + center.y);

                    wxPoint pt(xt,yt);
				    ptBullets[bullet_point_count++] = pt;      // Add to display array
            }
		}

//************ Wind_sped Column, plot curve and bullets ********************

		if(bullet_point_count > 2)					//Draw curves, needs min. 3 points
		{
			dc->SetPen(wxPen(Colour,3));
			dc->DrawSpline(bullet_point_count,ptBullets);
		}
  	
		for(int i = 0; i < bullet_point_count; i++)     // draw the bullet
		{
			if(ptBullets[i].x != 0 && ptBullets[i].y != 0)
			{            
		    dc->SetBrush( brush );
			dc->SetPen(wxPen(wxColour(0,0,0),2));
			dc->DrawCircle(ptBullets[i],5);	
			ptBullets[i].x = ptBullets[i].y = 0;
			}
		} 
	}

	dc->SetPen(init_pen);
}

void TnLDisplayOptionsDialog::OnPaintPolar( wxPaintEvent& event )
{
	wxPaintDC dc(m_panelPolar);
	this->dc = &dc;
	this->createDiagram(dc);
    this->Render_Polar();
}

void TnLDisplayOptionsDialog::OnSizePolar( wxSizeEvent& event )
{
	m_panelPolar->Fit();
	m_panelPolar->Refresh();
}

void TnLDisplayOptionsDialog::OnSizePolarDlg( wxSizeEvent& event )
{
	Layout();
	Refresh();
//	m_panelPolar->Fit();
//	m_panelPolar->Refresh();
}

void TnLDisplayOptionsDialog::OnClose ( wxCloseEvent& event )
{
//     pPlugIn->SaveConfig();
}

void TnLDisplayOptionsDialog::OnIdOKClick ( wxCommandEvent& event )
{
//+     pPlugIn->SaveConfig();
}

//******************************************************************
//************** Polar Table input ********************************

void TnLDisplayOptionsDialog::load_POL_file(void)
{
    wxString filetypext = _("*.pol");
	wxFileDialog fdlg(this,_("Select a POL File (.pol)"),_T(""), _T(""), filetypext, wxFD_OPEN|wxFD_FILE_MUST_EXIST );
	if(fdlg.ShowModal() == wxID_CANCEL) return;

	wxFileInputStream stream( fdlg.GetPath() );							
	wxTextInputStream in(stream);	
	wxString wdirstr,wsp;

	bool test_first = true;
	wxString file_type = _("");
    int file_line = -1;

    wxStringTokenizer tkz;              // Selected Parsing character

	while(!stream.Eof())
	{
		wxString str = in.ReadLine();

//************ File type test phase (one run) ************************
		if(test_first)
		{
            if(str.GetChar(0) == '5') {           // our POL (or csv)
				file_type = _("POL/CSV");
                wxStringTokenizer tk(str,_T(" "),wxTOKEN_RET_EMPTY);
				tkz = tk;
            }
			else if(str.Contains(_T("TWA/TWS"))) {    // TAB specials
				file_type = _("QTV");
                wxStringTokenizer tk(str,_T("\t"),wxTOKEN_RET_EMPTY);	
				tkz = tk;
            }
			else if (str.Contains(_T("TWA"))) {
				file_type = _("MAXSEA");
                wxStringTokenizer tk(str,_T("\t"),wxTOKEN_RET_EMPTY);	
				tkz = tk;
            }
			else
			{
				wxMessageBox(_T("File doesn't match known types."));
				return;
			}

			test_first = false;

			if( file_type != _("POL/CSV"))     // Tab special files have header lines
				continue;
		}

// *************Data Phase *************************************
/*        
		wxString field = tkz.GetNextToken();
		if (file_type == _T("TWA/TWS") && field == _T("0") ){
            continue;
        }
*/		

        tkz.SetString(str);
        int j_wdir = 0;
        int i_wspd = file_line;

//****************** POL file processing ****************************
		if (file_type == _("POL/CSV"))       // TODO stregnthen file parsing                               // standard POL and csv
		{
			wxString field_value;
            double number;
            tkz.GetNextToken();     // First field is Wind speed

			while(tkz.HasMoreTokens())
			{                         
				field_value = tkz.GetNextToken(); // Find wind direction (20 & up) 
                field_value.ToDouble(&number);

				if(number < 20.)         // test for Boat Speed
                {
                    wind_to_boat[i_wspd].boat_speed[j_wdir] = number;
                    tkz.GetNextToken();     // go past next angle
                    j_wdir++;
                }

                
            }
		}
//************************************************************
        else if(file_type ==  _("QTV") || file_type == _("MAXSEA"))                              // special format
		{
			int i_ws_col = 0, token_count = 0; 
			wxString s;
			while(tkz.HasMoreTokens())
			{
				token_count++;
				if(token_count > 11) break;
				s = tkz.GetNextToken();
//				if(s == _T("0") && (file_type == 1 || file_type == 2)) continue;
//				dlg->m_gridEdit->SetCellValue(file_line,i_ws_col,s);
//				load_POL_datum_str(s,file_line,i_ws_col++);
			}
		}
        file_line++;
	}
}

/********************************************************************************************************/
//   Distance measurement for simple sphere
/********************************************************************************************************/
double twoPI = 2 * PI;

static double deg2rad(double deg)
{
    return ((90 - deg * PI / 180.0));
}

static double rad2deg(double rad)
{
    return (int(rad * 180.0 / PI + 90 + 360) % 360);
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
    return BAW_value;
}


