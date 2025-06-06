// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//
/**********************************************
 *
 *   Class meant to do Glauber+NBD fits
 *
 *   This class makes full use of analytical
 *   properties of the Negative Binomial Function
 *
 *   Only the Glauber component is MC, while
 *   the NBD is evaluated probabilistically
 *
 *  - bugs, comments, suggestions, complaints?
 *  - Feel free to write to:
 *     david.dobrigkeit.chinellato@cern.ch
 *
 **********************************************/

#include "multGlauberNBDFitter.h"
#include "TList.h"
#include "TFile.h"
#include "TF1.h"
#include "TStopwatch.h"
#include "TVirtualFitter.h"
#include "TProfile.h"
#include "TFitResult.h"

using namespace std;

ClassImp(multGlauberNBDFitter);

multGlauberNBDFitter::multGlauberNBDFitter() : TNamed(),
                                               fNBD(0x0),
                                               fhNanc(0x0),
                                               fhNpNc(0x0),
                                               ffChanged(kTRUE),
                                               fCurrentf(-1),
                                               fAncestorMode(2),
                                               fNpart(0x0),
                                               fNcoll(0x0),
                                               fContent(0x0),
                                               fNNpNcPairs(-1),
                                               fMaxNpNcPairs(1000000),
                                               fMu(45),
                                               fdMu(0.0),
                                               fk(1.5),
                                               ff(0.8),
                                               fnorm(100),
                                               fFitOptions("R0"),
                                               fFitNpx(5000)
{
  // Constructor
  fNpart = new Double_t[fMaxNpNcPairs];
  fNcoll = new Double_t[fMaxNpNcPairs];
  fContent = new Long_t[fMaxNpNcPairs];

  //Ancestor histo
  fhNanc = new TH1D("fhNanc", "", 1000, -0.5, 999.5);

  //NBD
  fNBD = new TF1("fNBD", "ROOT::Math::negative_binomial_pdf(x,[0],[1])", 0, 45000);
  fNBD->SetNpx(45000);

  //master function
  fGlauberNBD = new TF1("fGlauberNBD", this, &multGlauberNBDFitter::ProbDistrib,
                        0, 50000, 4, "multGlauberNBDFitter", "ProbDistrib");
  fGlauberNBD->SetParameter(0, fMu);
  fGlauberNBD->SetParameter(1, fk);
  fGlauberNBD->SetParameter(2, ff);
  fGlauberNBD->SetParameter(3, fnorm);

  fGlauberNBD->SetParName(0, "mu");
  fGlauberNBD->SetParName(1, "k");
  fGlauberNBD->SetParName(2, "f");
  fGlauberNBD->SetParName(3, "norm");
  fGlauberNBD->SetParName(4, "dMu/dNanc");
}

multGlauberNBDFitter::multGlauberNBDFitter(const char* name, const char* title) : TNamed(name, title),
                                                                                  fNBD(0x0),
                                                                                  fhNanc(0x0),
                                                                                  fhNpNc(0x0),
                                                                                  ffChanged(kTRUE),
                                                                                  fCurrentf(-1),
                                                                                  fAncestorMode(2),
                                                                                  fNpart(0x0),
                                                                                  fNcoll(0x0),
                                                                                  fContent(0x0),
                                                                                  fNNpNcPairs(-1),
                                                                                  fMaxNpNcPairs(1000000),
                                                                                  fMu(45),
                                                                                  fdMu(0.0),
                                                                                  fk(1.5),
                                                                                  ff(0.8),
                                                                                  fnorm(100),
                                                                                  fFitOptions("R0"),
                                                                                  fFitNpx(5000)
{
  //Named constructor
  fNpart = new Double_t[fMaxNpNcPairs];
  fNcoll = new Double_t[fMaxNpNcPairs];
  fContent = new Long_t[fMaxNpNcPairs];

  //Ancestor histo
  //fhNanc = new TH1D("fhNanc", "", fAncestorMode==2?10000:1000, -0.5, 999.5);

  //NBD
  fNBD = new TF1("fNBD", "ROOT::Math::negative_binomial_pdf(x,[0],[1])", 0, 45000);
  fNBD->SetNpx(45000);

  //master function
  fGlauberNBD = new TF1("fGlauberNBD", this, &multGlauberNBDFitter::ProbDistrib,
                        0, 50000, 5, "multGlauberNBDFitter", "ProbDistrib");
  fGlauberNBD->SetParameter(0, fMu);
  fGlauberNBD->SetParameter(1, fk);
  fGlauberNBD->SetParameter(2, ff);
  fGlauberNBD->SetParameter(3, fnorm);

  fGlauberNBD->SetParName(0, "mu");
  fGlauberNBD->SetParName(1, "k");
  fGlauberNBD->SetParName(2, "f");
  fGlauberNBD->SetParName(3, "norm");
  fGlauberNBD->SetParName(4, "dMu/dNanc");
}
//________________________________________________________________
multGlauberNBDFitter::~multGlauberNBDFitter()
{
  // Destructor
  if (fNBD) {
    delete fNBD;
    fNBD = 0x0;
  }
  if (fhNanc) {
    delete fhNanc;
    fhNanc = 0x0;
  }
  if (fhNpNc) {
    delete fhNpNc;
    fhNpNc = 0x0;
  }
  if (fNpart)
    delete[] fNpart;
  if (fNcoll)
    delete[] fNcoll;
  if (fContent)
    delete[] fContent;
}

//______________________________________________________
Double_t multGlauberNBDFitter::ProbDistrib(Double_t* x, Double_t* par)
//Master fitter function
{
  Double_t lMultValue = x[0];
  Double_t lProbability = 0.0;
  ffChanged = kTRUE;
  const Double_t lAlmost0 = 1.e-13;
  //Comment this line in order to make the code evaluate Nancestor all the time
  if (TMath::Abs(fCurrentf - par[2]) < lAlmost0)
    ffChanged = kFALSE;

  //______________________________________________________
  //Recalculate the ancestor distribution in case f changed
  if (ffChanged) {
    fCurrentf = par[2];
    fhNanc->Reset();

    for (int ibin = 0; ibin < fNNpNcPairs; ibin++) {
      Double_t lOption0 = (Int_t)(fNpart[ibin] * par[2] + fNcoll[ibin] * (1.0 - par[2]));
      Double_t lOption1 = TMath::Floor(fNpart[ibin] * par[2] + fNcoll[ibin] * (1.0 - par[2]) + 0.5);
      Double_t lOption2 = (fNpart[ibin] * par[2] + fNcoll[ibin] * (1.0 - par[2]));
      if (fAncestorMode == 0)
        fhNanc->Fill(lOption0, fContent[ibin]);
      if (fAncestorMode == 1)
        fhNanc->Fill(lOption1, fContent[ibin]);
      if (fAncestorMode == 2)
        fhNanc->Fill(lOption2, fContent[ibin]);
    }
    if (fhNanc->Integral() < 1) {
      cout << "ERROR: ANCESTOR HISTOGRAM EMPTY" << endl;
      cout << "Will not do anything. Call InitializeNpNc if you want to plot without fitting" << endl;
      return 0;
    }
    fhNanc->Scale(1. / fhNanc->Integral());
  }
  //______________________________________________________
  //Actually evaluate function
  Int_t lStartBin = fhNanc->FindBin(0.0) + 1;
  for (Long_t iNanc = lStartBin; iNanc < fhNanc->GetNbinsX() + 1; iNanc++) {
    Double_t lNancestors = fhNanc->GetBinCenter(iNanc);
    Double_t lNancestorCount = fhNanc->GetBinContent(iNanc);
    //if(lNancestorCount<1e-12&&lNancestors>10) break;

    // allow for variable mu in case requested
    Double_t lThisMu = (((Double_t)lNancestors)) * (par[0] + par[4] * lNancestors);
    Double_t lThisk = (((Double_t)lNancestors)) * par[1];
    Double_t lpval = TMath::Power(1.0 + lThisMu / lThisk, -1);
    fNBD->SetParameter(1, lThisk);
    fNBD->SetParameter(0, lpval);
    Double_t lMult = 0.0;
    if (lMultValue > 1e-6)
      lMult = fAncestorMode != 2 ? fNBD->Eval(lMultValue) : ContinuousNBD(lMultValue, lThisMu, lThisk);
    lProbability += lNancestorCount * lMult;
  }
  //______________________________________________________
  return par[3] * lProbability;
}

//________________________________________________________________
Bool_t multGlauberNBDFitter::SetNpartNcollCorrelation(TH2* hNpNc)
{
  Bool_t lReturnValue = kTRUE;
  if (hNpNc) {
    fhNpNc = (TH2*)hNpNc;
  } else {
    lReturnValue = kFALSE;
  }
  return lReturnValue;
}

//________________________________________________________________
Bool_t multGlauberNBDFitter::SetInputV0M(TH1* hV0M)
{
  Bool_t lReturnValue = kTRUE;
  if (hV0M) {
    fhV0M = (TH1*)hV0M;
  } else {
    lReturnValue = kFALSE;
  }
  return lReturnValue;
}

//________________________________________________________________
TF1* multGlauberNBDFitter::GetNBD()
{
  return fNBD;
}

//________________________________________________________________
TF1* multGlauberNBDFitter::GetGlauberNBD()
{
  return fGlauberNBD;
}

//________________________________________________________________
void multGlauberNBDFitter::SetFitRange(Double_t lMin, Double_t lMax)
{
  fGlauberNBD->SetRange(lMin, lMax);
}

//________________________________________________________________
void multGlauberNBDFitter::SetFitOptions(TString lOpt)
{
  fFitOptions = lOpt;
}

//________________________________________________________________
void multGlauberNBDFitter::SetFitNpx(Long_t lNpx)
{
  fFitNpx = lNpx;
}

//________________________________________________________________
void multGlauberNBDFitter::InitAncestor()
{
  if (!fhNanc)
    fhNanc = new TH1D("fhNanc", "", fAncestorMode == 2 ? 10000 : 1000, -0.5, 999.5);
}

//________________________________________________________________
Bool_t multGlauberNBDFitter::DoFit()
{
  InitAncestor();
  //Try very hard, please
  TVirtualFitter::SetMaxIterations(5000000);
  if (!InitializeNpNc()) {
    cout << "---> Initialization of Npart x Ncoll correlation info failed!" << endl;
    return kFALSE;
  }

  TStopwatch* timer = new TStopwatch();
  timer->Start(kTRUE);
  if (fAncestorMode == 0)
    cout << "---> Config: Nancestors will be truncated" << endl;
  if (fAncestorMode == 1)
    cout << "---> Config: Nancestors will be rounded" << endl;
  if (fAncestorMode == 2)
    cout << "---> Config: Nancestors will be taken as float" << endl;
  cout << "---> Now fitting, please wait..." << endl;

  fGlauberNBD->SetNpx(fFitNpx);
  TFitResultPtr fitptr;
  fFitOptions.Append("S");
  fitptr = fhV0M->Fit("fGlauberNBD", fFitOptions.Data());

  timer->Stop();
  Double_t lTotalTime = timer->RealTime();
  cout << "---> Fitting took " << lTotalTime << " seconds" << endl;

  fMu = fGlauberNBD->GetParameter(0);
  fk = fGlauberNBD->GetParameter(1);
  ff = fGlauberNBD->GetParameter(2);
  fnorm = fGlauberNBD->GetParameter(3);
  fdMu = fGlauberNBD->GetParameter(4);

  return fitptr.Get()->IsValid();
}

//________________________________________________________________
Bool_t multGlauberNBDFitter::InitializeNpNc()
{
  //This function initializes fhNpNc
  //Warning: X == Npart, Y == Ncoll
  Bool_t lReturnValue = kFALSE;
  if (fhNpNc) {
    fNNpNcPairs = 0;
    //Sweep all allowed values of Npart, Ncoll; find counters
    for (int xbin = 1; xbin < 500; xbin++) {
      for (int ybin = 1; ybin < 3000; ybin++) {
        if (fhNpNc->GetBinContent(fhNpNc->FindBin(xbin, ybin)) != 0) {
          fNpart[fNNpNcPairs] = xbin;
          fNcoll[fNNpNcPairs] = ybin;
          fContent[fNNpNcPairs] = fhNpNc->GetBinContent(fhNpNc->FindBin(xbin, ybin));
          fNNpNcPairs++;
        }
      }
    }
    cout << "Initialized with number of (Npart, Ncoll) pairs: " << fNNpNcPairs << endl;
    lReturnValue = kTRUE;
  } else {
    cout << "Failed to initialize! Please provide input histogram with (Npart, Ncoll) info!" << endl;
    cout << "Please remember to call SetNpartNcollCorrelation before doing fit!" << endl;
  }
  return lReturnValue;
}

//________________________________________________________________
Double_t multGlauberNBDFitter::ContinuousNBD(Double_t n, Double_t mu, Double_t k)
{
  //Adaptation of the negative binomial distribution
  //for non-integer arguments: analytical continuation
  //
  //This function would actually also be fine with integers;
  //in fact it is equivalent to that if 'n' is typecast as
  //an integer prior to use

  Double_t F;
  Double_t f;

  if (n + k > 100.0) {
    // log method for handling large numbers
    F = TMath::LnGamma(n + k) - TMath::LnGamma(n + 1.) - TMath::LnGamma(k);
    f = n * TMath::Log(mu / k) - (n + k) * TMath::Log(1.0 + mu / k);
    F = F + f;
    F = TMath::Exp(F);
  } else {
    F = TMath::Gamma(n + k) / (TMath::Gamma(n + 1.) * TMath::Gamma(k));
    f = n * TMath::Log(mu / k) - (n + k) * TMath::Log(1.0 + mu / k);
    f = TMath::Exp(f);
    F *= f;
  }
  return F;
}

void multGlauberNBDFitter::CalculateAvNpNc(TProfile* lNPartProf, TProfile* lNCollProf, TH2F* lNPart2DPlot, TH2F* lNColl2DPlot, TH1F* hPercentileMap, Double_t lLoRange, Double_t lHiRange)
{
  cout << "Calculating <Npart>, <Ncoll> in centrality bins..." << endl;
  cout << "Range to calculate: " << lLoRange << " to " << lHiRange << endl;

  cout << "Acquiring values from the fit function..." << endl;

  fMu = fGlauberNBD->GetParameter(0);
  fk = fGlauberNBD->GetParameter(1);
  ff = fGlauberNBD->GetParameter(2);
  fnorm = fGlauberNBD->GetParameter(3);
  fdMu = fGlauberNBD->GetParameter(4);

  cout << "Please inspect now: " << endl;
  cout << "Glauber NBD mu ............: " << fMu << endl;
  cout << "Glauber NBD k .............: " << fk << endl;
  cout << "Glauber NBD f .............: " << ff << endl;
  cout << "Glauber NBD norm ..........: " << fnorm << endl;
  cout << "Glauber NBD dmu/dNanc .....: " << fdMu << endl;

  //2-fold nested loop:
  // + looping over all Nancestor combinations
  // + looping over all possible final multiplicities
  // ^---> final product already multiplicity-binned

  //______________________________________________________
  if (lLoRange < -1 && lHiRange < -1) {
    fGlauberNBD->GetRange(lLoRange, lHiRange);
  }
  // bypass to zero
  for (int ibin = 0; ibin < fNNpNcPairs; ibin++) {
    if (ibin % 2000 == 0)
      cout << "At NpNc pair #" << ibin << " of " << fNNpNcPairs << "..." << endl;
    Double_t lNAncestors0 = (Int_t)(fNpart[ibin] * ff + fNcoll[ibin] * (1.0 - ff));
    Double_t lNAncestors1 = TMath::Floor(fNpart[ibin] * ff + fNcoll[ibin] * (1.0 - ff) + 0.5);
    Double_t lNAncestors2 = (fNpart[ibin] * ff + fNcoll[ibin] * (1.0 - ff));
    for (Long_t lMultValue = 1; lMultValue < lHiRange; lMultValue++) {
      Double_t lNancestors = lNAncestors0;
      if (fAncestorMode == 1)
        lNancestors = lNAncestors1;
      if (fAncestorMode == 2)
        lNancestors = lNAncestors2;
      Double_t lNancestorCount = fContent[ibin];
      Double_t lThisMu = (((Double_t)lNancestors)) * fMu;
      Double_t lThisk = (((Double_t)lNancestors)) * fk;
      Double_t lpval = TMath::Power(1 + lThisMu / lThisk, -1);
      fNBD->SetParameter(1, lThisk);
      fNBD->SetParameter(0, lpval);
      Double_t lMult = 0.0;
      if (lMultValue > 1e-6)
        lMult = fAncestorMode != 2 ? fNBD->Eval(lMultValue) : ContinuousNBD(lMultValue, lThisMu, lThisk);
      Double_t lProbability = lNancestorCount * lMult;
      Double_t lMultValueToFill = lMultValue;
      if (hPercentileMap)
        lMultValueToFill = hPercentileMap->GetBinContent(hPercentileMap->FindBin(lMultValue));
      lNPartProf->Fill(lMultValueToFill, fNpart[ibin], lProbability);
      lNCollProf->Fill(lMultValueToFill, fNcoll[ibin], lProbability);
      if (lNPart2DPlot)
        lNPart2DPlot->Fill(lMultValueToFill, fNpart[ibin], lProbability);
      if (lNColl2DPlot)
        lNColl2DPlot->Fill(lMultValueToFill, fNcoll[ibin], lProbability);
    }
  }
}
