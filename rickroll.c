#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libetc.h>
#include <stdio.h>
#include <stdint.h>
#include <libetc.h>
#include <stdbool.h>

// Sample vector model
#include "cubetex.c"

#include "src/mod.h"

#define VMODE       0
#define SCREENXRES 320
#define SCREENYRES 240
#define CENTERX     SCREENXRES/2
#define CENTERY     SCREENYRES/2
#define OTLEN       2048        // Maximum number of OT entries
#define PRIMBUFFLEN 32768       // Maximum number of POLY_GT3 primitives

// Display and draw environments, double buffered
DISPENV disp[2];
DRAWENV draw[2];

u_long ot[2][OTLEN];                   // Ordering table (contains addresses to primitives)
char primbuff[2][PRIMBUFFLEN] = {0}; // Primitive list // That's our prim buffer

char * nextpri = primbuff[0];                       // Primitive counter

short db  = 0;                        // Current buffer counter

u_int musicVolume = 35072;

uint8_t sins[360] = {
  127,129,131,134,136,138,140,143,145,147,149,151,154,156,158,160,162,164,166,169,171,173,175,177,179,181,183,185,187,189,191,193,195,196,198,200,
  202,204,205,207,209,211,212,214,216,217,219,220,222,223,225,226,227,229,230,231,233,234,235,236,237,239,240,241,242,243,243,244,245,246,247,248,
  248,249,250,250,251,251,252,252,253,253,253,254,254,254,254,254,254,254,255,254,254,254,254,254,254,254,253,253,253,252,252,251,251,250,250,249,
  248,248,247,246,245,244,243,243,242,241,240,239,237,236,235,234,233,231,230,229,227,226,225,223,222,220,219,217,216,214,212,211,209,207,205,204,
  202,200,198,196,195,193,191,189,187,185,183,181,179,177,175,173,171,169,166,164,162,160,158,156,154,151,149,147,145,143,140,138,136,134,131,129,
  127,125,123,120,118,116,114,111,109,107,105,103,100,98,96,94,92,90,88,85,83,81,79,77,75,73,71,69,67,65,63,61,59,58,56,54,
  52,50,49,47,45,43,42,40,38,37,35,34,32,31,29,28,27,25,24,23,21,20,19,18,17,15,14,13,12,11,11,10,9,8,7,6,
  6,5,4,4,3,3,2,2,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,2,2,3,3,4,4,5,
  6,6,7,8,9,10,11,11,12,13,14,15,17,18,19,20,21,23,24,25,27,28,29,31,32,34,35,37,38,40,42,43,45,47,49,50,
  52,54,56,58,59,61,63,65,67,69,71,73,75,77,79,81,83,85,88,90,92,94,96,98,100,103,105,107,109,111,114,116,118,120,123,125
};

// Prototypes
void init(void);
void display(void);
void LoadTexture(u_long * tim, TIM_IMAGE * tparam);

void init(){
    // Reset the GPU before doing anything and the controller
    PadInit(0);
    ResetGraph(0);
    
    // Initialize and setup the GTE
    InitGeom();
    SetGeomOffset(CENTERX, CENTERY);        // x, y offset
    SetGeomScreen(CENTERX);                 // Distance between eye and screen  
    
        // Set the display and draw environments
    SetDefDispEnv(&disp[0], 0, 0         , SCREENXRES, SCREENYRES);
    SetDefDispEnv(&disp[1], 0, SCREENYRES, SCREENXRES, SCREENYRES);
    
    SetDefDrawEnv(&draw[0], 0, SCREENYRES, SCREENXRES, SCREENYRES);
    SetDefDrawEnv(&draw[1], 0, 0, SCREENXRES, SCREENYRES);
    
    if (VMODE)
    {
        SetVideoMode(MODE_PAL);
        disp[0].screen.y += 8;
        disp[1].screen.y += 8;
    }
    
    SetDispMask(1);

	setRGB0(&draw[0], 40, 40, 40); // set color for first draw area
    setRGB0(&draw[1], 40, 40, 40); // set color for second draw area

    draw[0].isbg = 1;
    draw[1].isbg = 1;

    PutDispEnv(&disp[db]);
    PutDrawEnv(&draw[db]);
        
    // Init font system
    FntLoad(960, 0);
    FntOpen(16, 16, 350, 220, 0, 256);
}

void display(void){
    
    DrawSync(0);
    VSync(0);

    PutDispEnv(&disp[db]);
    PutDrawEnv(&draw[db]);

    DrawOTag(&ot[db][OTLEN - 1]);
    
    db = !db;

    nextpri = primbuff[db];
}

void LoadTexture(u_long * tim, TIM_IMAGE * tparam){     // This part is from Lameguy64's tutorial series : lameguy64.net/svn/pstutorials/chapter1/3-textures.html login/pw: annoyingmous
        OpenTIM(tim);                                   // Open the tim binary data, feed it the address of the data in memory
        ReadTIM(tparam);                                // This read the header of the TIM data and sets the corresponding members of the TIM_IMAGE structure
        
        LoadImage(tparam->prect, tparam->paddr);        // Transfer the data from memory to VRAM at position prect.x, prect.y
        DrawSync(0);                                    // Wait for the drawing to end
        
        if (tparam->mode & 0x8){ // check 4th bit       // If 4th bit == 1, TIM has a CLUT
            LoadImage(tparam->crect, tparam->caddr);    // Load it to VRAM at position crect.x, crect.y
            DrawSync(0);                                // Wait for drawing to end
    }
}

int main() {
    int temp;
    int i;
    int TPressed=0;
    
    long t, p, OTz, Flag;                // t == vertex count, p == depth cueing interpolation value, OTz ==  value to create Z-ordered OT, Flag == see LibOver47.pdf, p.143
    
    POLY_GT3 *poly = {0};                           // pointer to a POLY_G4 
    
    SVECTOR Rotate={ 0 };                   // Rotation coordinates
    VECTOR  Trans={ 0, 0, CENTERX, 0 };     // Translation coordinates
                                            // Scaling coordinates
    VECTOR  Scale={ ONE, ONE, ONE, 0 };     // ONE == 4096
    MATRIX  Matrix={0};                     // Matrix data for the GTE
    
    // Texture window
    
    DR_MODE * dr_mode;                        // Pointer to dr_mode prim
    
    RECT tws = {0, 0, 32, 32};            // Texture window coordinates : x, y, w, h
                
    init();
    
    LoadTexture(_binary____TIM_ricktex_tim_start, &tim_cube);
	
	loadMod();
    startMusic();
    MOD_SetMusicVolume(musicVolume);
    
    // Main loop
    while (1) {
		temp++;
		if(temp == 360) temp = 0;
		setRGB0(&draw[0], sins[temp], sins[(temp+120)%360], sins[(temp+240)%360]);
		setRGB0(&draw[1], sins[temp], sins[(temp+120)%360], sins[(temp+240)%360]);
		
		
        Rotate.vy += 8; // Pan
        Rotate.vx += 8; // Tilt
        //~ Rotate.vz += 8; // Roll
        
        
        // Clear the current OT
        ClearOTagR(ot[db], OTLEN);
        
        // Convert and set the matrixes
        RotMatrix(&Rotate, &Matrix);
        TransMatrix(&Matrix, &Trans);
        ScaleMatrix(&Matrix, &Scale);
        
        SetRotMatrix(&Matrix);
        SetTransMatrix(&Matrix);
        
        
        // Render the sample vector model
        t=0;
        
        // modelCube is a TMESH, len member == # vertices, but here it's # of triangle... So, for each tri * 3 vertices ...
        for (i = 0; i < (modelCube.len*3); i += 3) {               
            
            poly = (POLY_GT3 *)nextpri;
            
            // Initialize the primitive and set its color values
            
            SetPolyGT3(poly);

            ((POLY_GT3 *)poly)->tpage = getTPage(tim_cube.mode&0x3, 0,
                                                 tim_cube.prect->x,
                                                 tim_cube.prect->y
                                                );
			
			setRGB0(poly, 128, 128, 128);
            setRGB1(poly, 128, 128, 128);
            setRGB2(poly, 128, 128, 128);

            setUV3(poly, modelCube.u[i].vx, modelCube.u[i].vy,
                         modelCube.u[i+2].vx, modelCube.u[i+2].vy,
                         modelCube.u[i+1].vx, modelCube.u[i+1].vy);
                         
            // Rotate, translate, and project the vectors and output the results into a primitive

            OTz  = RotTransPers(&modelCube_mesh[modelCube_index[t]]  , (long*)&poly->x0, &p, &Flag);
            OTz += RotTransPers(&modelCube_mesh[modelCube_index[t+2]], (long*)&poly->x1, &p, &Flag);
            OTz += RotTransPers(&modelCube_mesh[modelCube_index[t+1]], (long*)&poly->x2, &p, &Flag);
            
            // Sort the primitive into the OT
            OTz /= 3;
            if ((OTz > 0) && (OTz < OTLEN))
                AddPrim(&ot[db][OTz-2], poly);
            
            nextpri += sizeof(POLY_GT3);
            
            t+=3;
            
        }
        
            dr_mode = (DR_MODE *)nextpri;
            
            setDrawMode(dr_mode,1,0, getTPage(tim_cube.mode&0x3, 0,
                                              tim_cube.prect->x,
                                              tim_cube.prect->y), &tws);  //set texture window
        
            AddPrim(&ot[db], dr_mode);
            
            nextpri += sizeof(DR_MODE);
        

        FntPrint("Rick Rolled UwU\n");
        
        FntFlush(-1);
        
        display();

    }
    return 0;
}
