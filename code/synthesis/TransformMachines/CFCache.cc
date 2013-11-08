// -*- C++ -*-
//# CFCache.cc: Implementation of the CFCache class
//# Copyright (C) 1997,1998,1999,2000,2001,2002,2003
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
//#                        Charlottesville, VA 22903-2475 USA
//#
//# $Id$
#include <synthesis/TransformMachines/SynthesisError.h>
#include <synthesis/TransformMachines/CFCache.h>
#include <synthesis/TransformMachines/Utils.h>
#include <lattices/Lattices/LatticeExpr.h>
#include <casa/Exceptions/Error.h>
#include <fstream>
#include <algorithm>
// #include <tables/Tables/TableDesc.h>
// #include <tables/Tables/SetupNewTab.h>
// #include <tables/Tables/Table.h>

namespace casa{
  CFCache::~CFCache()  {}
  //
  //-------------------------------------------------------------------------
  // Load just the axillary info. if found.  The convolution functions
  // are loaded on-demand.
  //
  void CFCache::initCache()
  {
    LogOrigin logOrigin("CFCache", "initCache");
    LogIO log_l(logOrigin);

    ostringstream name;
    String line;
    Directory dirObj(Dir);

    if (Dir.length() == 0) 
      throw(SynthesisFTMachineError(LogMessage("Got null string for disk cache dir. ",
					       logOrigin).message()));
    //
    // If the directory does not exist, create it
    //
    if (!dirObj.exists()) dirObj.create();
    else if ((!dirObj.isWritable()) || (!dirObj.isReadable()))
      {
	throw(SynthesisFTMachineError(String("Directory \"")+Dir+String("\"")+
				      String(" for convolution function cache"
					     " exists but is unreadable/unwriteable")));
      }

    try
      {
	name << Dir << "/" << aux;
	File file(name);
	Int Npa=0,Nw=0;
	ifstream aux;
	Bool readFromFile=False;
	if (file.exists() && file.isRegular()) 
	  {
	    readFromFile=True;
	    aux.open(name.str().c_str());
	    if (readFromFile && aux.good()) aux >> Npa >> Nw;
	    else
	      throw(SynthesisFTMachineError(String("Error while reading convolution "
						   "function cache file ")+name));
	  }

	if (Npa > 0)
	  {
	    paList.resize(Npa,True);

	    IPosition s(2,Nw,Npa);
	    XSup.resize(s,True);
	    YSup.resize(s,True);
	    Sampling.resize(Npa,True);
	    for(Int i=0;i<Npa;i++)
	      {
		Float pa, S;
		Int XS, YS;
		s[2]=i;
		aux >> pa;
		for(Int iw=0;iw<Nw;iw++)
		  {
		    s[0]=iw;
		    aux >> XS >> YS;
		    YS = XS;
		    paList[i] = pa*M_PI/180.0;
		    XSup(iw,i)=XS;
		    YSup(iw,i)=YS;
		  }
		aux >> S;
		Sampling[i]=S;
	      }
	  }
      }
    catch(AipsError& x)
      {
	throw(SynthesisFTMachineError(String("Error while initializing CF disk cache: ")
				      +x.getMesg()));
      }
  }
  //
  //-----------------------------------------------------------------------
  //
  void CFCache::initPolMaps(PolMapType& polMap, PolMapType& conjPolMap)
  {
    if (OTODone()==False)
      {
	for(Int i=0;i<(Int)memCache2_p.nelements();i++)
	  memCache2_p[i].initPolMaps(polMap, conjPolMap);
	for(Int i=0;i<(Int)memCacheWt2_p.nelements();i++)
	  memCacheWt2_p[i].initPolMaps(polMap, conjPolMap);
	OTODone_p=True;
      }
  }
  //
  //-----------------------------------------------------------------------
  //
  void CFCache::initCache2()
  {
    LogOrigin logOrigin("CFCache", "initCache2");
    LogIO log_l(logOrigin);

    Directory dirObj(Dir);

    if (Dir.length() == 0) 
      throw(SynthesisFTMachineError(LogMessage("Got null string for disk cache dir. ",
					       logOrigin).message()));
    //
    // If the directory does not exist, create it
    //
    if (!dirObj.exists()) dirObj.create();
    else if ((!dirObj.isWritable()) || (!dirObj.isReadable()))
      {
	throw(SynthesisFTMachineError(String("Directory \"")+Dir+String("\"")+
				      String(" for convolution function cache"
					     " exists but is unreadable/unwriteable")));
      }

    fillCFSFromDisk(dirObj,"CFS*", memCache2_p);
    fillCFSFromDisk(dirObj,"WTCFS*", memCacheWt2_p);
    memCache2_p[0].primeTheCFB();
    memCacheWt2_p[0].primeTheCFB();

    memCache2_p[0].show("CF Cache: ");
    memCacheWt2_p[0].show("WTCF Cache: ");

    Double memUsed0,memUsed1;
    memUsed0=memCache2_p[0].memUsage()/1024;
    memUsed1=memCacheWt2_p[0].memUsage()/1024;
    log_l << "Total CF Cache memory footprint: " << (memUsed0+memUsed1) << " (" << memUsed0 << "," << memUsed1 << ") KB" << LogIO::POST;
  }
  //
  //-----------------------------------------------------------------------
  //
  void CFCache::fillCFSFromDisk(const Directory dirObj, const String& pattern, CFStoreCacheType2& memStore)
  {
    LogOrigin logOrigin("CFCache", "fillCFSFromDisk");
    LogIO log_l(logOrigin);
    try
      {
	if (memStore.nelements() == 0) memStore.resize(1,True);

	CFCacheTableType cfCacheTable_l;
	Regex regex(Regex::fromPattern(pattern));
	Vector<String> fileNames(dirObj.find(regex));
	if (fileNames.nelements() > 0)
	  {
	    String CFCDir=dirObj.path().absoluteName();
	    log_l << "No. of " << pattern << " found in " 
		  << dirObj.path().originalName() << ": " 
		  << fileNames.nelements() << LogIO::POST;

	    //
	    // Gather the list of PA values
	    //
	    for (uInt i=0; i < fileNames.nelements(); i++)
	      {
		PagedImage<Complex> thisCF(CFCDir+'/'+fileNames[i]);
		TableRecord miscinfo = thisCF.miscInfo();
		//	    miscinfo.print(cerr);
		Double  paVal;
                //UNUSED: Double  wVal; Int mVal;
		miscinfo.get("ParallacticAngle",paVal);
		paList_p.push_back(paVal);
	      }
	    //
	    // Make the PA-value list unique
	    //
	    sort( paList_p.begin(), paList_p.end() );
	    paList_p.erase( unique( paList_p.begin(), paList_p.end() ), paList_p.end() );
	    cfCacheTable_l.resize(paList_p.size());
	    
	    // For each CF, load the PA, Muelller element, WValue and the Ref. Freq. 
	    // Insert these values in the lists in the cfCacheTable
	    Array<Complex> pixBuf;

	    for (uInt i=0; i < fileNames.nelements(); i++)
	      {
		Double paVal, wVal, fVal, sampling; Int mVal, xSupport, ySupport;
		CoordinateSystem coordSys;

		getCFParams(fileNames[i], pixBuf, coordSys,  sampling, paVal, 
			    xSupport, ySupport, fVal, wVal, mVal);

		Int ipos; SynthesisUtils::stdNearestValue(paList_p, (Float)paVal,ipos);
		uInt paPos=ipos;
		//		cerr << paPos << " " << paList_p.size() << " " << i << endl;
		  
		if (paPos < paList_p.size())
		  {
		    cfCacheTable_l[paPos].freqList.push_back(fVal);
		    cfCacheTable_l[paPos].wList.push_back(wVal);
		    cfCacheTable_l[paPos].muellerList.push_back(mVal);
		    cfCacheTable_l[paPos].cfNameList.push_back(fileNames[i]);
		  }
	      }
	    for (uInt ipa=0; ipa < cfCacheTable_l.size(); ipa++)
	      {
		//
		// Resize the CFStore (poorly named private variable
		// memCache2_p) to add CFBuffer for the each entry in
		// the paList.
		//
		log_l << "PA: " << paList_p[ipa] << endl;
		vector<String> fileNames(cfCacheTable_l[ipa].cfNameList);
		
		//		if (memStore.nelements() == 0) memStore.resize(1);

		Quantity paQuant(paList_p[ipa],"deg"), dPA(1.0,"deg");
		memStore[0].resize(paQuant, dPA, 0,0);
		CountedPtr<CFBuffer> cfb=memStore[0].getCFBuffer(paQuant, dPA, 0, 0);


		//
		// Get the list of f, w, mVals from cfCacheTable_l for
		// the current ipa index.  Sort them.  And convert
		// them into a list of unique entires.
		//
		vector<Double> fList(cfCacheTable_l[ipa].freqList), 
		  wList(cfCacheTable_l[ipa].wList);
		vector<Int> mList(cfCacheTable_l[ipa].muellerList);
		sort( fList.begin(), fList.end() );
		sort( wList.begin(), wList.end() );
		sort( mList.begin(), mList.end() );
		fList.erase(SynthesisUtils::Unique(fList.begin(), fList.end()), fList.end());
		wList.erase(SynthesisUtils::Unique(wList.begin(), wList.end()), wList.end());
		mList.erase(SynthesisUtils::Unique(mList.begin(), mList.end()), mList.end());
		PolMapType muellerElements;
		Int npol=mList.size();
		muellerElements.resize(npol);
		for (Int ii=0;ii<npol;ii++)
		  {
		    muellerElements[ii].resize(1);
		    muellerElements[ii][0]=mList[ii];
		  }
		cfb->resize(0.0,0.0,wList,fList,
			    muellerElements,muellerElements,muellerElements,muellerElements);

		//
		// Now go over the list of fileNames corresponding to
		// the current PA value and them the current CFBuffer.
		//
		for (uInt nf=0; nf<fileNames.size(); nf++)
		  {
		    Double paVal, wVal, fVal, sampling; 
		    Int mVal, xSupport, ySupport;
		    CoordinateSystem coordSys;
		    //
		    // Get the parameters from the CF file
		    //
		    getCFParams(fileNames[nf], pixBuf, coordSys,  sampling, paVal, 
				xSupport, ySupport, fVal, wVal, mVal);
		    //
		    // Get the storage buffer from the CFBuffer and
		    // fill it in what we got from the getCFParams
		    // call above.
		    //
    		    Array<Complex> &cfBuf=(*(cfb->getCFCellPtr(fVal, wVal,mVal)->storage_p));
		    cfBuf.assign(pixBuf);

		    //
		    // Fill the cfBuf with the pixel array from the
		    // disk file.  Add it, along with the extracted CF
		    // parameters to the CFBuffer.
		    //

		    //cfb->addCF(&cfBuf,coordSys,fsampling,xSupport,ySupport,fVal,wVal,mVal);
		    Int fndx,wndx, mndx;
		    SynthesisUtils::stdNearestValue(fList, fVal, fndx);
		    SynthesisUtils::stdNearestValue(wList, wVal, wndx);
		    SynthesisUtils::stdNearestValue(mList, mVal, mndx);
		    Float fsampling=sampling;
		    //
		    // The coordSys, sampling, xSupport, ySuport
		    // params are set for the CFCell at the location
		    // determined by fndx and wndx.  The mndx is
		    // determined inside using mVal (why this
		    // treatment for mndx, please don't ask.  Not just
		    // yet (SB)).
		    cfb->setParams(fndx, wndx, 0,0, coordSys, fsampling, xSupport, ySupport, 
				   fVal, wVal, mVal);


		    // if (nf==0)
		    //   {
		    // 	log_l << " " << fList.size() << " " << mList.size() << " " << wList.size() << " " 
		    // 	      << cfCacheTable_l[ipa].cfNameList.size()  << LogIO::POST;
		    // 	log_l << "F: " << fList << endl << "M: " << mList << endl << "W: " << wList << endl << LogIO::POST;
		    //   }
		    // log_l << cfCacheTable_l[ipa].cfNameList[nf] << "[" << fndx << "," << wndx << "," << mndx << "]" ;
		  }
		// cfb->show("cfb: ");

		log_l << LogIO::POST;
		// for (uInt inu=0; inu<fList.size(); inu++)
		//   for (uInt im=0; im<mList.size(); im++)
		//     for (uInt iw=0; iw<wList.size(); iw++)
		//       log_l << " Freq: " << fList[inu] 
		// 	    << " MuellerElement: " << mList[im]
		// 	    << " WValue: " << wList[iw]
		// 	    << " FileName: " << cfCacheTable_l[ipa].cfNameList[inu]
		// 	    << LogIO::POST;
	      }

	  }
      }
    catch(AipsError& x)
      {
	throw(SynthesisFTMachineError(String("Error while initializing CF disk cache: ")
				      +x.getMesg()));
      }
  }
  //
  //-----------------------------------------------------------------------
  //
  void CFCache::getCFParams(const String& fileName,
			    Array<Complex>& pixelBuffer,
			    CoordinateSystem& coordSys, 
			    Double& sampling,
			    Double& paVal,
			    Int& xSupport, Int& ySupport,
			    Double& fVal, Double& wVal, Int& mVal)
  {
    try
      {
	PagedImage<Complex> thisCF(Dir+'/'+fileName);
	TableRecord miscinfo = thisCF.miscInfo();

	pixelBuffer.assign(thisCF.get());
	miscinfo.get("ParallacticAngle", paVal);
	miscinfo.get("MuellerElement", mVal);
	miscinfo.get("WValue", wVal);
	miscinfo.get("Xsupport", xSupport);
	miscinfo.get("Ysupport", ySupport);
	miscinfo.get("Sampling", sampling);
	Int index= thisCF.coordinates().findCoordinate(Coordinate::SPECTRAL);
	coordSys = thisCF.coordinates();
	SpectralCoordinate spCS = coordSys.spectralCoordinate(index);
	fVal=static_cast<casa::Float>(spCS.referenceValue()(0));
      }
    catch(AipsError& x)
      {
	throw(SynthesisFTMachineError(String("Error in CFCache::getCFParams(): ")
				      +x.getMesg()));
      }
  }
  //
  //-----------------------------------------------------------------------
  //
  CFCache& CFCache::operator=(const CFCache& other)
  {
    //    if (this != other)
      {
	paList = other.paList;
	Sampling = other.Sampling;
	XSup = other.XSup;
	YSup = other.YSup;
	Dir = other.Dir;
	cfPrefix = other.cfPrefix;
	aux = other.aux;
	paCD_p = other.paCD_p;
	memCache_p = other.memCache_p;
	memCacheWt_p = other.memCacheWt_p;
	cfCacheTable_p = other.cfCacheTable_p;
	OTODone_p = other.OTODone_p;
      }
    return *this;
  };
  //
  //-----------------------------------------------------------------------
  //
  Long CFCache::size()
  {
    Long s=0;
    for(uInt i=0;i<memCache_p.nelements();i++)
      s+=memCache_p[0].data->size();
    for(uInt i=0;i<memCacheWt_p.nelements();i++)
      s+=memCacheWt_p[0].data->size();

    return s*sizeof(Complex);
  }
  //
  //-----------------------------------------------------------------------
  //
  void CFCache::makeFTCoordSys(const CoordinateSystem& coords,
			       const Int& convSize,
			       const Vector<Double>& ftRef,
			       CoordinateSystem& ftCoords)
  {
    Int directionIndex;

    ftCoords = coords;
    directionIndex=ftCoords.findCoordinate(Coordinate::DIRECTION);
    //  The following line follows the (lame) logic that if a
    //  DIRECTION axis was not found, the coordinate system must be of
    //  the FT domain already
    if (directionIndex == -1) return;

    DirectionCoordinate dc;//=coords.directionCoordinate(directionIndex);
    //	AlwaysAssert(directionIndex>=0, AipsError);
    dc=coords.directionCoordinate(directionIndex);
    Vector<Bool> axes(2); axes(0)=axes(1)=True;//axes(2)=True;
    Vector<Int> shape(2,convSize);

    cerr << "CFC: " << shape << endl;

    Vector<Double>ref(4);
    ref(0)=ref(1)=ref(2)=ref(3)=0;
    dc.setReferencePixel(ref);
    Coordinate* ftdc=dc.makeFourierCoordinate(axes,shape);
    Vector<Double> refVal;
    refVal=ftdc->referenceValue();
    // refVal(0)=refVal(1)=0;
    // ftdc->setReferenceValue(refVal);
    ref(0)=ftRef(0);
    ref(1)=ftRef(1);
    ftdc->setReferencePixel(ref);
    
    ftCoords.replaceCoordinate(*ftdc, directionIndex);
    delete ftdc; ftdc=0;
  }
  //
  //-------------------------------------------------------------------------
  //
  Int CFCache::addToMemCache(CFStoreCacheType& memCache_l, 
			     Float pa, CFType* cf, 
			     CoordinateSystem& coords,
			     Vector<Int>& xConvSupport,
			     Vector<Int>& yConvSupport,
			     Float convSampling)
  {
    Float dPA=paCD_p.getParAngleTolerance().getValue("rad");

    Int where=-1, wConvSize = cf->shape()(CFDefs::NWPOS);
    Bool found=searchConvFunction(where, pa, dPA);
    //
    // If the PA value was not found, the return value in "where" is
    // the negative of the location in which the PA value should be
    // found.  Convert it to positive value to be used for resizing
    // etc.
    //
    where=abs(where);
    //
    // Resize the arrays if the CF for the relevant PA was not in the
    // MEM cache.  Note that if the arrays are already of size
    // where+1, Array<>::resize() is a no-op.
    //
    Int N=memCache_l.nelements();

    memCache_l.resize(max(N,where+1), True);
    if ((Int)paList.nelements() <= where)
      {
	IPosition s(2,wConvSize,where+1);
	paList.resize(where+1,True);
	XSup.resize(s,True);	YSup.resize(s,True);
	Sampling.resize(where+1,True);
      }
    //
    // If the PA was not found, enter the aux. values in the internal
    // arrays.
    //
    if (!found)
      {
	paList[where] = pa;
	for(Int iw=0;iw<wConvSize;iw++)
	  {
	    YSup(iw,where) = xConvSupport(iw);
	    XSup(iw,where) = yConvSupport(iw);
	  }
	Sampling[where]=convSampling;
      }
    //
    // If the CF was not in the mem. cache, add it.
    //
    if (memCache_l[where].null())
      {
	Vector<Float> sampling(1);sampling[0]=convSampling;

	Int maxXSup=max(xConvSupport), maxYSup=max(yConvSupport);
	memCache_l[where] = CFStore(cf,coords,sampling,
				    xConvSupport,yConvSupport,
				    maxXSup,maxYSup,Quantity(pa,"rad"),
				    0);
      }
    
    return where;
  }
  //-------------------------------------------------------------------------
  // Write the conv. functions from the mem. cache to the disk cache.
  //
  Int CFCache::cacheConvFunction(Int which,  const Float& pa, CFType& cf, 
				 CoordinateSystem& coords,
				 CoordinateSystem& ftCoords,
				 Int &convSize,
				 Vector<Int> &xConvSupport, 
				 Vector<Int> &yConvSupport, 
				 Float convSampling, 
				 String nameQualifier,
				 Bool savePA)
  {
    LogIO log_l(LogOrigin("CFCache","cacheConvFunction"));
    Int whereCached_l=-1;
    if (Dir.length() == 0) return whereCached_l;
    if (which < 0) 
      {
	Int i;
	searchConvFunction(i,pa,paCD_p.getParAngleTolerance().get("rad"));
	//	which = paList.nelements();
	which = abs(i);
      }
    
    try
      {
	IPosition newConvShape = cf.shape();
	Int wConvSize = newConvShape(CFDefs::NWPOS);
	for(Int iw=0;iw<wConvSize;iw++)
	  {
	    IPosition sliceStart(4,0,0,iw,0), 
	      sliceLength(4,newConvShape(CFDefs::NXPOS),
			  newConvShape(CFDefs::NYPOS),
			  1,
			  newConvShape(CFDefs::NPOLPOS));

	    Vector<Double> ftRef(2);
	    ftRef(0)=newConvShape(CFDefs::NXPOS)/2-1;
	    ftRef(1)=newConvShape(CFDefs::NYPOS)/2-1;
	    makeFTCoordSys(coords, convSize, ftRef, ftCoords);
	    ostringstream name;
	    name << Dir << "/" << cfPrefix << nameQualifier << iw << "_" << which;
	    const CFType tmpArr=cf(Slicer(sliceStart,sliceLength));

	    //	    storeArrayAsImage(name, ftCoords,tmpArr);

	    IPosition screenShape(4,newConvShape(CFDefs::NXPOS),
				  newConvShape(CFDefs::NYPOS),
				  newConvShape(CFDefs::NPOLPOS),
				  1);
	    Record miscinfo;
	    miscinfo.define("Xsupport",xConvSupport);
	    miscinfo.define("Ysupport",yConvSupport);
	    miscinfo.define("sampling", convSampling);
	    miscinfo.define("ParallacticAngle",pa);
	    PagedImage<Complex> thisScreen(screenShape, ftCoords, name);
	    thisScreen.setMiscInfo(miscinfo);
	    Array<Complex> buf;
	    buf=((cf(Slicer(sliceStart,sliceLength)).nonDegenerate()));
	    thisScreen.put(buf);
	  }
	if (savePA)
	  {
	    CFStoreCacheType& memCacheObj = getMEMCacheObj(nameQualifier);
	    whereCached_l=addToMemCache(memCacheObj, pa,&cf, ftCoords, 
	    				xConvSupport, yConvSupport, convSampling);
	  }
      }
    catch (AipsError& x)
      {
	throw(SynthesisFTMachineError("Error while caching CF to disk in "+x.getMesg()));
      }
    return whereCached_l;
  }
  //
  //-------------------------------------------------------------------------
  //  
  void CFCache::cacheConvFunction(const Float pa, CFStore& cfs, 
				  String nameQualifier,Bool savePA)
  {
    if (cfs.data.null())
      throw(SynthesisError(LogMessage("Won't cache a NULL CFStore",
				      LogOrigin("CFCache::cacheConvFunction")).message()));
    CoordinateSystem ftcoords;
    Int which=-1, whereCached=-1;
    Int convSize=(Int)cfs.data->shape()(0);
      
    whereCached = cacheConvFunction(which, pa, *(cfs.data), cfs.coordSys, ftcoords, convSize,
				    cfs.xSupport, cfs.ySupport, cfs.sampling[0],
				    nameQualifier,savePA);
    cfs.coordSys = ftcoords;
    if (whereCached > -1)
      {
	CFStoreCacheType& memCacheObj=getMEMCacheObj(nameQualifier);
	cfs=memCacheObj[whereCached];
      }
  }
  //
  //-------------------------------------------------------------------------
  //  
  Bool CFCache::searchConvFunction(Int& which,
				   const Float pa, const Float dPA)
  {
    if (paList.nelements()==0) initCache();
    Int i,NPA=paList.nelements(); Bool paFound=False;
    Float iPA;
    
    Float paDiff=2*dPA;
    Int saveNdx=-1;

    saveNdx = -1;
    for(i=0;i<NPA;i++)
      {
	iPA = paList[i];
	if (fabs(iPA - pa) < paDiff)
	  {
	    saveNdx = i;
	    paDiff = fabs(iPA-pa);
	  }
      }
    if (saveNdx > -1)
      {
	iPA = paList[saveNdx];
	if (fabs(iPA - pa) <= dPA)
	  {
	    i = saveNdx;
	    paFound=True;
	  }
      }
    if (paFound) which = i; 
    else which = -i;
    return paFound;
  }
  //
  //-------------------------------------------------------------------------
  //Write the aux. info. also in the disk cache (wonder if this should
  //be automatically be called from cacheCFtion() method).
  //
  void CFCache::flush()
  {
    LogIO log_l(LogOrigin("CFCache", "flush"));

    if (Dir.length() == 0) return;
    ostringstream name;
    
    name << Dir << "/aux.dat";
    Int n=memCache_p.nelements(),nw;
    if (n>0)
      try
	{
	  nw=memCache_p[0].xSupport.nelements();
	  ofstream aux(name.str().c_str());
	  aux << n << " " << nw << endl;
	  for(Int ipa=0;ipa<n;ipa++)
	    {
	      aux << paList[ipa]*180.0/M_PI << " ";
	      for(int iw=0;iw<nw;iw++)
		aux << memCache_p[ipa].xSupport(iw) << " " << memCache_p[ipa].ySupport(iw) << " ";
	      aux << " " << Sampling[ipa] <<endl;
	    }
	}
      catch(AipsError &x)
	{
	  throw(SynthesisFTMachineError(String("Error while writing ")
					+ name + x.getMesg()));
	}
  }
  //
  //-------------------------------------------------------------------------
  //Along with the aux. info., also save the average PB in the disk cache.
  //
  void CFCache::flush(ImageInterface<Float>& avgPB, String qualifier)
  {
    LogIO log_l(LogOrigin("CFCache", "flush"));

    if (Dir.length() == 0) return;
    flush();
    ostringstream Name;
    Name << Dir <<"/avgPB" << qualifier;
    try
      {
	storeImg(Name, avgPB);
	avgPBReady_p=True;
	avgPBReadyQualifier_p = qualifier;
      }
    catch(AipsError &x)
      {
	throw(SynthesisFTMachineError(String("Error while writing ")
				      + Name + x.getMesg()));
      }
  }
  //
  //-------------------------------------------------------------------------
  //Load the average PB from the disk cache.
  //
  Int CFCache::loadAvgPB(ImageInterface<Float>& avgPB, String qualifier)
  {
    LogIO log_l(LogOrigin("CFCache", "loadAvgPB"));

    if (Dir.length() == 0) 
      throw(SynthesisFTMachineError("Cache dir. name null"));


    ostringstream name;
    name << Dir << "/avgPB" << qualifier;
    //    cout << name.str() << endl;
    try
      {
	PagedImage<Float> tmp(name.str().c_str());
	avgPB.resize(tmp.shape());
	avgPB.put(tmp.get());
      }
    catch(AipsError& x) // Just rethrowing the exception for now.
                        // Ultimately, this should be used to make
			// the state of this object consistant.
      {
	return NOTCACHED;
      }
    avgPBReady_p=True;
    return DISKCACHE;
  }
  //
  //-------------------------------------------------------------------------
  //Load a conv. func. from the disk.  This is non-optimal due to the
  //data structure used for the conv. func. in-memory cache (it's an
  //array of pointers where it should really be a List of pointers).
  //The conf. func. index, which is also used as a key to located them
  //in the mem. cache, are not assured to be contiguous.  As a result,
  //in the current implementation there can be gaps in the
  //convFuncCache array.  These gaps are initialized to NULL pointers.
  //It's not much of a memory waste, but still non-optimal!  Leaving
  //it like this for now.
  //
  // Return TRUE if loaded from disk and FLASE if found in the mem. cache.
  //
  Int CFCache::loadFromDisk(Int where, Float /*pa*/, Float /*dPA*/,
			    Int Nw, CFStoreCacheType &convFuncCache,
			    CFStore& cfs, String nameQualifier)
  {
    LogIO log_l(LogOrigin("CFCache", "loadFromDisk"));

    Vector<Int> xconvSupport,yconvSupport;;
    Vector<Float> convSampling;
    Double cfRefFreq; CoordinateSystem coordSys;
    Array<Complex> cfBuf;
    Float samplingFromMisc, paFromMisc;
    if (Dir.length() == 0) 
      throw(SynthesisFTMachineError("Cache dir. name not set"));
      
    if (where < (Int)convFuncCache.nelements() && (!convFuncCache[where].data.null())) 
      return MEMCACHE;

    Int wConvSize, polInUse=2;
    Int N=convFuncCache.nelements();

    //
    // Re-size the conv. func. memory cache if required, and set the
    // new members of the resized cache to NULL.  This is used in the
    // loop below to make a decision about allocating new memory or
    // not.
    //
    convFuncCache.resize(max(where+1,N), True);
    //    for(Int i=N;i<=where;i++) convFuncCache[i].data=NULL;
    //
    // Each w-plan is in a separate disk file.  Each file contains all
    // polarization planes. Memory cache holds all w-planes and
    // poln-planes in a single complex array.  The loop below read
    // each w-plane image from the disk, and fills in the 3D
    // mem. cache for each computed PA.
    //
    wConvSize = Nw;
    for(Int iw=0;iw<Nw;iw++)
      {
	ostringstream name;
	name << Dir << "/" << cfPrefix << nameQualifier << iw << "_" << where;
	try
	  {
	    PagedImage<Complex> tmp(name.str().c_str());
	    Record miscInfo;
	    miscInfo = tmp.miscInfo();

	    miscInfo.get("Xsupport", xconvSupport);
	    miscInfo.get("Ysupport", yconvSupport);
	    miscInfo.get("sampling", samplingFromMisc);
	    miscInfo.get("ParallacticAngle", paFromMisc);
	    convSampling = samplingFromMisc;


	    Int index= tmp.coordinates().findCoordinate(Coordinate::SPECTRAL);
	    coordSys = tmp.coordinates();
	    SpectralCoordinate spCS = coordSys.spectralCoordinate(index);

	    cfRefFreq=spCS.referenceValue()(0);
	
	    polInUse = tmp.shape()(2);
	    IPosition ts=tmp.shape(),ndx(4,0,0,0,0),ts2(4,0,0,0,0);
	    Array<Complex> imBuf=tmp.get();
	    if (convFuncCache[where].data.null())
	      cfBuf.resize(IPosition(4,ts(0),ts(1), wConvSize,polInUse));
	    //	      cfBuf = new CFType(IPosition(4,ts(0),ts(1), wConvSize,polInUse));
	
	    ndx(CFDefs::NWPOS)=iw;                  // The w-axis
	    for(ndx(CFDefs::NPOLPOS)=0;ndx(CFDefs::NPOLPOS)<polInUse;ndx(CFDefs::NPOLPOS)++)  // The Poln. axis.
	      for(ndx(CFDefs::NXPOS)=0;ndx(CFDefs::NXPOS)<ts(CFDefs::NXPOS);ndx(CFDefs::NXPOS)++)   
	    	for(ndx(CFDefs::NYPOS)=0;ndx(CFDefs::NYPOS)<ts(CFDefs::NYPOS);ndx(CFDefs::NYPOS)++)
	    	  {
	    	    ts2(CFDefs::NXPOS)=ndx(CFDefs::NXPOS);
		    ts2(CFDefs::NYPOS)=ndx(CFDefs::NYPOS);
	    	    ts2(2)=ndx(CFDefs::NPOLPOS); // The Poln. axis of the disk-cache. The LHS index is different!
	    	    ts2(3)=0;      // The freq. axis of the disk-cache
	    	    (cfBuf)(ndx)=imBuf(ts2);
	    	  }
	  }
	catch(AipsError &x)
	  {
	    throw(SynthesisFTMachineError(String("Error while loading \"")+
					  name + String("\": ") + x.getMesg()));
	  }
      }
    // xconvSupport.resize(wConvSize,True);
    // yconvSupport.resize(wConvSize,True);
    // for(Int i=0;i<wConvSize;i++)
    //   {
    // 	xconvSupport(i) = XSup(i,where);
    // 	yconvSupport(i) = YSup(i,where);
    //   }

    // convSampling = Sampling[where];

    // where=addToMemCache(convFuncCache, pa, &cfBuf, coordSys, 
    // 			xconvSupport, yconvSupport, Sampling[where]);
    where=addToMemCache(convFuncCache, paFromMisc, &cfBuf, coordSys, 
			xconvSupport, yconvSupport, samplingFromMisc);
    cfs=convFuncCache[where];
    //    convFuncCache[where].show("loadFromDisk: ");

    return DISKCACHE;
  };
  //
  //-----------------------------------------------------------------------
  //
  Int CFCache::locateConvFunction(CFStore& cfs, CFStore& cfwts,
				  const Int Nw, const Float pa, const Float dPA,
				  const Int mosXPos, const Int mosYPos)
  {
    Int retVal;
    // This assumes that the return state of locateConvFunction() for
    // "CF" and "CFWT" will be the same.  I.e. if "CF" is found in the
    // cache, "CFWT" will be found.  If "CF" is not found "CFWT" won't
    // be found either.
    retVal=locateConvFunction(cfs, Nw, pa, dPA,"",mosXPos,mosYPos);
    locateConvFunction(cfwts, Nw, pa, dPA,"WT",mosXPos,mosYPos);

   return retVal;
  }
  //
  //-----------------------------------------------------------------------
  //
  // Locate a convlution function in either mem. or disk cache.  
  // Return CFCache::DISKCACHE (=1) if found in the disk cache.
  //        CFCache::MEMCACHE (=2)  if found in the mem. cache.
  //       <0 if not found in either cache.  In this case, absolute of
  //          the return value corresponds to the index in the list of
  //          conv. funcs. where this conv. func. should be filled
  //
  Int CFCache::locateConvFunction(CFStore& cfs, 
				  const Int Nw, const Float pa, const Float dPA,
				  const String& nameQualifier,
				  const Int /*mosXPos*/, const Int /*mosYPos*/)
  {
    LogIO log_l(LogOrigin("CFCache", "locatedConvFunction"));

    Int paKey,retVal=NOTCACHED; 
    Bool found;
    // Search for the PA corresponding to the supplied VB to find a
    // paKey in memCache_p which has a Conv. Func. within dPA (dPA is
    // given by paCD).  If found, return the key in paKey.
    found = searchConvFunction(paKey,pa, dPA);

    if (found)
      {
	CFStoreCacheType &memCacheObj=getMEMCacheObj(nameQualifier);
	retVal=loadFromDisk(paKey,pa,dPA,Nw,memCacheObj,cfs,nameQualifier);
	
	switch (retVal)
	  {
	  case DISKCACHE:
	    {
	      if (paKey < (Int)memCacheObj.nelements())
	      	log_l << "Loaded from disk cache: Conv. func. # "
		      << paKey << LogIO::POST;
	      else
	      	throw(SynthesisFTMachineError("Internal error: paKey out of range"));
	    }
	  case MEMCACHE:  
	    {
	      cfs=(memCacheObj[paKey]);
	      break;
	    }
	  case NOTCACHED: {break;}
	  };
      }
    return retVal;
  }
  //
  //-----------------------------------------------------------------------
  //
  CFCache::CFStoreCacheType& CFCache::getMEMCacheObj(const String& nameQualifier)
  {
    LogIO log_l(LogOrigin("CFCache::getMEMCacheObj"));
    if (nameQualifier == "")           return memCache_p;
    else if (nameQualifier == "WT")    return memCacheWt_p;
    else
      log_l << "Internal error. Unknown name qualifier '"+nameQualifier+"'."
	    << LogIO::EXCEPTION << endl;
    // 
    // Return to suppress compiler warning.  Control will never reach
    // the following line.
    //
    return memCache_p;
  }
  //
  //-----------------------------------------------------------------------
  //
  // void CFCache::constructTable_p(CFCache::CFCacheTable& tab)
  // {
  //   TableDesc td("CFCache","0.0",TableDesc::Scratch);
    
  //   add PA and baseline type info.

  //   td.addColumn(ScalarColumnDesc<Double>("Ref. Frequency"));
  //   td.addColumn(ScalarColumnDesc<Double>("W Value"));
  //   td.addColumn(ScalarColumnDesc<uInt>("Mueller Index"));
  //   td.addColumn(ScalarColumnDesc<String>("CF Disk File Name"));
  //   SetupNewTable newTab("CFCache.dat",td,Table::Scratch);
  //   tab = Table(newTab);
  // }

} // end casa namespace
