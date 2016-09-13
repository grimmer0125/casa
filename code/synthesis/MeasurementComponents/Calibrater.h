//# Calibrater.h: High-level calibrator object; controls calibration overall
//# Copyright (C) 1996,1997,1998,1999,2000,2001,2002,2003
//# Associated Universities, Inc. Washington DC, USA.
//#
//# This library is free software; you can redistribute it and/or modify it
//# under the terms of the GNU Library General Public License as published by
//# the Free Software Foundation; either version 2 of the License, or (at your
//# option) any later version.
//#
//# This library is distributed in the hope that it will be useful, but WITHOUT
//# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
//# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
//# License for more details.
//#
//# You should have received a copy of the GNU Library General Public License
//# along with this library; if not, write to the Free Software Foundation,
//# Inc., 675 Massachusetts Ave, Cambridge, MA 02139, USA.
//#
//# Correspondence concerning AIPS++ should be addressed as follows:
//#        Internet email: aips2-request@nrao.edu.
//#        Postal address: AIPS++ Project Office
//#                        National Radio Astronomy Observatory
//#                        520 Edgemont Road
//#
//# $Id: Calibrater.h,v 19.16 2006/02/14 19:46:23 gmoellen Exp $

#ifndef SYNTHESIS_CALIBRATER_H
#define SYNTHESIS_CALIBRATER_H

#include <casa/aips.h>
#include <casa/OS/Timer.h>
#include <casa/Containers/Record.h>
#include <ms/MeasurementSets/MeasurementSet.h>
#include <measures/Measures/MRadialVelocity.h>
#include <synthesis/MeasurementEquations/VisEquation.h>
#include <synthesis/MeasurementComponents/VisCal.h>
#include <synthesis/MeasurementComponents/SolvableVisCal.h>
#include <synthesis/MeasurementComponents/VisCalGlobals.h>
#include <casa/Logging/LogIO.h>
#include <casa/Logging/LogSink.h>
#include <ms/MeasurementSets/MSHistoryHandler.h>
#include <ms/MSSel/MSSelection.h>
#include <msvis/MSVis/VisibilityProcessing.h>
#include <msvis/MSVis/ViFrequencySelection.h>


namespace casa { //# NAMESPACE CASA - BEGIN

// <summary>Controls the solution of calibration components (Jones Matrices)</summary>

class Calibrater 
{

 public:
  // Default constructor
  Calibrater();


  // Simple casacore::MS-only ctor
  Calibrater(casacore::String msname);

  // Copy constructor and assignment operator
  Calibrater(const Calibrater&);
  Calibrater& operator=(const Calibrater&);

  // Destructor
  ~Calibrater();
  

  // Set uv-data selection via MSSelection
  void selectvis(const casacore::String& time="",
		 const casacore::String& spw="",
		 const casacore::String& scan="",
		 const casacore::String& field="",
		 const casacore::String& intent="",
		 const casacore::String& obsIDs="",
		 const casacore::String& baseline="",
		 const casacore::String& uvrange="",
		 const casacore::String& chanmode="none",
		 const casacore::Int& nchan=1,
		 const casacore::Int& start=0, 
		 const casacore::Int& step=1,
		 const casacore::MRadialVelocity& mStart=casacore::MRadialVelocity(),
		 const casacore::MRadialVelocity& mStep=casacore::MRadialVelocity(),
		 const casacore::String& msSelect="");

  // Set up to apply calibration (using casacore::MSSelection syntax)
  casacore::Bool setapply (const casacore::String& type, 
		 const casacore::Double& t,
		 const casacore::String& table,
		 const casacore::String& spw,
		 const casacore::String& field,
		 const casacore::String& interp,
		 const casacore::Bool& calwt,
		 const casacore::Vector<casacore::Int>& spwmap,
		 const casacore::Vector<casacore::Double>& opacity);

  casacore::Bool setapply (const casacore::String& type, 
		 const casacore::Record& applypar);

  // Set up apply-able calibration via a Cal Library
  casacore::Bool setcallib(casacore::Record callib);
  casacore::Bool setcallib2(casacore::Record callib);
  casacore::Bool validatecallib(casacore::Record callib);

  casacore::Bool setmodel(const casacore::String& modelImage);
  casacore::Bool setModel(const casacore::Vector<casacore::Double>& stokes);

  // Arrange to solve (using casacore::MSSelection syntax)
  casacore::Bool setsolve (const casacore::String& type, 
		 const casacore::String& solint,
		 const casacore::String& table,
		 const casacore::Bool append,
		 const casacore::Double preavg, 
		 const casacore::String& apmode="AP",
		 const casacore::Int minblperant=4,
		 const casacore::String& refant="",
		 const casacore::Bool solnorm=false,
		 const casacore::Float minsnr=0.0f,
		 const casacore::String& combine="",
		 const casacore::Int fillgaps=0,
		 const casacore::String& cfcache="",
		 const casacore::Double painc=360.0,
                 const casacore::Int fitorder=0,
                 const casacore::Float fraction=0.1,
                 const casacore::Int numedge=-1,
                 const casacore::String& radius="",
                 const casacore::Bool smooth=true);

  // Arrange to solve for BPOLY (using casacore::MSSelection syntax)
  casacore::Bool setsolvebandpoly(const casacore::String& table,
			const casacore::Bool& append,
			const casacore::String& solint,
			const casacore::String& combine,
			const casacore::Vector<casacore::Int>& degree,
			const casacore::Bool& visnorm,
			const casacore::Bool& solnorm,
			const casacore::Int& maskcenter,
			const casacore::Float& maskedge,
			const casacore::String& refant);

  // Arrange to solve for GSPLINE (using casacore::MSSelection syntax)
  casacore::Bool setsolvegainspline(const casacore::String& table,
			  const casacore::Bool& append,
			  const casacore::String& mode,
			  const casacore::Double& splinetime,
			  const casacore::Double& preavg,
			  const casacore::Int& numpoint,
			  const casacore::Double& phasewrap,
			  const casacore::String& refant);

  casacore::Bool setsolve (const casacore::String& type, 
		 const casacore::Record& solvepar);

  // Unset all (default) or one apply calibration component
  casacore::Bool unsetapply(const casacore::Int& which=-1);
  // TBD:   casacore::Bool unsetapply(const casacore::String& type);  // by type?

  // Unset the solved-for calibration component
  casacore::Bool unsetsolve();

  // Reset the calibrator object
  casacore::Bool reset(const casacore::Bool& apply=true, 
	     const casacore::Bool& solve=true);

  // Apply all setapply'd calibration components to DATA and
  //  deposit in the CORRECTED_DATA column
  casacore::Bool correct(casacore::String mode="calflag");
  casacore::Bool correct2(casacore::String mode="calflag");

  // Apply all setapply'd calibration components to MODEL_DATA and
  //  deposit in the MODEL_DATA column
  casacore::Bool corrupt();

  // Initialize sigma/weight, and possibly weight_spectrum
  casacore::Bool initWeights(casacore::String wtmode, casacore::Bool dowtsp=false);
  casacore::Bool initWeights(casacore::Bool doBT=true, casacore::Bool dowtsp=false);
  casacore::Bool initWeightsWithTsys(casacore::String wtmode, casacore::Bool dowtsp=false, casacore::String tsystable="",
			casacore::String gainfield="", casacore::String interp="linear", casacore::Vector<casacore::Int> spwmap=casacore::Vector<casacore::Int>());

  // Solve for a given calibration component
  casacore::Bool solve();

  // Modelfit
  casacore::Vector<casacore::Double> modelfit(const casacore::Int& iter,
			  const casacore::String& stype,
			  const casacore::Vector<casacore::Double>& par,
			  const casacore::Vector<casacore::Bool>& vary,
			  const casacore::String& file);

  // Fluxscale (using casacore::MSSelection syntax for fields)
  void fluxscale(const casacore::String& infile, 
		 const casacore::String& outfile,
		 const casacore::String& refFields, 
		 const casacore::Vector<casacore::Int>& refSpwMap,
		 const casacore::String& tranFields,
		 const casacore::Bool& append,
                 const casacore::Float& inGainThres,
                 const casacore::String& antSel,
                 const casacore::String& timerangeSel,
                 const casacore::String& scanSel,
		 SolvableVisCal::fluxScaleStruct& oFluxScaleFactor,
		 casacore::Vector<casacore::Int>& tranidx,
		 const casacore::String& oListFile,
                 const casacore::Bool& incremental,
                 const casacore::Int& fitorder,
                 const casacore::Bool& display);

  // Fluxscale (via field indices)
  void fluxscale(const casacore::String& infile, 
		 const casacore::String& outfile,
		 const casacore::Vector<casacore::Int>& refField, 
		 const casacore::Vector<casacore::Int>& refSpwMap,
		 const casacore::Vector<casacore::Int>& tranField,
		 const casacore::Bool& append,
                 const casacore::Float& inGainThres,
                 const casacore::String& antSel,
                 const casacore::String& timerangeSel,
                 const casacore::String& scanSel,
		 SolvableVisCal::fluxScaleStruct& oFluxScaleFactor,
		 const casacore::String& oListFile,
		 const casacore::Bool& incremental,
                 const casacore::Int& fitorder,
                 const casacore::Bool& display);

  // Accumulate (using casacore::MSSelection syntax)
  void accumulate(const casacore::String& intab,
		  const casacore::String& incrtab,
		  const casacore::String& outtab,
		  const casacore::String& fields,
		  const casacore::String& calFields,
		  const casacore::String& interp="linear",
		  const casacore::Double& t=-1.0,
		  const casacore::Vector<casacore::Int>& spwmap=casacore::Vector<casacore::Int>(1,-1));

  // Generate cal table from specified values
  void specifycal(const casacore::String& type,
		  const casacore::String& caltable,
		  const casacore::String& time,
		  const casacore::String& spw,
		  const casacore::String& antenna,
		  const casacore::String& pol,
		  const casacore::Vector<casacore::Double>& parameter,
		  const casacore::String& infile);

  // casacore::Smooth  calibration (using casacore::MSSelection syntax
  casacore::Bool smooth(const casacore::String& infile,
              casacore::String& outfile, 
	      const casacore::String& smoothtype, 
	      const casacore::Double& smoothtime,
	      const casacore::String& fields);


  // casacore::List a calibration table
  casacore::Bool listCal(const casacore::String& infile,
	       const casacore::String& field,
	       const casacore::String& antenna,
	       const casacore::String& spw,
	       const casacore::String& listfile="",
	       const casacore::Int& pagerows=50);

  // Initialize the calibrator object from an input MeasurementSet.
  // Optional compression of the calibration columns (MODEL_DATA,
  // and CORRECTED_DATA) is supported.
  //if addScratch=true, optionally one can just add CORRECTED_DATA by
  //setting addModel to false
  casacore::Bool initialize(casacore::MeasurementSet& inputMS, 
		  casacore::Bool compress=true,
		  casacore::Bool addScratch=true, casacore::Bool addModel=true);

  // Re-initialize the calibration scratch columns
  casacore::Bool initCalSet(const casacore::Int& calSet);

  // Report apply/solve state
  casacore::Bool state();
  casacore::Bool applystate();
  casacore::Bool solvestate();

  casacore::Record& getActRec() {return actRec_;};

  casacore::Bool cleanup();

  // Handle caltable backward compatibility
  static casacore::Bool updateCalTable(const casacore::String& caltable);

  // Return access to the VisEquation
  VisEquation* ve() { return ve_p; };
  
 private:
  // Log functions and variables
  casacore::LogIO sink_p;
  casacore::LogIO& logSink();

  // Method to update casacore::MS HISTORY Table
  void writeHistory(casacore::LogIO& os, 
		    casacore::Bool cliCommand=false);

  // casacore::Time functions and variables
  casacore::String timerString();
  casacore::Timer timer_p;

  VisibilityIterator::DataColumn configureForCorrection ();

  // Select on channel using MSSelection
  void selectChannel(const casacore::String& spw);

  // Channel mask services
  void initChanMask();

  // Select on channel in the VisSet
  void selectChannel(const casacore::String& mode, 
		     const casacore::Int& nchan, const casacore::Int& start, const casacore::Int& step,
		     const casacore::MRadialVelocity& mStart,
		     const casacore::MRadialVelocity& mStep);
  
  // Interpret refant index
  casacore::Vector<casacore::Int> getRefantIdxList(const casacore::String& refant);
  
  casacore::Vector<casacore::Int> getAntIdx(const casacore::String& antenna);

  // Interpret field indices (casacore::MSSelection)
  casacore::Vector<casacore::Int> getFieldIdx(const casacore::String& fields);

  // Interpret spw indices (casacore::MSSelection)
  casacore::Vector<casacore::Int> getSpwIdx(const casacore::String& spws);

  // Interpret spw indices (casacore::MSSelection)
  casacore::Matrix<casacore::Int> getChanIdx(const casacore::String& spws);
  
  // Query apply types to see if we need to calibrate the weights
  casacore::Bool calWt();

  // Returns true if calibrator object is in a valid state
  casacore::Bool ok();

  // Given a (supplied) list of uncalibrated spws, determines and returns if there were
  // any, and if so sends them as a warning message to the logger.
  casacore::Bool summarize_uncalspws(const casacore::Vector<casacore::Bool>& uncalspw, const casacore::String& origin,
			   casacore::Bool strictflag=false);

  // Create a VisSet for raw phase transfer if needed
  void getRawPhaseVisSet(casacore::Vector<casacore::Int>& spwid); 

  // The standard solving mechanism
  casacore::Bool genericGatherAndSolve();

  // casacore::Input casacore::MeasurementSet and derived selected MeasurementSet
  casacore::String msname_p;
  casacore::MeasurementSet* ms_p;
  casacore::MeasurementSet* mssel_p;
  casacore::MeasurementSet* msselRaw_p;

  // casacore::MSSelection object
  casacore::MSSelection* mss_p;
  vi::FrequencySelections* frequencySelections_p;

  // VisSet and VisEquation derived from the input MeasurementSet
  VisSet* vs_p;
  VisSet* rawvs_p;
  VisEquation* ve_p;

  // VisCals for applying and solving:
  casacore::PtrBlock<VisCal*> vc_p;
  SolvableVisCal* svc_p;

  // casacore::MeasurementSet selection parameters
  casacore::String dataMode_p;
  casacore::Int dataNchan_p, dataStart_p, dataStep_p;
  casacore::MRadialVelocity mDataStart_p, mDataStep_p;

  //Spline phase wrapping helper params
  casacore::Double phasewrap_p;
  casacore::Int splinepoint_p;

  //Used to update the casacore::MS HISTORY Table
  casacore::Int histLockCounter_p;
  casacore::MSHistoryHandler *hist_p;
  casacore::Table historytab_p;

  // channel masking 
  casacore::PtrBlock<casacore::Vector<casacore::Bool>*> chanmask_;

  // Activity record
  casacore::Record actRec_;

};

} //# NAMESPACE CASA - END

#endif
