/*------------------------------------------------------------------------*
 NAME:     querytopo2.cpp

 PURPOSE:  Queries global topographic and geoid databases and returns
           topography and geoid heights for a given lat/lon (in deg) point.
           User specifies whether topography should be returned relative
           to geoid or WGS-84 ellipsoid.  This is an evolution of the
           original querytopo.cpp, replacing Bedmap-2 and GIMP90 DEMS
           with more modern REMA and ArcticDEM databases.

 AUTHOR:   John Gary Sonntag

 DATE:     24 March 2020
 *------------------------------------------------------------------------*/

#include "/home/sonntag/Include/mission.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>


main(int argc, char *argv[])
{
  char line[85],demid[10],wpname[10],geoidid[10];
  int htrefflag;
  double lat,lon,topo,geoid;
  double querytopo(double,double,char *);
  double querygeoid(double,double,char *);
  int initquerytopo(),closequerytopo();
  int initquerygeoid(),closequerygeoid();
  FILE *fptr;

  // Check input
  if (argc != 3)
  {
    printf("Usage: querytopo2 <latlon filename> <height ref (1=geoid 2=ellipsoid)>\n");
    exit(0);
  }

  // Open the input file
  if ((fptr=fopen(argv[1],"r"))==NULL)
  {
    printf("Input file %s not found - exiting\n",argv[1]);
    exit(-1);
  }
  htrefflag = atoi(argv[2]);
  if (htrefflag<1||htrefflag>2)
  {
    printf("Unrecognized height reference - exiting\n");
    exit(-1);
  }

  // Loop over the input file entries
  initquerygeoid();
  initquerytopo();
  while(fgets(line,85,fptr)!=NULL)
  {

    // Parse the input and ensure longitude is within bounds
    sscanf(line,"%lf %lf %s",&lat,&lon,wpname);
    while (lon<=-180.0) lon+=360.0;
    while (lon>180.0) lon-=360.0;

    // Process the request if latitude is within bounds
    if (lat>=-90.0&&lat<=90.0)
    {

      // Query the topo and geoid databases
      topo = querytopo(lat,lon,demid);
      geoid = querygeoid(lat,lon,geoidid);

      // Reference the topo heights according to request and native reference of database
      if (!strncmp(demid,"GT3\0",3))  // GTOPO30 referenced to mean sea level (geoid)
      {
        if (htrefflag==2) topo = topo+geoid;
        if (isnan(topo)) topo = -9999.0;
      }
      else if (!strncmp(demid,"BM2\0",3))  // BEDMAP2 referenced to geoid
      {
        if (htrefflag==2) topo = topo+geoid;
        if (isnan(topo)) topo = -9999.0;
      }
      else if (!strncmp(demid,"G90\0",3))  // GIMP90 referenced to WGS84 ellipsoid
      {
        if (htrefflag==1) topo = topo-geoid;
        if (isnan(topo)) topo = -9999.0;
      }
      else if (!strncmp(demid,"AD1\0",3))  // ArcticDEM referenced to WGS84 ellipsoid
      {
        if (htrefflag==1) topo = topo-geoid;
        if (isnan(topo)) topo = -9999.0;
      }
      else if (!strncmp(demid,"REP\0",3))  // REMA Peninsula referenced to WGS84 ellipsoid
      {
        if (htrefflag==1) topo = topo-geoid;
        if (isnan(topo)) topo = -9999.0;
      }
      else if (!strncmp(demid,"REM\0",3))  // REMA referenced to WGS84 ellipsoid
      {
        if (htrefflag==1) topo = topo-geoid;
        if (isnan(topo)) topo = -9999.0;
      }

      // Output the result
      printf("%8.4lf %9.4lf %8.2lf %3s %7.2lf %3s\n",lat,lon,topo,demid,geoid,geoidid);

    }

    else
      printf("latitude of %lf is out of bounds\n",lat);

  }

  // Close the input file
  closequerygeoid();
  closequerytopo();
  fclose(fptr);

}


#define EGM96PATH "/usr/local/share/geoid/egm96/WW15MGH.DAC\0"
#define EGM08PATH "/usr/local/share/geoid/egm2008/Und_min1x1_egm2008_isw=82_WGS84_TideFree_SE\0"
#define GTOPO30PATH "/usr/local/share/dem/gtopo30/\0"
#define BEDMAP2PATH "/usr/local/share/dem/bedmap2/bedmap2_surface.flt\0"
#define ARCTICDEM100PATH "/usr/local/share/dem/arcticdem100/arcticdem_mosaic_100m_v3.0.flt\0"
#define REMP100PATH "/usr/local/share/dem/rema100/REMA_100m_peninsula_dem_filled.flt\0"
#define REMA100PATH "/usr/local/share/dem/rema100/REMA_100m_dem.flt\0"
#define GIMP90PATH "/usr/local/share/dem/gimp90/gimp90m.dem\0"
#define C 299792458.0
#define WE 7.292115147e-5
#define MU 3.986005005e14
#define PI (4.0*atan((double)(1.0)))
#define AE 6378137.0
#define FLAT (1.0/298.257223563)
#define RAD2NM (180.0*60.0/PI)
#define RAD2KM (RAD2NM*6076.1*12.0*2.54/100.0/1000.0)

FILE *fptrdem;
FILE *fptrgeoid;
int opendem; // -1 nothing open
             // 0-32 corresponding GTOPO30 tile file open
             // 33 GIMP90 file open
             // 34 Bedmap-2 file open
             // 35 ArcticDEM-100m file open
             // 36 REMA Peninsula-100m filled file open
             // 37 REMA-100m file open
int opengeoid; // -1 nothing open
             // 0 EGM2008 open


int initquerytopo()
{
  opendem = -1; // No DEM files are open at start
}


int closequerytopo()
{
  //printf("closing previously open DEM\n");
  if (opendem!=-1) fclose(fptrdem);
  opendem=-1;
}


int initquerygeoid()
{
  opengeoid = -1; // No geoid files are open at start
}


int closequerygeoid()
{
  //printf("closing previously open geoid\n");
  if (opengeoid!=-1) fclose(fptrgeoid);
  opengeoid=-1;
}


double querytopo(double lat, double lon, char *demid)
{
  int repflag,remflag;
  double x,y,topo;
  //int bm2flag,g90flag,ad1flag;
  double rempx[5],rempy[5],remax[5],remay[5];
  double querygtopo30(double, double);
  double queryarcticdem100(double, double);
  double queryremp(double, double);
  double queryrema(double, double);
  bool pointinpolygon(double,double,double *,double *,int);

  // Define the REMA Peninsula (filled) boundary
  rempx[0] = -2700000.0;  rempy[0] =  1800000.0;
  rempx[1] = -1900000.0;  rempy[1] =  1800000.0;
  rempx[2] = -1900000.0;  rempy[2] =   800000.0;
  rempx[3] = -2700000.0;  rempy[3] =   800000.0;
  rempx[4] = -2700000.0;  rempy[4] =  1800000.0;

  // Define the REMA boundary
  remax[0] = -2700000.0;  remay[0] =  2300000.0;
  remax[1] =  2800000.0;  remay[1] =  2300000.0;
  remax[2] =  2800000.0;  remay[2] = -2204200.0;
  remax[3] = -2700000.0;  remay[3] = -2204200.0;
  remax[4] = -2700000.0;  remay[4] =  2300000.0;

  // Northern hemisphere
  if (lat>=0.0)
  {

    // Try ArcticDEM-100m
    geod2ps(lat,lon,70.0,-45.0,1.0,AE,FLAT,&x,&y);
    strcpy(demid,"AD1\0");
    topo = queryarcticdem100(x,y);  // answer is relative to the WGS-84 ellipsoid

    // Go to GTOPO30 if ArcticDEM returns unknown
    if (topo==-9999.0)
    {
      strcpy(demid,"GT3\0");
      topo = querygtopo30(lat,lon);  // answer is relative to mean sea level
    }

  }

  // Southern hemisphere
  else
  {

    // Determine if input coords are within REMA-Peninsula or REMA limits
    geod2ps(lat,lon,-71.0,0.0,1.0,AE,FLAT,&x,&y);
    repflag = pointinpolygon(x,y,rempx,rempy,5);
    remflag = pointinpolygon(x,y,remax,remay,5);
    if (repflag)
    {
      topo = queryremp(x,y);
      strcpy(demid,"REP\0");      
    }
    else if (remflag)
    {
      topo = queryrema(x,y);
      strcpy(demid,"REM\0");      
    }
    else
    {
      topo = querygtopo30(lat,lon);
      strcpy(demid,"GT3\0");      
    }

    // Query GTOPO30 if one of the REMAs returned a no data flag
    if (topo==-9999.0&&(repflag||remflag))
    {
      topo = querygtopo30(lat,lon);
      strcpy(demid,"GT3\0");      
    }

  }

  // Return the result
  return(topo);

}


double queryremp(double x,double y)
{
  long long int nx,ny,n1,n2,m1,m2;
  long long int offsetq11,offsetq21,offsetq12,offsetq22;
  float q11,q21,q12,q22;
  double x0,y0,mdbl,ndbl,p,f1,f2,f3,f4,denom,x1,x2,y1,y2;

  // Define size of grid
  x0 =-2700000.0;
  y0 = 1800000.0;
  nx =  8000;
  ny = 10000;

  // Determine row and column of surrounding grid cells
  mdbl = (x-x0)/100.0-0.5;
  if (mdbl<0.0)
  {
    m1 = 0;
    m2 = 0;
  }
  else if (mdbl>(nx-1)) 
  {
    m1 = nx-1;
    m2 = nx-1;
  }
  else
  {
    m1 = int(mdbl);
    m2 = m1+1;
  }
  ndbl = (y0-y)/100.0-0.5;
  if (ndbl<0.0)
  {
    n1 = 0;
    n2 = 0;
  }
  else if (ndbl>(ny-1)) 
  {
    n1 = ny-1;
    n2 = ny-1;
  }
  else
  {
    n1 = int(ndbl);
    n2 = n1+1;
  }
  offsetq11 = (long long)4*(n1*nx+m1);
  offsetq21 = (long long)4*(n1*nx+m2);
  offsetq12 = (long long)4*(n2*nx+m1);
  offsetq22 = (long long)4*(n2*nx+m2);

  // Open REMA Peninsula 100m Filled DEM file if not already open
  if (opendem==-1)
  {
    if ((fptrdem=fopen(REMP100PATH,"r"))==NULL) return(-9999.9);
    opendem = 36;
  }
  else if (opendem!=36)
  {
    fclose(fptrdem);
    if ((fptrdem=fopen(REMP100PATH,"r"))==NULL) return(-9999.9);
    opendem = 36;    
  }

  // Read the four surrounding pixels from the DEM file
  fseek(fptrdem,offsetq11,SEEK_SET);
  fread(&q11,4,1,fptrdem);
  fseek(fptrdem,offsetq21,SEEK_SET);
  fread(&q21,4,1,fptrdem);
  fseek(fptrdem,offsetq12,SEEK_SET);
  fread(&q12,4,1,fptrdem);
  fseek(fptrdem,offsetq22,SEEK_SET);
  fread(&q22,4,1,fptrdem);

  // Compute terrain at requested lat/lon by bilinear interpolation
  if (q11==-9999.0||q21==-9999.0||q12==-9999.0||q22==-9999.0)
    p = -9999.0;
  else
  {
    x1 = x0+(m1+0.5)*100.0;
    x2 = x0+(m2+0.5)*100.0;
    y1 = y0-(n1+0.5)*100.0;
    y2 = y0-(n2+0.5)*100.0;
    denom = (x2-x1)*(y2-y1);
    f1 = ((x2-x)*(y2-y))/denom;
    f2 = ((x-x1)*(y2-y))/denom;
    f3 = ((x2-x)*(y-y1))/denom;
    f4 = ((x-x1)*(y-y1))/denom;
    p = f1*q11 + f2*q21 + f3*q12 + f4*q22;
  }
  return(p);

}


double queryrema(double x,double y)
{
  long long int nx,ny,n1,n2,m1,m2;
  long long int offsetq11,offsetq21,offsetq12,offsetq22;
  float q11,q21,q12,q22;
  double x0,y0,mdbl,ndbl,p,f1,f2,f3,f4,denom,x1,x2,y1,y2;

  // Define size of grid
  x0 =-2700000.0;
  y0 = 2300000.0;
  nx = 55000;
  ny = 45042;

  // Determine row and column of surrounding grid cells
  mdbl = (x-x0)/100.0-0.5;
  if (mdbl<0.0)
  {
    m1 = 0;
    m2 = 0;
  }
  else if (mdbl>(nx-1)) 
  {
    m1 = nx-1;
    m2 = nx-1;
  }
  else
  {
    m1 = int(mdbl);
    m2 = m1+1;
  }
  ndbl = (y0-y)/100.0-0.5;
  if (ndbl<0.0)
  {
    n1 = 0;
    n2 = 0;
  }
  else if (ndbl>(ny-1)) 
  {
    n1 = ny-1;
    n2 = ny-1;
  }
  else
  {
    n1 = int(ndbl);
    n2 = n1+1;
  }
  offsetq11 = (long long)4*(n1*nx+m1);
  offsetq21 = (long long)4*(n1*nx+m2);
  offsetq12 = (long long)4*(n2*nx+m1);
  offsetq22 = (long long)4*(n2*nx+m2);

  // Open REMA 100m DEM file if not already open
  if (opendem==-1)
  {
    if ((fptrdem=fopen(REMA100PATH,"r"))==NULL) return(-9999.9);
    opendem = 37;
  }
  else if (opendem!=37)
  {
    fclose(fptrdem);
    if ((fptrdem=fopen(REMA100PATH,"r"))==NULL) return(-9999.9);
    opendem = 37;    
  }

  // Read the four surrounding pixels from the DEM file
  fseek(fptrdem,offsetq11,SEEK_SET);
  fread(&q11,4,1,fptrdem);
  fseek(fptrdem,offsetq21,SEEK_SET);
  fread(&q21,4,1,fptrdem);
  fseek(fptrdem,offsetq12,SEEK_SET);
  fread(&q12,4,1,fptrdem);
  fseek(fptrdem,offsetq22,SEEK_SET);
  fread(&q22,4,1,fptrdem);

  // Compute terrain at requested lat/lon by bilinear interpolation
  if (q11==-9999.0||q21==-9999.0||q12==-9999.0||q22==-9999.0)
    p = -9999.0;
  else
  {
    x1 = x0+(m1+0.5)*100.0;
    x2 = x0+(m2+0.5)*100.0;
    y1 = y0-(n1+0.5)*100.0;
    y2 = y0-(n2+0.5)*100.0;
    denom = (x2-x1)*(y2-y1);
    f1 = ((x2-x)*(y2-y))/denom;
    f2 = ((x-x1)*(y2-y))/denom;
    f3 = ((x2-x)*(y-y1))/denom;
    f4 = ((x-x1)*(y-y1))/denom;
    p = f1*q11 + f2*q21 + f3*q12 + f4*q22;
  }
  return(p);

}


double querygimp90(double x,double y)
{
  short sdtemp,q11,q21,q12,q22;
  int nx,ny,m1,m2,n1,n2;
  long int offsetq11,offsetq21,offsetq12,offsetq22;
  double x0,y0,mdbl,ndbl;
  double x1,x2,y1,y2,denom,f1,f2,f3,f4,p;
  void byteswap(char *,char *,int);

  // Define size of grid
  x0 =-639955.0;
  y0 =-655595.0;
  nx = 16620;
  ny = 30000;

  // Determine row and column of surrounding grid cells
  mdbl = (x-x0)/90.0-0.5;
  if (mdbl<0.0)
  {
    m1 = 0;
    m2 = 0;
  }
  else if (mdbl>(nx-1)) 
  {
    m1 = nx-1;
    m2 = nx-1;
  }
  else
  {
    m1 = int(mdbl);
    m2 = m1+1;
  }
  ndbl = (y0-y)/90.0-0.5;
  if (ndbl<0.0)
  {
    n1 = 0;
    n2 = 0;
  }
  else if (ndbl>(ny-1)) 
  {
    n1 = ny-1;
    n2 = ny-1;
  }
  else
  {
    n1 = int(ndbl);
    n2 = n1+1;
  }
  offsetq11 = 2*(n1*nx+m1);
  offsetq21 = 2*(n1*nx+m2);
  offsetq12 = 2*(n2*nx+m1);
  offsetq22 = 2*(n2*nx+m2);
  //printf("x: %lf  y: %lf\n",x,y);
  //printf("ncols: %d  nrows: %d\n",nx,ny);
  //printf("double col# %lf  double row# %lf\n",mdbl,ndbl);
  //printf("m1: %d  m2:%d\n",m1,m2);
  //printf("n1: %d  n2: %d\n",n1,n2);
  //printf("offsetq11=%ld  offsetq21=%ld\n",offsetq11,offsetq21);
  //printf("offsetq12=%ld  offsetq22=%ld\n",offsetq12,offsetq22);

  // Open GIMP90m DEM file if not already open
  if (opendem==-1)
  {
    //printf("no DEM open, opening GIMP90 DEM file\n");
    if ((fptrdem=fopen(GIMP90PATH,"r"))==NULL) return(-9999.9);
    opendem = 33;
  }
  else if (opendem!=33)
  {
    //printf("closing previously open DEM, opening GIMP90 DEM file\n");
    fclose(fptrdem);
    if ((fptrdem=fopen(GIMP90PATH,"r"))==NULL) return(-9999.9);
    opendem = 33;    
  }

  // Read the four surrounding pixels from the DEM file
  fseek(fptrdem,offsetq11,SEEK_SET);
  fread(&q11,2,1,fptrdem);
  if (q11==-9999.0) q11 = 0.0;
  fseek(fptrdem,offsetq21,SEEK_SET);
  fread(&q21,2,1,fptrdem);
  if (q21==-9999.0) q21 = 0.0;
  fseek(fptrdem,offsetq12,SEEK_SET);
  fread(&q12,2,1,fptrdem);
  if (q12==-9999.0) q12 = 0.0;
  fseek(fptrdem,offsetq22,SEEK_SET);
  fread(&q22,2,1,fptrdem);
  if (q22==-9999.0) q22 = 0.0;
  //printf("q11 = %d  q21 = %d\n",q11,q21);
  //printf("q12 = %d  q22 = %d\n",q12,q22);

  // Compute terrain at requested lat/lon by bilinear interpolation
  x1 = x0+(m1+0.5)*90.0;
  x2 = x0+(m2+0.5)*90.0;
  y1 = y0-(n1+0.5)*90.0;
  y2 = y0-(n2+0.5)*90.0;
  //printf("x1 = %lf  x2 = %lf\n",x1,x2);
  //printf("y1 = %lf  y2 = %lf\n",y1,y2);
  denom = (x2-x1)*(y2-y1);
  f1 = ((x2-x)*(y2-y))/denom;
  f2 = ((x-x1)*(y2-y))/denom;
  f3 = ((x2-x)*(y-y1))/denom;
  f4 = ((x-x1)*(y-y1))/denom;
  p = f1*q11 + f2*q21 + f3*q12 + f4*q22;
  return(p);

}


double querybedmap2(double x,double y)
{
  int nx,ny,m1,m2,n1,n2;
  long int offsetq11,offsetq21,offsetq12,offsetq22;
  float q11,q21,q12,q22;
  double x0,y0,mdbl,ndbl,x1,x2,y1,y2,denom,f1,f2,f3,f4,p;
  void byteswap(char *,char *,int);

  // Define size of grid
  x0 =-3333500.0;
  y0 = 3333500.0;
  nx = 6667;
  ny = 6667;

  // Determine row and column of surrounding grid cells
  mdbl = (x-x0)/1000.0-0.5;
  if (mdbl<0.0)
  {
    m1 = 0;
    m2 = 0;
  }
  else if (mdbl>(nx-1)) 
  {
    m1 = nx-1;
    m2 = nx-1;
  }
  else
  {
    m1 = int(mdbl);
    m2 = m1+1;
  }
  ndbl = (y0-y)/1000.0-0.5;
  if (ndbl<0.0)
  {
    n1 = 0;
    n2 = 0;
  }
  else if (ndbl>(ny-1)) 
  {
    n1 = ny-1;
    n2 = ny-1;
  }
  else
  {
    n1 = int(ndbl);
    n2 = n1+1;
  }
  offsetq11 = 4*(n1*nx+m1);
  offsetq21 = 4*(n1*nx+m2);
  offsetq12 = 4*(n2*nx+m1);
  offsetq22 = 4*(n2*nx+m2);
  //printf("ncols: %d  nrows: %d\n",nx,ny);
  //printf("double col# %lf  double row# %lf\n",mdbl,ndbl);
  //printf("m1: %d  m2:%d\n",m1,m2);
  //printf("n1: %d  n2: %d\n",n1,n2);
  //printf("offsetq11=%ld  offsetq21=%ld\n",offsetq11,offsetq21);
  //printf("offsetq12=%ld  offsetq22=%ld\n",offsetq12,offsetq22);

  // Open BEDMAP-2 DEM file if not already open
  if (opendem==-1)
  {
    //printf("no DEM open, opening Bedmap-2 DEM file\n");
    if ((fptrdem=fopen(BEDMAP2PATH,"r"))==NULL) return(-9999.9);
    opendem = 34;
  }
  else if (opendem!=34)
  {
    //printf("closing previously open DEM, opening Bedmap-2 DEM file\n");
    fclose(fptrdem);
    if ((fptrdem=fopen(BEDMAP2PATH,"r"))==NULL) return(-9999.9);
    opendem = 34;    
  }

  // Read the four surrounding pixels from the DEM file
  fseek(fptrdem,offsetq11,SEEK_SET);
  fread(&q11,4,1,fptrdem);
  //if (q11==-9999.0) q11 = 0.0;
  fseek(fptrdem,offsetq21,SEEK_SET);
  fread(&q21,4,1,fptrdem);
  //if (q21==-9999.0) q21 = 0.0;
  fseek(fptrdem,offsetq12,SEEK_SET);
  fread(&q12,4,1,fptrdem);
  //if (q12==-9999.0) q12 = 0.0;
  fseek(fptrdem,offsetq22,SEEK_SET);
  fread(&q22,4,1,fptrdem);
  //if (q22==-9999.0) q22 = 0.0;
  //printf("q11 = %f  q21 = %f\n",q11,q21);
  //printf("q12 = %f  q22 = %f\n",q12,q22);

  // Compute terrain at requested lat/lon by bilinear interpolation
  if (q11==-9999.0||q21==-9999.0||q12==-9999.0||q22==-9999.0)
    p = -9999.0;
  else
  {
    x1 = x0+(m1+0.5)*1000.0;
    x2 = x0+(m2+0.5)*1000.0;
    y1 = y0-(n1+0.5)*1000.0;
    y2 = y0-(n2+0.5)*1000.0;
    //printf("x1 = %lf  x2 = %lf\n",x1,x2);
    //printf("y1 = %lf  y2 = %lf\n",y1,y2);
    denom = (x2-x1)*(y2-y1);
    f1 = ((x2-x)*(y2-y))/denom;
    f2 = ((x-x1)*(y2-y))/denom;
    f3 = ((x2-x)*(y-y1))/denom;
    f4 = ((x-x1)*(y-y1))/denom;
    p = f1*q11 + f2*q21 + f3*q12 + f4*q22;
  }

  // Return the result
  return(p);

}


double queryarcticdem100(double x,double y)
{
  long long int nx,ny,m1,m2,n1,n2;
  long long int offsetq11,offsetq21,offsetq12,offsetq22;
  float q11,q21,q12,q22;
  double x0,y0,mdbl,ndbl,x1,x2,y1,y2,denom,f1,f2,f3,f4,p;
  void byteswap(char *,char *,int);

  // Define size of grid
  //printf("x=%lf y=%lf\n",x,y);
  x0 =-4000000.0;
  y0 = 4100000.0;
  nx = 74000;
  ny = 75000;

  // Determine row and column of surrounding grid cells
  mdbl = (x-x0)/100.0-0.5;
  if (mdbl<0.0||mdbl>(nx-1))  // Requested coordinates are outside x bounds of DEM
  {
    return(-9999.0);
  }
  else
  {
    m1 = int(mdbl);
    m2 = m1+1;
  }
  ndbl = (y0-y)/100.0-0.5;
  if (ndbl<0.0||ndbl>(ny-1))  // Requested coordinates are outside y bounds of DEM
  {
    return(-9999.0);
  }
  else
  {
    n1 = int(ndbl);
    n2 = n1+1;
  }
  offsetq11 = (long long)4*(n1*nx+m1);
  offsetq21 = (long long)4*(n1*nx+m2);
  offsetq12 = (long long)4*(n2*nx+m1);
  offsetq22 = (long long)4*(n2*nx+m2);
  //printf("nx: %lld  ny: %lld\n",nx,ny);
  //printf("double col# %lf  double row# %lf\n",mdbl,ndbl);
  //printf("m1: %lld  m2:%lld\n",m1,m2);
  //printf("n1: %lld  n2: %lld\n",n1,n2);
  //printf("offsetq11=%lld  offsetq21=%lld\n",offsetq11,offsetq21);
  //printf("offsetq12=%lld  offsetq22=%lld\n",offsetq12,offsetq22);

  // Open ArcticDEM-100m DEM file if not already open
  if (opendem==-1)
  {
    if ((fptrdem=fopen(ARCTICDEM100PATH,"r"))==NULL) return(-9999.9);
    opendem = 35;
  }
  else if (opendem!=35)
  {
    fclose(fptrdem);
    if ((fptrdem=fopen(ARCTICDEM100PATH,"r"))==NULL) return(-9999.9);
    opendem = 35;    
  }

  // Read the four surrounding pixels from the DEM file
  fseek(fptrdem,offsetq11,SEEK_SET);
  fread(&q11,4,1,fptrdem);
  fseek(fptrdem,offsetq21,SEEK_SET);
  fread(&q21,4,1,fptrdem);
  fseek(fptrdem,offsetq12,SEEK_SET);
  fread(&q12,4,1,fptrdem);
  fseek(fptrdem,offsetq22,SEEK_SET);
  fread(&q22,4,1,fptrdem);
  //printf("q11=%f q21=%f\n",q11,q21);
  //printf("q12=%f q22=%f\n",q12,q22);

  // Compute terrain at requested lat/lon by bilinear interpolation
  if (q11==-9999.0||q21==-9999.0||q12==-9999.0||q22==-9999.0)
    p = -9999.0;
  else
  {
    x1 = x0+(m1+0.5)*100.0;
    x2 = x0+(m2+0.5)*100.0;
    y1 = y0-(n1+0.5)*100.0;
    y2 = y0-(n2+0.5)*100.0;
    denom = (x2-x1)*(y2-y1);
    f1 = ((x2-x)*(y2-y))/denom;
    f2 = ((x-x1)*(y2-y))/denom;
    f3 = ((x2-x)*(y-y1))/denom;
    f4 = ((x-x1)*(y-y1))/denom;
    p = f1*q11 + f2*q21 + f3*q12 + f4*q22;
  }

  // Return the result
  return(p);

}



double querygtopo30(double lat,double lon)
{
  bool flag;
  char tilename[35][15],filename[120];
  short sdtemp,q11,q21,q12,q22;
  int ntiles,i,nlon,nlat,n1,n2,m1,m2;
  long int offsetq11,offsetq21,offsetq12,offsetq22;
  double mdbl,ndbl,lon1,lon2,lat1,lat2;
  double denom,f1,f2,f3,f4,p;
  double tilelat[35][5],tilelon[40][5];
  bool pointinpolygon(double,double,double *,double *,int);
  void byteswap(char *,char *,int);

  // Define the GTOPO30 tile boundaries
  ntiles = 33;
  strcpy(tilename[ 0],"W180N90.DEM\0");
  tilelat[ 0][0] = 90.0;  tilelon[ 0][0] =-180.0;
  tilelat[ 0][1] = 90.0;  tilelon[ 0][1] =-140.0;
  tilelat[ 0][2] = 40.0;  tilelon[ 0][2] =-140.0;
  tilelat[ 0][3] = 40.0;  tilelon[ 0][3] =-180.0;
  tilelat[ 0][4] = 90.0;  tilelon[ 0][4] =-180.0;
  strcpy(tilename[ 1],"W140N90.DEM\0");
  tilelat[ 1][0] = 90.0;  tilelon[ 1][0] =-140.0;
  tilelat[ 1][1] = 90.0;  tilelon[ 1][1] =-100.0;
  tilelat[ 1][2] = 40.0;  tilelon[ 1][2] =-100.0;
  tilelat[ 1][3] = 40.0;  tilelon[ 1][3] =-140.0;
  tilelat[ 1][4] = 90.0;  tilelon[ 1][4] =-140.0;
  strcpy(tilename[ 2],"W100N90.DEM\0");
  tilelat[ 2][0] = 90.0;  tilelon[ 2][0] =-100.0;
  tilelat[ 2][1] = 90.0;  tilelon[ 2][1] =-060.0;
  tilelat[ 2][2] = 40.0;  tilelon[ 2][2] =-060.0;
  tilelat[ 2][3] = 40.0;  tilelon[ 2][3] =-100.0;
  tilelat[ 2][4] = 90.0;  tilelon[ 2][4] =-100.0;
  strcpy(tilename[ 3],"W060N90.DEM\0");
  tilelat[ 3][0] = 90.0;  tilelon[ 3][0] =-060.0;
  tilelat[ 3][1] = 90.0;  tilelon[ 3][1] =-020.0;
  tilelat[ 3][2] = 40.0;  tilelon[ 3][2] =-020.0;
  tilelat[ 3][3] = 40.0;  tilelon[ 3][3] =-060.0;
  tilelat[ 3][4] = 90.0;  tilelon[ 3][4] =-060.0;
  strcpy(tilename[ 4],"W020N90.DEM\0");
  tilelat[ 4][0] = 90.0;  tilelon[ 4][0] =-020.0;
  tilelat[ 4][1] = 90.0;  tilelon[ 4][1] = 020.0;
  tilelat[ 4][2] = 40.0;  tilelon[ 4][2] = 020.0;
  tilelat[ 4][3] = 40.0;  tilelon[ 4][3] =-020.0;
  tilelat[ 4][4] = 90.0;  tilelon[ 4][4] =-020.0;
  strcpy(tilename[ 5],"E020N90.DEM\0");
  tilelat[ 5][0] = 90.0;  tilelon[ 5][0] = 020.0;
  tilelat[ 5][1] = 90.0;  tilelon[ 5][1] = 060.0;
  tilelat[ 5][2] = 40.0;  tilelon[ 5][2] = 060.0;
  tilelat[ 5][3] = 40.0;  tilelon[ 5][3] = 020.0;
  tilelat[ 5][4] = 90.0;  tilelon[ 5][4] = 020.0;
  strcpy(tilename[ 6],"E060N90.DEM\0");
  tilelat[ 6][0] = 90.0;  tilelon[ 6][0] = 060.0;
  tilelat[ 6][1] = 90.0;  tilelon[ 6][1] = 100.0;
  tilelat[ 6][2] = 40.0;  tilelon[ 6][2] = 100.0;
  tilelat[ 6][3] = 40.0;  tilelon[ 6][3] = 060.0;
  tilelat[ 6][4] = 90.0;  tilelon[ 6][4] = 060.0;
  strcpy(tilename[ 7],"E100N90.DEM\0");
  tilelat[ 7][0] = 90.0;  tilelon[ 7][0] = 100.0;
  tilelat[ 7][1] = 90.0;  tilelon[ 7][1] = 140.0;
  tilelat[ 7][2] = 40.0;  tilelon[ 7][2] = 140.0;
  tilelat[ 7][3] = 40.0;  tilelon[ 7][3] = 100.0;
  tilelat[ 7][4] = 90.0;  tilelon[ 7][4] = 100.0;
  strcpy(tilename[ 8],"E140N90.DEM\0");
  tilelat[ 8][0] = 90.0;  tilelon[ 8][0] = 140.0;
  tilelat[ 8][1] = 90.0;  tilelon[ 8][1] = 180.0;
  tilelat[ 8][2] = 40.0;  tilelon[ 8][2] = 180.0;
  tilelat[ 8][3] = 40.0;  tilelon[ 8][3] = 140.0;
  tilelat[ 8][4] = 90.0;  tilelon[ 8][4] = 140.0;
  strcpy(tilename[ 9],"W180N40.DEM\0");
  tilelat[ 9][0] = 40.0;  tilelon[ 9][0] =-180.0;
  tilelat[ 9][1] = 40.0;  tilelon[ 9][1] =-140.0;
  tilelat[ 9][2] =-10.0;  tilelon[ 9][2] =-140.0;
  tilelat[ 9][3] =-10.0;  tilelon[ 9][3] =-180.0;
  tilelat[ 9][4] = 40.0;  tilelon[ 9][4] =-180.0;
  strcpy(tilename[10],"W140N40.DEM\0");
  tilelat[10][0] = 40.0;  tilelon[10][0] =-140.0;
  tilelat[10][1] = 40.0;  tilelon[10][1] =-100.0;
  tilelat[10][2] =-10.0;  tilelon[10][2] =-100.0;
  tilelat[10][3] =-10.0;  tilelon[10][3] =-140.0;
  tilelat[10][4] = 40.0;  tilelon[10][4] =-140.0;
  strcpy(tilename[11],"W100N40.DEM\0");
  tilelat[11][0] = 40.0;  tilelon[11][0] =-100.0;
  tilelat[11][1] = 40.0;  tilelon[11][1] =-060.0;
  tilelat[11][2] =-10.0;  tilelon[11][2] =-060.0;
  tilelat[11][3] =-10.0;  tilelon[11][3] =-100.0;
  tilelat[11][4] = 40.0;  tilelon[11][4] =-100.0;
  strcpy(tilename[12],"W060N40.DEM\0");
  tilelat[12][0] = 40.0;  tilelon[12][0] =-060.0;
  tilelat[12][1] = 40.0;  tilelon[12][1] =-020.0;
  tilelat[12][2] =-10.0;  tilelon[12][2] =-020.0;
  tilelat[12][3] =-10.0;  tilelon[12][3] =-060.0;
  tilelat[12][4] = 40.0;  tilelon[12][4] =-060.0;
  strcpy(tilename[13],"W020N40.DEM\0");
  tilelat[13][0] = 40.0;  tilelon[13][0] =-020.0;
  tilelat[13][1] = 40.0;  tilelon[13][1] = 020.0;
  tilelat[13][2] =-10.0;  tilelon[13][2] = 020.0;
  tilelat[13][3] =-10.0;  tilelon[13][3] =-020.0;
  tilelat[13][4] = 40.0;  tilelon[13][4] =-020.0;
  strcpy(tilename[14],"E020N40.DEM\0");
  tilelat[14][0] = 40.0;  tilelon[14][0] = 020.0;
  tilelat[14][1] = 40.0;  tilelon[14][1] = 060.0;
  tilelat[14][2] =-10.0;  tilelon[14][2] = 060.0;
  tilelat[14][3] =-10.0;  tilelon[14][3] = 020.0;
  tilelat[14][4] = 40.0;  tilelon[14][4] = 020.0;
  strcpy(tilename[15],"E060N40.DEM\0");
  tilelat[15][0] = 40.0;  tilelon[15][0] = 060.0;
  tilelat[15][1] = 40.0;  tilelon[15][1] = 100.0;
  tilelat[15][2] =-10.0;  tilelon[15][2] = 100.0;
  tilelat[15][3] =-10.0;  tilelon[15][3] = 060.0;
  tilelat[15][4] = 40.0;  tilelon[15][4] = 060.0;
  strcpy(tilename[16],"E100N40.DEM\0");
  tilelat[16][0] = 40.0;  tilelon[16][0] = 100.0;
  tilelat[16][1] = 40.0;  tilelon[16][1] = 140.0;
  tilelat[16][2] =-10.0;  tilelon[16][2] = 140.0;
  tilelat[16][3] =-10.0;  tilelon[16][3] = 100.0;
  tilelat[16][4] = 40.0;  tilelon[16][4] = 100.0;
  strcpy(tilename[17],"E140N40.DEM\0");
  tilelat[17][0] = 40.0;  tilelon[17][0] = 140.0;
  tilelat[17][1] = 40.0;  tilelon[17][1] = 180.0;
  tilelat[17][2] =-10.0;  tilelon[17][2] = 180.0;
  tilelat[17][3] =-10.0;  tilelon[17][3] = 140.0;
  tilelat[17][4] = 40.0;  tilelon[17][4] = 140.0;
  strcpy(tilename[18],"W180S10.DEM\0");
  tilelat[18][0] =-10.0;  tilelon[18][0] =-180.0;
  tilelat[18][1] =-10.0;  tilelon[18][1] =-140.0;
  tilelat[18][2] =-60.0;  tilelon[18][2] =-140.0;
  tilelat[18][3] =-60.0;  tilelon[18][3] =-180.0;
  tilelat[18][4] =-10.0;  tilelon[18][4] =-180.0;
  strcpy(tilename[19],"W140S10.DEM\0");
  tilelat[19][0] =-10.0;  tilelon[19][0] =-140.0;
  tilelat[19][1] =-10.0;  tilelon[19][1] =-100.0;
  tilelat[19][2] =-60.0;  tilelon[19][2] =-100.0;
  tilelat[19][3] =-60.0;  tilelon[19][3] =-140.0;
  tilelat[19][4] =-10.0;  tilelon[19][4] =-140.0;
  strcpy(tilename[20],"W100S10.DEM\0");
  tilelat[20][0] =-10.0;  tilelon[20][0] =-100.0;
  tilelat[20][1] =-10.0;  tilelon[20][1] =-060.0;
  tilelat[20][2] =-60.0;  tilelon[20][2] =-060.0;
  tilelat[20][3] =-60.0;  tilelon[20][3] =-100.0;
  tilelat[20][4] =-10.0;  tilelon[20][4] =-100.0;
  strcpy(tilename[21],"W060S10.DEM\0");
  tilelat[21][0] =-10.0;  tilelon[21][0] =-060.0;
  tilelat[21][1] =-10.0;  tilelon[21][1] =-020.0;
  tilelat[21][2] =-60.0;  tilelon[21][2] =-020.0;
  tilelat[21][3] =-60.0;  tilelon[21][3] =-060.0;
  tilelat[21][4] =-10.0;  tilelon[21][4] =-060.0;
  strcpy(tilename[22],"W020S10.DEM\0");
  tilelat[22][0] =-10.0;  tilelon[22][0] =-020.0;
  tilelat[22][1] =-10.0;  tilelon[22][1] = 020.0;
  tilelat[22][2] =-60.0;  tilelon[22][2] = 020.0;
  tilelat[22][3] =-60.0;  tilelon[22][3] =-020.0;
  tilelat[22][4] =-10.0;  tilelon[22][4] =-020.0;
  strcpy(tilename[23],"W020S10.DEM\0");
  tilelat[23][0] =-10.0;  tilelon[23][0] = 020.0;
  tilelat[23][1] =-10.0;  tilelon[23][1] = 060.0;
  tilelat[23][2] =-60.0;  tilelon[23][2] = 060.0;
  tilelat[23][3] =-60.0;  tilelon[23][3] = 020.0;
  tilelat[23][4] =-10.0;  tilelon[23][4] = 020.0;
  strcpy(tilename[24],"W060S10.DEM\0");
  tilelat[24][0] =-10.0;  tilelon[24][0] = 060.0;
  tilelat[24][1] =-10.0;  tilelon[24][1] = 100.0;
  tilelat[24][2] =-60.0;  tilelon[24][2] = 100.0;
  tilelat[24][3] =-60.0;  tilelon[24][3] = 060.0;
  tilelat[24][4] =-10.0;  tilelon[24][4] = 060.0;
  strcpy(tilename[25],"W100S10.DEM\0");
  tilelat[25][0] =-10.0;  tilelon[25][0] = 100.0;
  tilelat[25][1] =-10.0;  tilelon[25][1] = 140.0;
  tilelat[25][2] =-60.0;  tilelon[25][2] = 140.0;
  tilelat[25][3] =-60.0;  tilelon[25][3] = 100.0;
  tilelat[25][4] =-10.0;  tilelon[25][4] = 100.0;
  strcpy(tilename[26],"E140S10.DEM\0");
  tilelat[26][0] =-10.0;  tilelon[26][0] = 140.0;
  tilelat[26][1] =-10.0;  tilelon[26][1] = 180.0;
  tilelat[26][2] =-60.0;  tilelon[26][2] = 180.0;
  tilelat[26][3] =-60.0;  tilelon[26][3] = 140.0;
  tilelat[26][4] =-10.0;  tilelon[26][4] = 140.0;
  strcpy(tilename[27],"W180S60.DEM\0");
  tilelat[27][0] =-60.0;  tilelon[27][0] =-180.0;
  tilelat[27][1] =-60.0;  tilelon[27][1] =-120.0;
  tilelat[27][2] =-90.0;  tilelon[27][2] =-120.0;
  tilelat[27][3] =-90.0;  tilelon[27][3] =-180.0;
  tilelat[27][4] =-60.0;  tilelon[27][4] =-180.0;
  strcpy(tilename[28],"W120S60.DEM\0");
  tilelat[28][0] =-60.0;  tilelon[28][0] =-120.0;
  tilelat[28][1] =-60.0;  tilelon[28][1] =-060.0;
  tilelat[28][2] =-90.0;  tilelon[28][2] =-060.0;
  tilelat[28][3] =-90.0;  tilelon[28][3] =-120.0;
  tilelat[28][4] =-60.0;  tilelon[28][4] =-120.0;
  strcpy(tilename[29],"W060S60.DEM\0");
  tilelat[29][0] =-60.0;  tilelon[29][0] =-060.0;
  tilelat[29][1] =-60.0;  tilelon[29][1] = 000.0;
  tilelat[29][2] =-90.0;  tilelon[29][2] = 000.0;
  tilelat[29][3] =-90.0;  tilelon[29][3] =-060.0;
  tilelat[29][4] =-60.0;  tilelon[29][4] =-060.0;
  strcpy(tilename[30],"W000S60.DEM\0");
  tilelat[30][0] =-60.0;  tilelon[30][0] = 000.0;
  tilelat[30][1] =-60.0;  tilelon[30][1] = 060.0;
  tilelat[30][2] =-90.0;  tilelon[30][2] = 060.0;
  tilelat[30][3] =-90.0;  tilelon[30][3] = 000.0;
  tilelat[30][4] =-60.0;  tilelon[30][4] = 000.0;
  strcpy(tilename[31],"E060S60.DEM\0");
  tilelat[31][0] =-60.0;  tilelon[31][0] = 060.0;
  tilelat[31][1] =-60.0;  tilelon[31][1] = 120.0;
  tilelat[31][2] =-90.0;  tilelon[31][2] = 120.0;
  tilelat[31][3] =-90.0;  tilelon[31][3] = 060.0;
  tilelat[31][4] =-60.0;  tilelon[31][4] = 060.0;
  strcpy(tilename[32],"E120S60.DEM\0");
  tilelat[32][0] =-60.0;  tilelon[32][0] = 120.0;
  tilelat[32][1] =-60.0;  tilelon[32][1] = 180.0;
  tilelat[32][2] =-90.0;  tilelon[32][2] = 180.0;
  tilelat[32][3] =-90.0;  tilelon[32][3] = 120.0;
  tilelat[32][4] =-60.0;  tilelon[32][4] = 120.0;

  // Loop over all tiles
  if (lat<=-89.9) return(2772.0); // Special case for bottom of GTOPO30 grids
  for (i=0;i<33;i++)
  {

    // If point is within current tile, query that tile
    flag = pointinpolygon(lon,lat,tilelon[i],tilelat[i],5);
    //printf("flag=%d\n",flag);
    if (flag) 
    {
      //printf("%s\n",tilename[i]);
      nlon = int(120.0*(tilelon[i][1]-tilelon[i][0]));
      nlat = int(120.0*(tilelat[i][1]-tilelat[i][2]));
      mdbl = 120.0*(lon-tilelon[i][0])-0.5;
      if (mdbl<0.0)
      {
        m1 = 0;
        m2 = 0;
      }
      else if (mdbl>(nlon-1)) 
      {
        m1 = nlon-1;
        m2 = nlon-1;
      }
      else
      {
        m1 = int(mdbl);
        m2 = m1+1;
      }
      ndbl = 120.0*(tilelat[i][0]-lat)-0.5;
      if (ndbl<0.0)
      {
        n1 = 0;
        n2 = 0;
      }
      else if (ndbl>(nlat-1)) 
      {
        n1 = nlat-1;
        n2 = nlat-1;
      }
      else
      {
        n1 = int(ndbl);
        n2 = n1+1;
      }
      offsetq11 = 2*(n1*nlon+m1);
      offsetq21 = 2*(n1*nlon+m2);
      offsetq12 = 2*(n2*nlon+m1);
      offsetq22 = 2*(n2*nlon+m2);
      //printf("ncols: %d  nrows: %d\n",nlon,nlat);
      //printf("double col# %lf  double row# %lf\n",mdbl,ndbl);
      //printf("m1: %d  m2:%d\n",m1,m2);
      //printf("n1: %d  n2: %d\n",n1,n2);
      //printf("offsetq11=%ld  offsetq21=%ld\n",offsetq11,offsetq21);
      //printf("offsetq12=%ld  offsetq22=%ld\n",offsetq12,offsetq22);

      // Open this GTOPO30 DEM tile if not already open
      strcpy(filename,GTOPO30PATH);
      strcat(filename,tilename[i]);
      if (opendem==-1)
      {
        //printf("no DEM open, opening GTOPO30 DEM tile %d\n",i);
        if ((fptrdem=fopen(filename,"r"))==NULL) return(-9999.9);
        opendem = i;
      }
      else if (opendem!=i)
      {
        //printf("closing previously open DEM, opening GTOPO30 DEM tile %d\n",i);
        fclose(fptrdem);
        if ((fptrdem=fopen(filename,"r"))==NULL) return(-9999.9);
        opendem = i;    
      }

      // Read the four surrounding pixels from the DEM file
      fseek(fptrdem,offsetq11,SEEK_SET);
      fread(&sdtemp,2,1,fptrdem);
      byteswap((char *)&sdtemp,(char *)&q11,2);
      if (q11==-9999.0) q11 = 0.0;
      fseek(fptrdem,offsetq21,SEEK_SET);
      fread(&sdtemp,2,1,fptrdem);
      byteswap((char *)&sdtemp,(char *)&q21,2);
      if (q21==-9999.0) q21 = 0.0;
      fseek(fptrdem,offsetq12,SEEK_SET);
      fread(&sdtemp,2,1,fptrdem);
      byteswap((char *)&sdtemp,(char *)&q12,2);
      if (q12==-9999.0) q12 = 0.0;
      fseek(fptrdem,offsetq22,SEEK_SET);
      fread(&sdtemp,2,1,fptrdem);
      byteswap((char *)&sdtemp,(char *)&q22,2);
      if (q22==-9999.0) q22 = 0.0;
      //printf("q11 = %d  q21 = %d\n",q11,q21);
      //printf("q12 = %d  q22 = %d\n",q12,q22);
      
      // Compute terrain at requested lat/lon by bilinear interpolation
      if (q11==-9999.0||q21==-9999.0||q12==-9999.0||q22==-9999.0)
        p = -9999.0;
      else
      {
        //printf("point b\n");
        lon1 = tilelon[i][0]+(m1+0.5)/120.0;
        lon2 = tilelon[i][0]+(m2+0.5)/120.0;
        lat1 = tilelat[i][0]-(n1+0.5)/120.0;
        lat2 = tilelat[i][0]-(n2+0.5)/120.0;
        //printf("lon1 = %lf  lon2 = %lf\n",lon1,lon2);
        //printf("lat1 = %lf  lat2 = %lf\n",lat1,lat2);
        if (lat1==lat2 && lon1==lon2)  // at a corner of a tile
        {
          //printf("at corner of tile\n");
          p = q11;
        }
        else if (lat1==lat2) // at top or bottom edge of a tile
        {
          //printf("at top or bottom edge of tile\n");
          p = q11+(q21-q11)*(lon-lon1)/(lon2-lon1);
        }
        else if (lon1==lon2) // at right or left edge of a tile
        {
          //printf("at right or left edge of tile\n");
          p = q11+(q12-q11)*(lat-lat1)/(lat2-lat1);
        }
        else // within a tile, the usual case
        {
          denom = (lon2-lon1)*(lat2-lat1);
          f1 = ((lon2-lon)*(lat2-lat))/denom;
          f2 = ((lon-lon1)*(lat2-lat))/denom;
          f3 = ((lon2-lon)*(lat-lat1))/denom;
          f4 = ((lon-lon1)*(lat-lat1))/denom;
          //printf("f1=%lf f2=%lf f3=%lf f4=%lf\n",f1,f2,f3,f4);
          p = f1*q11 + f2*q21 + f3*q12 + f4*q22;
        }
      }
      return(p);

    }

  }

}


double querygeoid(double lat, double lon, char *geoidid)
{
  double geoid;
  double queryegm96(double, double);
  double queryegm2008(double, double);
  
  // EGM96 is our only available geoid currently
  //if (1)
  //{
  //  strcpy(geoidid,"E96\0");
  //  geoid = queryegm96(lat,lon);
  //}

  // Query the EGM2008 1'x1' geoid grid
  if (1)
  {
    strcpy(geoidid,"E08\0");
    geoid = queryegm2008(lat,lon);
  }

}



double queryegm2008(double y,double x)
{
  int nx,nxtg,ny,m1,m2,n1,n2;
  long long int offsetq11,offsetq12,offsetq21,offsetq22;
  float ftemp,q11,q12,q21,q22;
  double x0,y0,res,mdbl,ndbl,lat1,lon1,lat2,lon2,p,denom,f1,f2,f3,f4;

  // The EGM2008 geoid grid files are odd, and poorly documented.  After lots of trial
  // and error I determined that the grid dimensions are 10801 rows by 21602 columns.
  // The first and last columns are filled with 0s, padding I suppose.

  // Offset longitude to between 0 and 360
  while (x<0.0) x+=360.0;

  // Define size of grid
  x0 = 0.0;
  y0 = 90.0;
  nx = 21602;
  ny = 10801;
  res = 1.0/60.0;

  // Determine row and column of surrounding grid cells
  // for now, we ignore the padding columns at left and right
  nxtg = nx-2;
  mdbl = (x-x0)/res;
  //printf("mdbl=%lf nxtg=%d\n",mdbl,nxtg);
  if (mdbl<0.0)
  {
    m1 = 0;
    m2 = 0;
  }
  else if (mdbl>(nxtg-1))
  {
    m1 = nxtg-1;
    m2 = 1;
  }
  else
  {
    m1 = int(mdbl);
    m2 = m1+1;
  }
  ndbl = (y0-y)/res;
  if (ndbl<0.0)
  {
    n1 = 0;
    n2 = 0;
  }
  else if (ndbl>(ny-1)) 
  {
    n1 = ny-1;
    n2 = ny-1;
  }
  else
  {
    n1 = int(ndbl);
    n2 = n1+1;
  }
  m1 += 1; // add the padding column
  m2 += 1; // add the padding column
  offsetq11 = 4*(n1*nx+m1);
  offsetq21 = 4*(n1*nx+m2);
  offsetq12 = 4*(n2*nx+m1);
  offsetq22 = 4*(n2*nx+m2);
  //printf("ncols: %d  nrows: %d\n",nx,ny);
  //printf("double col# %lf  double row# %lf\n",mdbl,ndbl);
  //printf("m1: %d  m2:%d\n",m1,m2);
  //printf("n1: %d  n2: %d\n",n1,n2);
  //printf("offsetq11=%lld  offsetq21=%lld\n",offsetq11,offsetq21);
  //printf("offsetq12=%lld  offsetq22=%lld\n",offsetq12,offsetq22);

  // Open EGM2008 geoid file if not already open
  if (opengeoid==-1)
  {
    if ((fptrgeoid=fopen(EGM08PATH,"r"))==NULL) return(-9999.9);
    opengeoid = 0;
  }

  // Read the four surrounding pixels from the geoid file
  fseek(fptrgeoid,offsetq11,SEEK_SET);
  fread(&q11,4,1,fptrgeoid);
  fseek(fptrgeoid,offsetq21,SEEK_SET);
  fread(&q21,4,1,fptrgeoid);
  fseek(fptrgeoid,offsetq12,SEEK_SET);
  fread(&q12,4,1,fptrgeoid);
  fseek(fptrgeoid,offsetq22,SEEK_SET);
  fread(&q22,4,1,fptrgeoid);
  //printf("q11 = %f  q21 = %f\n",q11,q21);
  //printf("q12 = %f  q22 = %f\n",q12,q22);

  // Compute geoid at requested lat/lon by bilinear interpolation
  if (q11==-9999.0||q21==-9999.0||q12==-9999.0||q22==-9999.0)
    p = -9999.0;
  else
  {
    lon1 = x0+(m1-1)*res;
    lon2 = x0+(m2-1)*res;
    lat1 = y0-n1*res;
    lat2 = y0-n2*res;
    denom = (lon2-lon1)*(lat2-lat1);
    f1 = ((lon2-x)*(lat2-y))/denom;
    f2 = ((x-lon1)*(lat2-y))/denom;
    f3 = ((lon2-x)*(y-lat1))/denom;
    f4 = ((x-lon1)*(y-lat1))/denom;
    p = f1*q11 + f2*q21 + f3*q12 + f4*q22;
  }

  // Return the result
  return(p);

}



double queryegm96(double y,double x)
{
  short sdtemp,q11,q21,q12,q22;
  int nx,ny,mdbl,ndbl,m1,m2,n1,n2;
  double x0,y0,lon1,lon2,lat1,lat2,denom,f1,f2,f3,f4,p;
  long int offsetq11,offsetq21,offsetq12,offsetq22;
  void byteswap(char *,char *,int);

  // Offset longitude to between 0 and 360
  while (x<0.0) x+=360.0;

  // Define size of grid
  x0 = 0.0;
  y0 = 90.0;
  nx = 1440;
  ny = 721;

  // Determine row and column of surrounding grid cells
  mdbl = 4.0*(x-x0);
  if (mdbl<0.0)
  {
    m1 = 0;
    m2 = 0;
  }
  else if (mdbl>(nx-1)) 
  {
    m1 = nx-1;
    m2 = nx-1;
  }
  else
  {
    m1 = int(mdbl);
    m2 = m1+1;
  }
  ndbl = 4.0*(y0-y);
  if (ndbl<0.0)
  {
    n1 = 0;
    n2 = 0;
  }
  else if (ndbl>(ny-1)) 
  {
    n1 = ny-1;
    n2 = ny-1;
  }
  else
  {
    n1 = int(ndbl);
    n2 = n1+1;
  }
  offsetq11 = 2*(n1*nx+m1);
  offsetq21 = 2*(n1*nx+m2);
  offsetq12 = 2*(n2*nx+m1);
  offsetq22 = 2*(n2*nx+m2);
  //printf("ncols: %d  nrows: %d\n",nx,ny);
  //printf("double col# %lf  double row# %lf\n",mdbl,ndbl);
  //printf("m1: %d  m2:%d\n",m1,m2);
  //printf("n1: %d  n2: %d\n",n1,n2);
  //printf("offsetq11=%ld  offsetq21=%ld\n",offsetq11,offsetq21);
  //printf("offsetq12=%ld  offsetq22=%ld\n",offsetq12,offsetq22);

  // Open EGM96 geoid file if not already open
  if (opengeoid==-1)
  {
    if ((fptrgeoid=fopen(EGM96PATH,"r"))==NULL) return(-9999.9);
    opengeoid = 0;
  }

  // Read the four surrounding pixels from the geoid file
  fseek(fptrgeoid,offsetq11,SEEK_SET);
  fread(&sdtemp,2,1,fptrgeoid);
  byteswap((char *)&sdtemp,(char *)&q11,2);
  if (q11==-9999.0) q11 = 0.0;
  fseek(fptrgeoid,offsetq21,SEEK_SET);
  fread(&sdtemp,2,1,fptrgeoid);
  byteswap((char *)&sdtemp,(char *)&q21,2);
  if (q21==-9999.0) q21 = 0.0;
  fseek(fptrgeoid,offsetq12,SEEK_SET);
  fread(&sdtemp,2,1,fptrgeoid);
  byteswap((char *)&sdtemp,(char *)&q12,2);
  if (q12==-9999.0) q12 = 0.0;
  fseek(fptrgeoid,offsetq22,SEEK_SET);
  fread(&sdtemp,2,1,fptrgeoid);
  byteswap((char *)&sdtemp,(char *)&q22,2);
  if (q22==-9999.0) q22 = 0.0;
  //printf("q11 = %d  q21 = %d\n",q11,q21);
  //printf("q12 = %d  q22 = %d\n",q12,q22);

  // Compute geoid at requested lat/lon by bilinear interpolation
  lon1 = x0+m1/4.0;
  lon2 = x0+m2/4.0;
  lat1 = y0-n1/4.0;
  lat2 = y0-n2/4.0;
  //printf("lon1 = %lf  lon2 = %lf\n",lon1,lon2);
  //printf("lat1 = %lf  lat2 = %lf\n",lat1,lat2);
  denom = (lon2-lon1)*(lat2-lat1);
  f1 = ((lon2-x)*(lat2-y))/denom;
  f2 = ((x-lon1)*(lat2-y))/denom;
  f3 = ((lon2-x)*(y-lat1))/denom;
  f4 = ((x-lon1)*(y-lat1))/denom;
  p = f1*q11 + f2*q21 + f3*q12 + f4*q22;
  return(p/100.0); // heights in database are in cm

}


bool pointinpolygon(double x, double y,double xpoly[],double ypoly[],int npoly)
{
  int i,j=npoly-2;
  bool oddnodes=false;

  for (i=0; i<(npoly-1); i++) 
  {
    if (ypoly[i]<y && ypoly[j]>=y || ypoly[j]<y && ypoly[i]>=y) 
    {
      if (xpoly[i]+(y-ypoly[i])/(ypoly[j]-ypoly[i])*(xpoly[j]-xpoly[i])<x) 
      {
        oddnodes=!oddnodes; 
      }
    }
    j=i; 
  }

  return(oddnodes);

}


void byteswap(char *in,char *out,int len)
{
  int i,len2;

  len2 = len-1;
  for (i=0;i<len;i++)
  {
    out[i]=in[len2-i];
    //out[i] = in[i];
  }

}

