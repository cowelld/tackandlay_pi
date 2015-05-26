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

#include <wx/fileconf.h>
#include "wx/wx.h"
#include <wx/glcanvas.h>
#include <wx/notebook.h> 
#include <wx/textfile.h>
#include <wx/tokenzr.h>
#include <wx/wfstream.h> 
#include <wx/txtstrm.h> 
#include <wx/timer.h>
#include "nmea0183/nmea0183.h"
#include <wx/mstream.h>         // for icon
#include "ocpn_plugin.h"
#include <math.h>
#include <typeinfo>
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
bool   POS_OK, MWV_OK, MWD_OK;
bool   POS_warn, MWV_warn, MWD_warn;
double boat_lat, boat_lon;
double cur_lat, cur_lon;
double tnl_mark_brng, tnl_mark_rng;
double tack_angle, lay_angle;
double lgth_line;

struct wind{
    double TWA;
    double RWA;
    double TWS;
    double RWS;
    double TWD;
} Wind;

struct boat{
    double SOG;
    double COG;
    double STW;
    double HDG;
    double HDGM;
} Boat;

bool  tnl_shown_dc_message;

wxPoint boat_center, mark_center;
double mark_lat = 0.0,mark_lon = 0.0;

PlugIn_Route *m_pRoute = NULL;
PlugIn_Waypoint *m_pMark = NULL;

wxFrame *frame;

//wxGLContext     *m_context;

//wxTextCtrl        *plogtc;
//wxDialog          *plogcontainer;


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
      :opencpn_plugin_111(ppimgr), wxTimer(this)
{
    wxMemoryInputStream _tnl("\211PNG\r\n\032\n\000\000\000\rIHDR\000\000\000 \000\000\000 \b\006\000\000\000szz\364\000\000\000\004sBIT\b\b\b\b|\bd\210\000\000\000\011pHYs\000\000\r\327\000\000\r\327\001B(\233x\000\000\000\031tEXtSoftware\000www.inkscape.org\233\356<\032\000\000\000\304IDATX\205\355\3261\016AA\020\306\361\377\274\347\002^\247{W\320\211\003(\034@\245Qm'q\004\275J\245\323+\204F\341\006\016!\032y\255V\"\214\013X\317n\304*f\352\315|\277d2\331\021U%eeI\323\r`\000\240Q\373\302I\013\030\002\035\240\004\n\340\014\234Pv\334\330\260\324k,@\274k8\225\214\212\0310\006\3627=.\b#\026\272\215\001\370GP1\a&5\341\000M\2245Nz1\200\327#\030HNA\027\345\370q'\241\017\354\277\003X\351\035'\355\240N\017\312\320p\370\203-0\200\001\014`\000\003\370\357\001%\364{=\304\000\374\367\300\217*\371\b\014`\000\003$\a<\001z\344&\362\346\013\372\337\000\000\000\000IEND\256B`\202", 327);
    m_pdeficon = new wxBitmap(_tnl);
}
tackandlay_pi::~tackandlay_pi(void)
{
}

int tackandlay_pi::Init(void)
{
    m_interval = 10000;   // ten seconds
    Start( m_interval, wxTIMER_CONTINUOUS );

    m_pconfig = GetOCPNConfigObject();
    LoadConfig();
    Master_pol_loaded = false;
    m_pOptionsDialog = NULL;

    m_parent_window = GetOCPNCanvasWindow();
    
m_tool_id  = InsertPlugInTool(_T(""), m_pdeficon, m_pdeficon, wxITEM_NORMAL,
            _T("TnL Line"), _T(""), NULL,
            TACKANDLAY_TOOL_POSITION, 0, this);
TnLactive = false;

//            CacheSetToolbarToolBitmaps( 1, 1);

  // Context menue for making marks    
    m_pmenu = new wxMenu(); 
    // this is a dummy menu required by Windows as parent to item created
    wxMenuItem *pmi = new wxMenuItem( m_pmenu, -1, _T("Set Race Mark "));
    int miid = AddCanvasContextMenuItem(pmi, this);
    SetCanvasContextMenuItemViz(miid, true);

//****************************************************************************************
 
    return (
            WANTS_TOOLBAR_CALLBACK     |
            INSTALLS_TOOLBAR_TOOL      |
            WANTS_CONFIG               |
            WANTS_DYNAMIC_OPENGL_OVERLAY_CALLBACK |
            WANTS_OPENGL_OVERLAY_CALLBACK |
            WANTS_OVERLAY_CALLBACK     |
            WANTS_CURSOR_LATLON        |
            INSTALLS_CONTEXTMENU_ITEMS |           
            WANTS_NMEA_SENTENCES       |
            WANTS_NMEA_EVENTS          |
            WANTS_PREFERENCES
            );
}

bool tackandlay_pi::DeInit(void)
{
        //      SaveConfig();
    if ( IsRunning() ) // Timer started?
      {
            Stop(); // Stop timer
      }
    if (m_pRoute){
        m_pRoute->pWaypointList->DeleteContents(true);
        DeletePlugInRoute( m_pRoute->m_GUID );
    }
    SaveConfig();
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
		m_pOptionsDialog = new TnLDisplayOptionsDialog();
        m_pOptionsDialog->Create(m_parent_window, this);

        wxString file_name = m_pOptionsDialog->Get_POL_File_name();
        load_POL_file(file_name);
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
        TnLactive = !TnLactive;
        if (m_pRoute){
           if(!m_pRoute->pWaypointList->IsEmpty())
           {
                m_pRoute->pWaypointList->DeleteContents(true);
           }
           delete m_pRoute;
        }

        wxString File_message = _("File currently loading is ") + m_filename;
        wxMessageBox(File_message);

        if (!m_filename.IsEmpty() && TnLactive){
            load_POL_file(m_filename);
            if(m_pOptionsDialog != NULL){
                m_pOptionsDialog->Show();
                m_pOptionsDialog->Refresh();
            }
        }
    }
}

void tackandlay_pi::Notify()        // ten second timer
{
    if (POS_warn){
        POS_OK = false;         // 20 second fail
        POS_warn = false;
    }
    else
        POS_warn = true;        // 10 second warning

    if (MWV_warn){
        MWV_OK = false;
        MWV_warn = false;
    }
    else
        MWV_warn = true;

    if (MWD_warn){
        MWD_OK = false;
        MWD_warn = false;
    }
    else
        MWD_warn = true;
}
//*****************************************************************************

bool tackandlay_pi::LoadConfig(void)
{

      wxFileConfig *pConf = m_pconfig;

      if(pConf)
      {
           pConf->SetPath(wxT("/Plugins/TackandLay"));
            pConf->Read ( _T( "TnLTransparency" ),  &tnl_overlay_transparency, .50 );
            pConf->Read (_T("TnLPOLFILE"), &m_filename, _(""));
            return true;
      }
      else
            return false;

}

bool tackandlay_pi::SaveConfig(void)
{
      wxFileConfig *pConf = m_pconfig;

      if(pConf)
      {
            pConf->SetPath(wxT("/Plugins/TackandLay"));
            pConf->Write ( _T( "TnLTransparency" ),  tnl_overlay_transparency);
            pConf->Write (_T("TnLPOLFILE"), m_filename);
            return true;
      }
      else
            return false;
}

void tackandlay_pi::clear_Master_pol()
{
	for(int i = 0; i < 10; i++)
	{
        Master_pol[i].TWS = 0;

		for(int n = 0; n < 60; n++)
		{
			Master_pol[i].TWA[n] = 0;
			Master_pol[i].boat_speed[n] = 0;

		}
	}
}

void tackandlay_pi::load_POL_file(wxString file_name)
{  
    if (!file_name.IsEmpty())
    {
	    clear_Master_pol();
        
        wxTextFile   m_infile;
        m_infile.Open(file_name);

	    int file_type = -1;
        wxStringTokenizer tkz;              // Selected Parsing character
        wxString token_string;

        long TWA_count = m_infile.GetLineCount(); //  Row count for TWA incrementing 

//***************** Header and Wind Speeds *********************************
        wxString in_str =  m_infile.GetFirstLine();

        wxString temp;

        if(in_str.Find(_T("\t"))!= wxNOT_FOUND){
			file_type = 1;
            wxStringTokenizer tk(in_str,_T("\t"),wxTOKEN_RET_EMPTY);	
			tkz = tk;
        }

        if(in_str.Find(_T(";"))!= wxNOT_FOUND){
            file_type = 1;
            wxStringTokenizer tk(in_str,_T(";"),wxTOKEN_RET_EMPTY);	
			tkz = tk;
        }

	    if(in_str.Find (_(","))!= wxNOT_FOUND)
        {
            file_type = 1;
            wxStringTokenizer tk(in_str,_T(","),wxTOKEN_RET_EMPTY);
		    tkz = tk;
        }

        if(in_str.Find (_("    "))!= wxNOT_FOUND)   // Linux tab substitution
        {
            file_type = 1;
            wxStringTokenizer tk(in_str,_T("     "),wxTOKEN_RET_EMPTY);
		    tkz = tk;
        }

        if((file_type == -1) && (in_str.Find (_(" ")) != wxNOT_FOUND))
        {
            file_type = 1;
            wxStringTokenizer tk(in_str,_T(" "),wxTOKEN_RET_EMPTY);
		    tkz = tk;
        }

        if (file_type == -1)
	    {
		    wxMessageBox(_T("Cannot load this file"));
		    return;
	    }

//************************ Wind Speeds *************************************** 
        in_str =  m_infile.GetFirstLine();
        long wind_speed_count = tkz.CountTokens(); // TWS count for incrementing
        long wind_speed;
        long wind_angle, TWA_line = 1;
        double boat_speed;
        int wspd_zero = 0;

        token_string = tkz.GetNextToken(); // burn TWA\TWS etc
    
        for (i_wspd= 0; i_wspd < 15; i_wspd++)
	    {
		    token_string = tkz.GetNextToken();
            if(token_string == _("0")){
                token_string = tkz.GetNextToken();
                wspd_zero = 1;
            }
            token_string.ToLong(&wind_speed);
            Master_pol[i_wspd].TWS = wind_speed;
            if(wind_speed_count > 20) token_string = tkz.GetNextToken();
        }

//************************* Wind Angle, Boat_speed data *********************


        in_str =  m_infile.GetNextLine();       // check for zero line
        tkz.SetString(in_str);
        token_string = tkz.GetNextToken();

        if(token_string != _("0"))
            m_infile.GetFirstLine();
    
        for(int i_TWA = 0; i_TWA < 60; i_TWA++)
        {
            if (TWA_line < TWA_count)
            {
                in_str =  m_infile.GetNextLine();
                tkz.SetString(in_str);

                if(file_type == 1)              // Normal format
		        {
                    token_string = tkz.GetNextToken();
                    token_string.ToLong(&wind_angle);
                    Master_pol[0].TWA[i_TWA] = wind_angle;
                    if(wspd_zero > 0) token_string = tkz.GetNextToken(); // account for 0 wind speed collumn

                    for (i_wspd= 1; i_wspd < 15; i_wspd++)
	                {
		                token_string = tkz.GetNextToken();
                        token_string.ToDouble(&boat_speed);
                        Master_pol[i_wspd].boat_speed[i_TWA] = boat_speed;
            
                        if(wind_speed_count > 20) token_string = tkz.GetNextToken();
			        }
                } 
                TWA_line = TWA_line + 1;

                if(TWA_count > 33 && TWA_line < TWA_count){
                    TWA_line = TWA_line + 1;
                    in_str =  m_infile.GetNextLine();
                }

                if(TWA_count > 48 && TWA_line < TWA_count){
                    TWA_line = TWA_line + 1;
                    in_str =  m_infile.GetNextLine();
                }
            }       
        }
        if (file_type != -1 && wind_speed_count > 0)
            Master_pol_loaded = true;
    }
}

//***************** Cursor position data **********
 
void tackandlay_pi::SetCursorLatLon(double lat, double lon)
{
cur_lat = lat;
cur_lon = lon;
}

//******************* Set MARK Position ***************
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
    mark_lat = cur_lat;
    mark_lon = cur_lon;
    m_pMark->m_CreateTime = wxDateTime::Now();
    m_pRoute->pWaypointList->Append(m_pMark);

//    if(!UpdatePlugInRoute(m_pRoute))
    {
        AddPlugInRoute(m_pRoute,false);
    }
 
}

// Boat Position from Main program
void tackandlay_pi::SetPositionFix(PlugIn_Position_Fix &pfix)
{
      boat_lat = pfix.Lat;
      boat_lon = pfix.Lon;
      Boat.COG = pfix.Cog;
      Boat.SOG = pfix.Sog;
      POS_OK = true;
      POS_warn = false;

}

void tackandlay_pi::SetPositionFixEx(PlugIn_Position_Fix_Ex &pfix)
{
      boat_lat = pfix.Lat;
      boat_lon = pfix.Lon;

      Boat.COG = pfix.Cog;

      Boat.HDG = pfix.Hdt;
      if(Boat.HDG == 0)
        Boat.HDG = pfix.Cog;

	  Boat.SOG = pfix.Sog;     

      POS_OK = true;
      POS_warn = false;
}
// Wind data from NMEA stream
void tackandlay_pi::SetNMEASentence( wxString &sentence )
{
    double wind_speed;
    m_NMEA0183 << sentence;
    if( m_NMEA0183.Parse()  == true)
    {
        if( m_NMEA0183.LastSentenceIDParsed == _T("MWV") )
        {
            MWV_OK = true;
            MWV_warn = false;
            wind_speed = m_NMEA0183.Mwv.WindSpeed;
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
                Wind.RWA = m_NMEA0183.Mwv.WindAngle;
                Wind.RWS = wind_speed;
                Wind.TWA = fTWA(Wind.RWS, Wind.RWA, Boat.SOG) ;
                Wind.TWS = fTWS(Wind.RWS, Wind.RWA, Boat.SOG);
            }
            else
            {
                Wind.TWA = m_NMEA0183.Mwv.WindAngle;
                Wind.TWS = wind_speed;
                Wind.RWA = fRWA(Wind.TWS, Wind.TWA, Boat.SOG);
                Wind.RWS = fRWS(Wind.TWS, Wind.TWA, Boat.SOG);
            }
        }
/*        if( m_NMEA0183.LastSentenceIDParsed == _T("MWD") )
        {
            MWD_OK = true;
            MWD_warn = false;
            Wind.TWS = m_NMEA0183.Mwd.WindSpeedKnots;
            Wind.TWD = m_NMEA0183.Mwd.WindAngleTrue;
        }
 */           
    }
}

bool tackandlay_pi::RenderOverlay(wxDC &dc, PlugIn_ViewPort *vp)
{
    if(0 == tnl_shown_dc_message) {
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

	if (TnLactive && Master_pol_loaded)
    {
        if(POS_OK && MWV_OK)
	    {
            glPushAttrib(GL_COLOR_BUFFER_BIT | GL_LINE_BIT | GL_HINT_BIT);      //Save state
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glPushMatrix();
            if (Wind.TWS > 2)
            {
                GetCanvasPixLL(vp, &pp, boat_lat, boat_lon);
		        boat_center = pp;

                Wind.TWD = (Wind.TWA + Boat.COG);
                if (Wind.TWD > 360) Wind.TWD = Wind.TWD - 360;
                Draw_Wind_Barb(boat_center, Wind.TWD, Wind.TWS);

		        if(Wind.TWA < 90 || Wind.TWA > 270){ // Wind forward
                    tack_angle =  TWA_for_Max_Tack_VMG(Wind.TWS);
                    glColor4ub(0, 255, 0, 255);
		        }

                if(Wind.TWA > 90 && Wind.TWA < 270){
                    tack_angle =  TWA_for_Max_Run_VMG(Wind.TWS);
                    glColor4ub(0, 255, 255, 255);
                }
		            lgth_line = 200;
//                    GLubyte red(0), green(255), blue(0), alpha(255);
                    glTranslated( boat_center.x, boat_center.y, 0);
//                    glColor4ub(0, 255, 0, 255);                 	// red, green, blue,  alpha
		            Draw_Line(Wind.TWD + tack_angle, lgth_line);  // angle, lgth_line
		            Draw_Line(Wind.TWD - tack_angle, lgth_line);  // angle, lgth_line
               
		        if(mark_lat > 0.0)
                {
                    glPopMatrix();
                    glPushMatrix();
		            lay_angle = tack_angle;
                    GetCanvasPixLL(vp, &mark_center, mark_lat, mark_lon);
                    Draw_Wind_Barb(mark_center, Wind.TWD, Wind.TWS);
                        
                    glTranslated( mark_center.x, mark_center.y, 0);
                    glColor4ub(255, 0, 0, 255);
                    Draw_Line(Wind.TWD + 180 + lay_angle, lgth_line);
                    Draw_Line(Wind.TWD + 180 - lay_angle, lgth_line);
                }
            }             
            glPopMatrix();
            glPushMatrix();

            dial_center.x = 150;
            dial_center.y = 700;

            glColor4ub(0, 0, 0, 255);
            DrawCircle(dial_center,100,30);
            glColor4ub(255, 0, 0, 255);
            Draw_Boat(dial_center);
            Draw_Wind_Ptr(dial_center, 100, Wind.TWA, Wind.TWS);

            glTranslated( dial_center.x, dial_center.y, 0);
            glColor4ub(0, 255, 0, 255);                 	// red, green, blue,  alpha
            Draw_Line(tack_angle, 100);
            Draw_Line(-tack_angle, 100);

            glPopMatrix();
            glPopAttrib();
        }
    }
	return true;
}

double tackandlay_pi::Calc_VMG_W( double TWA,  double SOG)
{
    double speed =  SOG * cos(TWA * PI / 180);
    return speed;
}

double tackandlay_pi::Calc_VMG_C(double COG, double SOG, double BTM)
{
    double speed = SOG * cos((COG-BTM)* PI/180);
    return speed;
}

double tackandlay_pi::interpret_Polar_boat_speed (double TWS, double TWA)
{
    double first_boat_speed, second_boat_speed;
    int first_TWA, second_TWA, first_wdir, first_wspd;
   
    for (j_wdir = 0; j_wdir < 60 ; j_wdir++)
    {
        if ( Master_pol[0].TWA[j_wdir] < TWA){  // next lower TWA
            first_TWA = Master_pol[0].TWA[j_wdir];
            first_wdir = j_wdir;
        }
        else j_wdir = 60;
    }
    second_TWA = Master_pol[0].TWA[first_wdir + 1];

    for (i_wspd = 0; i_wspd < 15 ; i_wspd++) // get next lower TWS
    {
        if(Master_pol[i_wspd].TWS < TWS)
            first_wspd = i_wspd;

        else 
            i_wspd = 15;
    }

    first_boat_speed = Master_pol[first_wspd].boat_speed[first_wdir];
    second_boat_speed = Master_pol[first_wspd + 1].boat_speed[first_wdir + 1];

    double int_boat_speed = first_boat_speed + (second_boat_speed - first_boat_speed) *
        (TWA - first_TWA)/(second_TWA - first_TWA);

    return int_boat_speed;
}

double tackandlay_pi::TWA_for_Max_VMG_to_Mark(double TWS, double TWD, double BTM)
{
    double pol_speed, max_speed = 0;
    int pol_TWA, max_TWA;
    int i_wspd;

    for (int i = 0; i < 14; i++)            // Find current wind speed index
    {
        if (Master_pol[i].TWS < TWS)
            i_wspd = i + 1;
    }

    double Course_WA = TWD - BTM;                 // determine if upwind or downwind to mark

    if (Course_WA < 30){

    };
        
    if (Course_WA < 90) // Tacking - approach from 0           
    {
        for (int j_wdir = 0; j_wdir < 30 ; j_wdir++)  // 0-90 deg TWA
        
        {
            pol_speed = Master_pol[i_wspd].boat_speed[j_wdir];
            pol_TWA = Master_pol[i_wspd].TWA[j_wdir];

            double VMG = Calc_VMG_W(pol_TWA, pol_speed);             //VMG_C to wind
            if (VMG > max_speed)
            {
                max_speed = VMG;
                max_TWA = pol_TWA;
            }
        }
    }
    else if(Course_WA > 90)            // Running - approach from 180
    {
        for (int j_wdir = 59; j_wdir >= 30; j_wdir--) // 180 -> 90 deg TWA
        {
            pol_speed = Master_pol[i_wspd].boat_speed[j_wdir];
            pol_TWA = Master_pol[i_wspd].TWA[j_wdir];

            double VMG = Calc_VMG_W(pol_TWA, pol_speed);             //VMG_C to wind
            if (VMG > max_speed)
            {
                max_speed = VMG;
                max_TWA = pol_TWA;
            }
        }
    }
    return max_TWA;
}

double tackandlay_pi::TWA_for_Max_Tack_VMG(double TWS)
    {
        double pol_speed, max_speed = 0;
        int pol_TWA, max_TWA;
        int i_wspd;

        for (int i = 0; i < 14; i++)            // Find current wind speed index
        {
            if (Master_pol[i].TWS < TWS)
                i_wspd = i;
        }

        for (int j_wdir = 0; j_wdir < 30 ; j_wdir++)  // 0-90 deg TWA      
        {
            pol_speed = Master_pol[i_wspd].boat_speed[j_wdir];
            pol_TWA = Master_pol[0].TWA[j_wdir];

           if (pol_speed > 0){
               double VMG = Calc_VMG_W(pol_TWA, pol_speed);             //VMG_C to wind
                if (VMG > max_speed)
                {
                    max_speed = VMG;
                    max_TWA = pol_TWA;
                }
           }
        }
    return max_TWA;
}

double tackandlay_pi::TWA_for_Max_Run_VMG(double TWS)
    {
        double pol_speed, max_speed = 0;
        int pol_TWA, max_TWA;
        int i_wspd;

        for (int i = 0; i < 14; i++)            // Find current wind speed index
        {
            if (Master_pol[i].TWS < TWS)
                i_wspd = i;
        }

        for (j_wdir = 59; j_wdir >= 30; j_wdir--) // 180 -> 90 deg TWA
        {
            pol_speed = Master_pol[i_wspd].boat_speed[j_wdir];
            pol_TWA = Master_pol[0].TWA[j_wdir];

            if (pol_speed > 0) {
                double VMG = -Calc_VMG_W(pol_TWA, pol_speed);             //VMG_C to wind
                if (VMG > max_speed)
                {
                    max_speed = VMG;
                    max_TWA = pol_TWA;
                }
            }
        }
    return max_TWA;
}

void tackandlay_pi::Draw_Line(int angle,int legnth)
{
	double x = cos(deg2rad(angle) ) * legnth;
	double y = -sin(deg2rad(angle)) * legnth;

    glBegin(GL_LINES);
    glVertex2d(0.0, 0.0);
    glVertex2d(x,y);
    glEnd();

}

void tackandlay_pi::Draw_Wind_Barb(wxPoint pp, double TWD, double speed)
{
    glColor4ub(0, 0, 0, 255);	// red, green, blue,  alpha (byte values)
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

    rad_angle = deg2rad(TWD);
    
    shaft_x = cos(rad_angle) * 30;
	shaft_y = -sin(rad_angle) * 30;

    barb_0_x = pp.x + .6 * shaft_x;
    barb_0_y = (pp.y + .6 * shaft_y);
    barb_1_x = pp.x + .8 * shaft_x;
    barb_1_y = (pp.y + .8 * shaft_y);
    barb_2_x = pp.x + shaft_x;
    barb_2_y = (pp.y + shaft_y);

    
    barb_legnth_0_x = cos(rad_angle + PI/4) * barb_legnth[p];
    barb_legnth_0_y = -sin(rad_angle + PI/4) * barb_legnth[p];
    barb_legnth_1_x = cos(rad_angle + PI/4) * barb_legnth[p+1];
    barb_legnth_1_y = -sin(rad_angle + PI/4) * barb_legnth[p+1];
    barb_legnth_2_x = cos(rad_angle + PI/4) * barb_legnth[p+2];
    barb_legnth_2_y = -sin(rad_angle + PI/4) * barb_legnth[p+2];


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

void tackandlay_pi::Draw_Boat(wxPoint pp)
{
    int bow_y = -100;
    int beam_x = 50;
    int beam_y = 0;
    int quarter_x = 40;
    int quarter_y = 100;

    glBegin(GL_LINE_STRIP);       
        glVertex2d(pp.x, pp.y + bow_y);
        glVertex2d(pp.x + beam_x, pp.y + beam_y);
        glVertex2d(pp.x + quarter_x, pp.y + quarter_y);
        glVertex2d(pp.x - quarter_x, pp.y + quarter_y);
        glVertex2d(pp.x - beam_x, pp.y + beam_y);
        glVertex2d(pp.x, pp.y + bow_y);
    glEnd();

}

void tackandlay_pi::DrawCircle(wxPoint pp, float r, int num_segments)
{
    glBegin(GL_LINE_LOOP);
    for(int ii = 0; ii < num_segments; ii++)
    {
        float theta = 2.0f * 3.1415926f * float(ii) / float(num_segments);//get the current angle

        float x = r * cosf(theta);//calculate the x component
        float y = r * sinf(theta);//calculate the y component

        glVertex2f(x + pp.x, y + pp.y);//output vertex

    }
    glEnd();
}

void tackandlay_pi::Draw_Wind_Ptr(wxPoint pp, float r, double angle, double speed)
{

    double C1_alpha = deg2rad(angle+5);
    double C2_alpha = deg2rad(angle-5);
    double C3_alpha = deg2rad(angle);
    glColor4ub(0,255,0,255);
    glBegin(GL_TRIANGLES);    
    	glVertex2d(pp.x + (cos(C1_alpha)*(r+20)), pp.y - (sin(C1_alpha)*(r+20)));
        glVertex2d(pp.x + (cos(C2_alpha)*(r+20)), pp.y - (sin(C2_alpha)*(r+20)));
        glVertex2d(pp.x + (cos(C3_alpha)*r), pp.y - (sin(C3_alpha)*r));
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
    windColour[10] = wxTheColourDatabase->Find(_T("WHITE"));
    windColour[11] = wxTheColourDatabase->Find(_T("CORAL"));
	windColour[12] = wxTheColourDatabase->Find(_T("CYAN"));
	windColour[13] = wxTheColourDatabase->Find(_T("LIGHT BLUE"));
	windColour[14] = wxTheColourDatabase->Find(_T("SALMON"));
//	Init();
}

 TnLDisplayOptionsDialog::~TnLDisplayOptionsDialog()
{
    	m_panelPolar->Disconnect( wxEVT_PAINT, wxPaintEventHandler( TnLDisplayOptionsDialog::OnPaintPolar ), NULL, this );
}


bool TnLDisplayOptionsDialog::Create( wxWindow *parent, tackandlay_pi *ppi )
{
    pProgram = ppi;

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
}

void TnLDisplayOptionsDialog::Render_Polar()
{
//********** Draw Rings and speeds***********************
	int pos_xknt, pos_yknt,neg_yknt;

	for(int i = display_speed-1; i >= 0; i--)
	{
        image_pixel_height[i] = wxRound(pixels_knot_ratio*(i+1));
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
	int xt, yt, bullet_point_count;
	wxPoint ptBullets[600];             // max of 10 X 60

	wxColour Colour,brush;
	wxPen init_pen = dc->GetPen();			// get actual Pen for restoring later

    for(int i_wspd = 0; i_wspd < 15; i_wspd++)	 // go thru all wind speeds
	{
		bullet_point_count = 0;
		Colour = windColour[i_wspd];
        brush = windColour[i_wspd];

		for(int j_wdir = 0; j_wdir < 60; j_wdir++)          // min-> max (180 deg)
		{
            double boat_speed = pProgram->Master_pol[i_wspd].boat_speed[j_wdir];
			if( boat_speed > 0) 
                {
			        double speed_in_pixels = boat_speed * pixels_knot_ratio;
                    double wind_angle = pProgram->Master_pol[0].TWA[j_wdir];

			        xt = wxRound(cos((wind_angle- 90)*PI/180)*speed_in_pixels + center.x);		// calculate the point for the bullet
			        yt = wxRound(sin((wind_angle- 90)*PI/180)*speed_in_pixels + center.y);

                    wxPoint pt(xt,yt);
				    ptBullets[bullet_point_count++] = pt;      // Add to display array
            }
		}

//************ Wind_speed Column, plot curve and bullets ********************

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
			ptBullets[i].x = 0;
            ptBullets[i].y = 0;
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
     pProgram->SaveConfig();
}

void TnLDisplayOptionsDialog::OnIdOKClick ( wxCommandEvent& event )
{
     pProgram->SaveConfig();
}

//******************************************************************
//************** Polar Table input ********************************

wxString TnLDisplayOptionsDialog::Get_POL_File_name(void)
{
    wxString filetypext = _("*.pol");
	wxFileDialog fdlg(this,_("Select a POL File (.pol)"),_T(""), _T(""), filetypext, wxFD_OPEN|wxFD_FILE_MUST_EXIST );
	if(fdlg.ShowModal() == wxID_CANCEL) return _("");
    
    pProgram->m_filename = fdlg.GetPath();
    return pProgram->m_filename;
}



/********************************************************************************************************/
//   Distance measurement for simple sphere
/********************************************************************************************************/
double twoPI = 2 * PI;

static double deg2rad(double deg)
{
    return ((90 - deg) * PI / 180.0);
}

static double rad2deg(double rad)
{
    return (int(90 - (rad * 180.0 / PI) + 360) % 360);
}

static double local_distance (double lat1, double lon1, double lat2, double lon2) {
	// Spherical Law of Cosines
	double theta, dist; 

	theta = lon2 - lon1; 
	dist = sin(lat1 * PI/180) * sin(lat2 * PI/180) + cos(lat1 * PI/180) * cos(lat2* PI/180) * cos(theta* PI/180); 
	dist = acos(dist);		// radians
	dist = dist * 180 /PI; 
    dist = fabs(dist) * 60    ;    // nautical miles/degree
	return (dist); 
}

static double local_bearing (double lat1, double lon1, double lat2, double lon2) //FES
{
double angle = atan2( (lat2-lat1)*PI/180, ((lon2-lon1)* PI/180 * cos(lat1 *PI/180)));

    angle = angle * 180/PI ;
    angle = 90.0 - angle;
    if (angle < 0) angle = 360 + angle;
	return (angle);
}

double fTWS(double RWS, double RWA, double SOG)
{
    double RWA_R = RWA * PI/180.0;
    double TWS = sqrt(pow((RWS * sin(RWA_R)),2) + pow((RWS * cos(RWA_R)- SOG),2));
    return TWS;
}

double fTWA(double RWS, double RWA, double SOG)
{
    double RWA_R = RWA*PI/180.0;
    double TWA_R = atan((RWS * sin(RWA_R))/(RWS * cos(RWA_R)-SOG));
    double TWA = TWA_R * 180.0/PI;
    if (TWA < 0)
        TWA = TWA + 180;
    if(RWA > 180.0)
        TWA = TWA + 180;       // atan only maps to 0->180 (pi/2 -> -pi/2)
    return TWA;
}

double fRWS(double TWS, double TWA, double SOG)
{
    TWA = TWA*PI/180.0;
    double RWS_value = sqrt(pow((TWS * sin(TWA)),2) + pow((TWS * cos(TWA)+ SOG), 2));
    return RWS_value;
}

double fRWA(double TWS, double TWA,double SOG)
{
    double TWA_R = TWA = TWA*PI/180.0;
    double RWA_R = atan((TWS * sin(TWA_R))/(TWS * cos(TWA_R)+ SOG));
    double RWA = RWA_R *180.0 / PI;
    if (RWA < 0)
        RWA = RWA + 180;
    if( TWA > 180 )
        RWA = RWA + 180;               // atan only maps to 0->180 (pi/2 -> -pi/2)
    return RWA;
}

