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
///
/// \brief QA task for Cascade analysis using derived data
///
/// \author Francesca Ercolessi (francesca.ercolessi@cern.ch)

#include "Framework/runDataProcessing.h"
#include "Framework/AnalysisTask.h"
#include "Common/DataModel/TrackSelectionTables.h"
#include "PWGLF/DataModel/LFStrangenessTables.h"
#include "Common/DataModel/EventSelection.h"
#include "Common/DataModel/PIDResponse.h"
#include "Common/DataModel/Multiplicity.h"
#include "Common/DataModel/Centrality.h"
#include "PWGLF/DataModel/cascqaanalysis.h"
#include "TRandom.h"
#include "Framework/O2DatabasePDGPlugin.h"

using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;

// using DauTracks = soa::Join<aod::TracksIU, aod::TracksExtra, aod::TracksCovIU, aod::TracksDCA, aod::pidTPCPi, aod::pidTPCKa, aod::pidTPCPr, aod::pidTOFPi, aod::pidTOFPr>;
using DauTracks = soa::Join<aod::Tracks, aod::TracksExtra, aod::TracksDCA, aod::pidTPCPi, aod::pidTPCPr, aod::pidTPCKa, aod::pidTOFPi, aod::pidTOFPr, aod::pidTOFKa>;
using LabeledCascades = soa::Join<aod::CascDataExt, aod::McCascLabels>;

struct cascqaanalysis {

  // Produces
  Produces<aod::MyCascades> mycascades;
  Produces<aod::MyMCCascades> myMCcascades;

  HistogramRegistry registry{"registry"};

  AxisSpec ptAxis = {200, 0.0f, 10.0f, "#it{p}_{T} (GeV/#it{c})"};
  AxisSpec rapidityAxis = {200, -2.0f, 2.0f, "y"};

  void init(InitContext const&)
  {
    TString hCandidateCounterLabels[5] = {"All candidates", "v0data exists", "passed topo cuts", "has associated MC particle", "associated with Xi(Omega)"};
    TString hNEventsMCLabels[6] = {"All", "z vrtx", "INEL", "INEL>0", "INEL>1", "Associated with rec. collision"};
    TString hNEventsLabels[6] = {"All", "sel8", "z vrtx", "INEL", "INEL>0", "INEL>1"};

    registry.add("hNEvents", "hNEvents", {HistType::kTH1F, {{6, 0.f, 6.f}}});
    for (Int_t n = 1; n <= registry.get<TH1>(HIST("hNEvents"))->GetNbinsX(); n++) {
      registry.get<TH1>(HIST("hNEvents"))->GetXaxis()->SetBinLabel(n, hNEventsLabels[n - 1]);
    }
    registry.add("hNAssocMCCollisions", "hNAssocMCCollisions", {HistType::kTH1F, {{5, -0.5f, 4.5f}}});
    registry.add("hNContributorsCorrelation", "hNContributorsCorrelation", {HistType::kTH2F, {{250, -0.5f, 249.5f, "Secondary Contributor"}, {250, -0.5f, 249.5f, "Main Contributor"}}});
    registry.add("hZCollision", "hZCollision", {HistType::kTH1F, {{200, -20.f, 20.f}}});
    registry.add("hZCollisionGen", "hZCollisionGen", {HistType::kTH1F, {{200, -20.f, 20.f}}});
    registry.add("hCentFT0M", "hCentFT0M", {HistType::kTH2F, {{1055, 0.f, 105.5f}, {3, -0.5f, 2.5f}}});
    registry.add("hCentFV0A", "hCentFV0A", {HistType::kTH2F, {{1055, 0.f, 105.5f}, {3, -0.5f, 2.5f}}});

    registry.add("hNEventsMC", "hNEventsMC", {HistType::kTH1F, {{6, 0.0f, 6.0f}}});
    for (Int_t n = 1; n <= registry.get<TH1>(HIST("hNEventsMC"))->GetNbinsX(); n++) {
      registry.get<TH1>(HIST("hNEventsMC"))->GetXaxis()->SetBinLabel(n, hNEventsMCLabels[n - 1]);
    }

    registry.add("hCandidateCounter", "hCandidateCounter", {HistType::kTH1F, {{5, 0.0f, 5.0f}}});
    for (Int_t n = 1; n <= registry.get<TH1>(HIST("hCandidateCounter"))->GetNbinsX(); n++) {
      registry.get<TH1>(HIST("hCandidateCounter"))->GetXaxis()->SetBinLabel(n, hCandidateCounterLabels[n - 1]);
    }

    AxisSpec allTracks = {2000, 0, 2000, "N_{all tracks}"};
    AxisSpec secondaryTracks = {2000, 0, 2000, "N_{secondary tracks}"};
  }

  // Event selection criteria
  Configurable<float> cutzvertex{"cutzvertex", 20.0f, "Accepted z-vertex range (cm)"};
  Configurable<bool> sel8{"sel8", 1, "Apply sel8 event selection"};

  // Cascade selection criteria
  Configurable<float> scalefactor{"scalefactor", 1.0, "Scaling factor"};
  Configurable<double> casccospa{"casccospa", 0.97, "Casc CosPA"};
  Configurable<double> v0cospa{"v0cospa", 0.97, "V0 CosPA"};
  Configurable<float> dcacascdau{"dcacascdau", 2.0, "DCA Casc Daughters"};
  Configurable<float> dcav0dau{"dcav0dau", 2.0, "DCA V0 Daughters"};
  Configurable<float> dcanegtopv{"dcanegtopv", 0.0, "DCA Neg To PV"};
  Configurable<float> dcapostopv{"dcapostopv", 0.0, "DCA Pos To PV"};
  Configurable<float> dcabachtopv{"dcabachtopv", 0.0, "DCA Bach To PV"};
  Configurable<float> v0radius{"v0radius", 0.0, "V0 Radius"};
  Configurable<float> cascradius{"cascradius", 0.0, "Casc Radius"};
  Configurable<float> etadau{"etadau", 0.8, "Eta Daughters"};

  // Necessary for particle charges
  Service<O2DatabasePDG> pdgDB;

  TRandom* fRand = new TRandom();

  Filter preFilter =
    (nabs(aod::cascdata::dcapostopv) > dcapostopv &&
     nabs(aod::cascdata::dcanegtopv) > dcanegtopv &&
     nabs(aod::cascdata::dcabachtopv) > dcabachtopv &&
     aod::cascdata::dcaV0daughters < dcav0dau &&
     aod::cascdata::dcacascdaughters < dcacascdau);

  template <class TCascTracksTo, typename TCascade>
  bool AcceptCascCandidate(TCascade const& cascCand, float const& pvx, float const& pvy, float const& pvz)
  {
    // Access daughter tracks
    auto v0index = cascCand.template v0_as<o2::aod::V0sLinked>();
    auto v0 = v0index.v0Data();
    auto posdau = v0.template posTrack_as<TCascTracksTo>();
    auto negdau = v0.template negTrack_as<TCascTracksTo>();
    auto bachelor = cascCand.template bachelor_as<TCascTracksTo>();

    // Basic set of selections
    if (cascCand.cascradius() > cascradius &&
        v0.v0radius() > v0radius &&
        cascCand.casccosPA(pvx, pvy, pvz) > casccospa &&
        cascCand.v0cosPA(pvx, pvy, pvz) > v0cospa &&
        TMath::Abs(posdau.eta()) < etadau &&
        TMath::Abs(negdau.eta()) < etadau &&
        TMath::Abs(bachelor.eta()) < etadau) {
      return true;
    } else {
      return false;
    }
  }

  template <typename TCollision, typename TTracks>
  bool AcceptEvent(TCollision const& collision, TTracks const& tracks, bool isFillEventSelectionQA)
  {
    if (isFillEventSelectionQA) {
      registry.fill(HIST("hNEvents"), 0.5);
    }
    // Event selection if required
    if (sel8 && !collision.sel8()) {
      return false;
    }
    if (isFillEventSelectionQA) {
      registry.fill(HIST("hNEvents"), 1.5);
    }

    if (TMath::Abs(collision.posZ()) > cutzvertex) {
      return false;
    }
    if (isFillEventSelectionQA) {
      registry.fill(HIST("hNEvents"), 2.5);
    }

    int evFlag = 0;
    if (isFillEventSelectionQA) {
      registry.fill(HIST("hNEvents"), 3.5);
      if (collision.multNTracksPVeta1() > 0) {
        evFlag += 1;
        registry.fill(HIST("hNEvents"), 4.5);
      }
      if (collision.multNTracksPVeta1() > 1) {
        evFlag += 1;
        registry.fill(HIST("hNEvents"), 5.5);
      }
    }

    if (isFillEventSelectionQA) {
      registry.fill(HIST("hZCollision"), collision.posZ());
      registry.fill(HIST("hCentFT0M"), collision.centFT0M(), evFlag);
      registry.fill(HIST("hCentFV0A"), collision.centFV0A(), evFlag);
    }
    return true;
  }

  template <typename TMcParticles>
  bool isINELgtNmc(TMcParticles particles, int nChToSatisfySelection)
  {
    // INEL > 0 (at least 1 charged particle in |eta| < 1.0)
    typedef struct EtaCharge {
      double eta;
      int charge;
    } EtaCharge;
    EtaCharge etaCharge;
    std::vector<EtaCharge> ParticlesEtaAndCharge(particles.size());
    unsigned int nParticles = 0;
    for (const auto& particle : particles) {
      if (particle.isPhysicalPrimary() == 0)
        continue;           // consider only primaries
      etaCharge = {999, 0}; // refresh init. for safety
      TParticlePDG* p = pdgDB->GetParticle(particle.pdgCode());
      if (!p) {
        switch (std::to_string(particle.pdgCode()).length()) {
          case 10: // nuclei
          {
            etaCharge = {particle.eta(), static_cast<int>(particle.pdgCode() / 10000 % 1000)};
            ParticlesEtaAndCharge[nParticles++] = etaCharge;
            break;
          }
          default:
            break;
        }
      } else {
        etaCharge = {particle.eta(), static_cast<int>(p->Charge())};
        ParticlesEtaAndCharge[nParticles++] = etaCharge;
      }
    }

    ParticlesEtaAndCharge.resize(nParticles);

    auto etaChargeConditionFunc = [](EtaCharge elem) {
      return ((TMath::Abs(elem.eta) < 1.0) && (TMath::Abs(elem.charge) > 0.001));
    };

    if (std::count_if(ParticlesEtaAndCharge.begin(), ParticlesEtaAndCharge.end(), etaChargeConditionFunc) > nChToSatisfySelection) {
      return true;
    } else {
      return false;
    }
  }

  void processData(soa::Join<aod::Collisions, aod::EvSels, aod::Mults, aod::CentFT0Ms, aod::CentFV0As>::iterator const& collision,
                   soa::Filtered<aod::CascDataExt> const& Cascades,
                   aod::V0sLinked const&,
                   aod::V0Datas const&,
                   DauTracks const& Tracks)
  {
    if (!AcceptEvent(collision, Tracks, 1)) {
      return;
    }

    float lEventScale = scalefactor;

    for (const auto& casc : Cascades) {              // loop over Cascades
      registry.fill(HIST("hCandidateCounter"), 0.5); // all candidates

      // Access daughter tracks
      auto v0index = casc.v0_as<o2::aod::V0sLinked>();
      if (!(v0index.has_v0Data())) {
        return; // skip those cascades for which V0 doesn't exist
      }
      registry.fill(HIST("hCandidateCounter"), 1.5); // v0data exists

      auto v0 = v0index.v0Data();
      auto posdau = v0.posTrack_as<DauTracks>();
      auto negdau = v0.negTrack_as<DauTracks>();
      auto bachelor = casc.bachelor_as<DauTracks>();

      // ITS N hits
      int posITSNhits = 0, negITSNhits = 0, bachITSNhits = 0;
      for (unsigned int i = 0; i < 7; i++) {
        if (posdau.itsClusterMap() & (1 << i)) {
          posITSNhits++;
        }
        if (negdau.itsClusterMap() & (1 << i)) {
          negITSNhits++;
        }
        if (bachelor.itsClusterMap() & (1 << i)) {
          bachITSNhits++;
        }
      }

      uint8_t flags = 0;
      flags |= o2::aod::mycascades::EvFlags::EvINEL;
      if (collision.multNTracksPVeta1() > 0) {
        flags |= o2::aod::mycascades::EvFlags::EvINELgt0;
      }
      if (collision.multNTracksPVeta1() > 1) {
        flags |= o2::aod::mycascades::EvFlags::EvINELgt1;
      }

      // c x tau
      float cascpos = std::hypot(casc.x() - collision.posX(), casc.y() - collision.posY(), casc.z() - collision.posZ());
      float cascptotmom = std::hypot(casc.px(), casc.py(), casc.pz());
      //
      float ctauXi = RecoDecay::getMassPDG(3312) * cascpos / (cascptotmom + 1e-13);
      float ctauOmega = RecoDecay::getMassPDG(3334) * cascpos / (cascptotmom + 1e-13);

      if (AcceptCascCandidate<DauTracks>(casc, collision.posX(), collision.posY(), collision.posZ())) {
        registry.fill(HIST("hCandidateCounter"), 2.5); // passed topo cuts
        // Fill table
        if (fRand->Rndm() < lEventScale) {
          mycascades(casc.globalIndex(), collision.posZ(), collision.centFT0M(), collision.centFV0A(), casc.sign(), casc.pt(), casc.yXi(), casc.yOmega(), casc.eta(),
                     casc.mXi(), casc.mOmega(), casc.mLambda(), casc.cascradius(), casc.v0radius(),
                     casc.casccosPA(collision.posX(), collision.posY(), collision.posZ()), casc.v0cosPA(collision.posX(), collision.posY(), collision.posZ()),
                     casc.dcapostopv(), casc.dcanegtopv(), casc.dcabachtopv(), casc.dcacascdaughters(), casc.dcaV0daughters(), casc.dcav0topv(collision.posX(), collision.posY(), collision.posZ()),
                     posdau.eta(), negdau.eta(), bachelor.eta(), posITSNhits, negITSNhits, bachITSNhits,
                     ctauXi, ctauOmega, negdau.tpcNSigmaPr(), posdau.tpcNSigmaPr(), negdau.tpcNSigmaPi(), posdau.tpcNSigmaPi(), bachelor.tpcNSigmaPi(), bachelor.tpcNSigmaKa(),
                     negdau.tofNSigmaPr(), posdau.tofNSigmaPr(), negdau.tofNSigmaPi(), posdau.tofNSigmaPi(), bachelor.tofNSigmaPi(), bachelor.tofNSigmaKa(),
                     posdau.tpcNClsFound(), negdau.tpcNClsFound(), bachelor.tpcNClsFound(),
                     posdau.tpcNClsCrossedRows(), negdau.tpcNClsCrossedRows(), bachelor.tpcNClsCrossedRows(),
                     posdau.hasTOF(), negdau.hasTOF(), bachelor.hasTOF(),
                     posdau.pt(), negdau.pt(), bachelor.pt(), -1, -1, casc.bachBaryonCosPA(), casc.bachBaryonDCAxyToPV(), flags);
        }
      }
    }
  }

  PROCESS_SWITCH(cascqaanalysis, processData, "Process Run 3 data", true);

  void processMCrec(soa::Join<aod::Collisions, aod::EvSels, aod::Mults, aod::CentFT0Ms, aod::CentFV0As>::iterator const& collision,
                    soa::Filtered<LabeledCascades> const& Cascades,
                    aod::V0sLinked const&,
                    aod::V0Datas const&,
                    DauTracks const& Tracks,
                    aod::McParticles const&)
  {
    if (!AcceptEvent(collision, Tracks, 1)) {
      return;
    }

    float lEventScale = scalefactor;

    for (const auto& casc : Cascades) {              // loop over Cascades
      registry.fill(HIST("hCandidateCounter"), 0.5); // all candidates
      // Access daughter tracks
      auto v0index = casc.v0_as<o2::aod::V0sLinked>();
      if (!(v0index.has_v0Data())) {
        return; // skip those cascades for which V0 doesn't exist
      }

      registry.fill(HIST("hCandidateCounter"), 1.5); // v0data exists

      auto v0 = v0index.v0Data();
      auto posdau = v0.posTrack_as<DauTracks>();
      auto negdau = v0.negTrack_as<DauTracks>();
      auto bachelor = casc.bachelor_as<DauTracks>();

      // ITS N hits
      int posITSNhits = 0, negITSNhits = 0, bachITSNhits = 0;
      for (unsigned int i = 0; i < 7; i++) {
        if (posdau.itsClusterMap() & (1 << i)) {
          posITSNhits++;
        }
        if (negdau.itsClusterMap() & (1 << i)) {
          negITSNhits++;
        }
        if (bachelor.itsClusterMap() & (1 << i)) {
          bachITSNhits++;
        }
      }

      uint8_t flags = 0;
      flags |= o2::aod::mycascades::EvFlags::EvINEL;
      if (collision.multNTracksPVeta1() > 0) {
        flags |= o2::aod::mycascades::EvFlags::EvINELgt0;
      }
      if (collision.multNTracksPVeta1() > 1) {
        flags |= o2::aod::mycascades::EvFlags::EvINELgt1;
      }

      // c x tau
      float cascpos = std::hypot(casc.x() - collision.posX(), casc.y() - collision.posY(), casc.z() - collision.posZ());
      float cascptotmom = std::hypot(casc.px(), casc.py(), casc.pz());
      //
      float ctauXi = RecoDecay::getMassPDG(3312) * cascpos / (cascptotmom + 1e-13);
      float ctauOmega = RecoDecay::getMassPDG(3334) * cascpos / (cascptotmom + 1e-13);

      if (AcceptCascCandidate<DauTracks>(casc, collision.posX(), collision.posY(), collision.posZ())) {
        registry.fill(HIST("hCandidateCounter"), 2.5); // passed topo cuts
        // Check mc association
        float lPDG = -1;
        float isPrimary = -1;
        if (casc.has_mcParticle()) {
          registry.fill(HIST("hCandidateCounter"), 3.5); // has associated MC particle
          auto cascmc = casc.mcParticle();
          if (TMath::Abs(cascmc.pdgCode()) == 3312 || TMath::Abs(cascmc.pdgCode()) == 3334) {
            registry.fill(HIST("hCandidateCounter"), 4.5); // associated with Xi or Omega
            lPDG = cascmc.pdgCode();
            isPrimary = cascmc.isPhysicalPrimary() ? 1 : 0;
          }
        }
        // Fill table
        if (fRand->Rndm() < lEventScale) {
          mycascades(casc.globalIndex(), collision.posZ(), collision.multFT0A() + collision.multFT0C(), collision.multFV0A(), casc.sign(), casc.pt(), casc.yXi(), casc.yOmega(), casc.eta(),
                     casc.mXi(), casc.mOmega(), casc.mLambda(), casc.cascradius(), casc.v0radius(),
                     casc.casccosPA(collision.posX(), collision.posY(), collision.posZ()), casc.v0cosPA(collision.posX(), collision.posY(), collision.posZ()),
                     casc.dcapostopv(), casc.dcanegtopv(), casc.dcabachtopv(), casc.dcacascdaughters(), casc.dcaV0daughters(), casc.dcav0topv(collision.posX(), collision.posY(), collision.posZ()),
                     posdau.eta(), negdau.eta(), bachelor.eta(), posITSNhits, negITSNhits, bachITSNhits,
                     ctauXi, ctauOmega, negdau.tpcNSigmaPr(), posdau.tpcNSigmaPr(), negdau.tpcNSigmaPi(), posdau.tpcNSigmaPi(), bachelor.tpcNSigmaPi(), bachelor.tpcNSigmaKa(),
                     negdau.tofNSigmaPr(), posdau.tofNSigmaPr(), negdau.tofNSigmaPi(), posdau.tofNSigmaPi(), bachelor.tofNSigmaPi(), bachelor.tofNSigmaKa(),
                     posdau.tpcNClsFound(), negdau.tpcNClsFound(), bachelor.tpcNClsFound(),
                     posdau.tpcNClsCrossedRows(), negdau.tpcNClsCrossedRows(), bachelor.tpcNClsCrossedRows(),
                     posdau.hasTOF(), negdau.hasTOF(), bachelor.hasTOF(),
                     posdau.pt(), negdau.pt(), bachelor.pt(), lPDG, isPrimary, casc.bachBaryonCosPA(), casc.bachBaryonDCAxyToPV(), flags);
        }
      }
    }
  }

  PROCESS_SWITCH(cascqaanalysis, processMCrec, "Process Run 3 mc, reconstructed", false);

  void processMCgen(aod::McCollision const& mcCollision,
                    aod::McParticles const& mcParticles,
                    const soa::SmallGroups<o2::soa::Join<o2::aod::Collisions, o2::aod::McCollisionLabels, o2::aod::EvSels, aod::Mults, aod::CentFT0Ms, aod::CentFV0As>>& collisions,
                    DauTracks const& Tracks)
  {
    // All generated collisions
    registry.fill(HIST("hNEventsMC"), 0.5);

    // Generated with accepted z vertex
    if (TMath::Abs(mcCollision.posZ()) > cutzvertex) {
      return;
    }
    registry.fill(HIST("hZCollisionGen"), mcCollision.posZ());
    registry.fill(HIST("hNEventsMC"), 1.5);

    // Define the type of generated MC collision
    uint8_t flagsGen = 0;
    flagsGen |= o2::aod::myMCcascades::EvFlags::EvINEL;
    registry.fill(HIST("hNEventsMC"), 2.5);
    // Generated collision is INEL>=0
    if (isINELgtNmc(mcParticles, 0)) {
      flagsGen |= o2::aod::myMCcascades::EvFlags::EvINELgt0;
      registry.fill(HIST("hNEventsMC"), 3.5);
    }
    // Generated collision is INEL>=1
    if (isINELgtNmc(mcParticles, 1)) {
      flagsGen |= o2::aod::myMCcascades::EvFlags::EvINELgt1;
      registry.fill(HIST("hNEventsMC"), 4.5);
    }

    typedef struct CollisionIndexAndType {
      int64_t index;
      uint8_t typeFlag;
    } CollisionIndexAndType;

    std::vector<CollisionIndexAndType> SelectedEvents(collisions.size());
    std::vector<int64_t> NumberOfContributors;
    int nevts = 0;
    int nAssocColl = 0;
    for (const auto& collision : collisions) {
      CollisionIndexAndType collWithType = {0, 0x0};
      if (!AcceptEvent(collision, Tracks, 0)) {
        continue;
      }
      collWithType.index = collision.mcCollision_as<aod::McCollisions>().globalIndex();
      collWithType.typeFlag |= o2::aod::myMCcascades::EvFlags::EvINEL;

      if (collision.multNTracksPVeta1() > 0) {
        collWithType.typeFlag |= o2::aod::myMCcascades::EvFlags::EvINELgt0;
      }
      if (collision.multNTracksPVeta1() > 1) {
        collWithType.typeFlag |= o2::aod::myMCcascades::EvFlags::EvINELgt1;
      }

      SelectedEvents[nevts++] = collWithType;
      if (collision.mcCollision_as<aod::McCollisions>().globalIndex() == mcCollision.globalIndex()) {
        nAssocColl++;
        NumberOfContributors.push_back(collision.numContrib());
      }
    }
    SelectedEvents.resize(nevts);
    registry.fill(HIST("hNAssocMCCollisions"), nAssocColl);
    if (NumberOfContributors.size() == 2) {
      std::sort(NumberOfContributors.begin(), NumberOfContributors.end());
      registry.fill(HIST("hNContributorsCorrelation"), NumberOfContributors[0], NumberOfContributors[1]);
    }

    auto isAssocToINEL = [&mcCollision](CollisionIndexAndType i) { return (i.index == mcCollision.globalIndex()) && ((i.typeFlag & o2::aod::myMCcascades::EvFlags::EvINEL) == o2::aod::myMCcascades::EvFlags::EvINEL); };
    auto isAssocToINELgt0 = [&mcCollision](CollisionIndexAndType i) { return (i.index == mcCollision.globalIndex()) && ((i.typeFlag & o2::aod::myMCcascades::EvFlags::EvINELgt0) == o2::aod::myMCcascades::EvFlags::EvINELgt0); };
    auto isAssocToINELgt1 = [&mcCollision](CollisionIndexAndType i) { return (i.index == mcCollision.globalIndex()) && ((i.typeFlag & o2::aod::myMCcascades::EvFlags::EvINELgt1) == o2::aod::myMCcascades::EvFlags::EvINELgt1); };
    // at least 1 selected reconstructed INEL event has the same global index as mcCollision
    const auto evtReconstructedAndINEL = std::find_if(SelectedEvents.begin(), SelectedEvents.end(), isAssocToINEL) != SelectedEvents.end();
    // at least 1 selected reconstructed INELgt0 event has the same global index as mcCollision
    const auto evtReconstructedAndINELgt0 = std::find_if(SelectedEvents.begin(), SelectedEvents.end(), isAssocToINELgt0) != SelectedEvents.end();
    // at least 1 selected reconstructed INELgt1 event has the same global index as mcCollision
    const auto evtReconstructedAndINELgt1 = std::find_if(SelectedEvents.begin(), SelectedEvents.end(), isAssocToINELgt1) != SelectedEvents.end();

    uint8_t flagsAssoc = 0;
    if (evtReconstructedAndINEL) {
      flagsAssoc |= o2::aod::myMCcascades::EvFlags::EvINEL;
      registry.fill(HIST("hNEventsMC"), 5.5);
    }
    if (evtReconstructedAndINELgt0) {
      flagsAssoc |= o2::aod::myMCcascades::EvFlags::EvINELgt0;
    }
    if (evtReconstructedAndINELgt1) {
      flagsAssoc |= o2::aod::myMCcascades::EvFlags::EvINELgt1;
    }

    // Particle counting in FITFT0: -3.3<η<-2.1; 3.5<η<4.9
    uint16_t nchFT0 = 0;
    for (auto& mcParticle : mcParticles) {
      if (!mcParticle.isPhysicalPrimary()) {
        continue;
      }
      const auto& pdgInfo = pdgDB->GetParticle(mcParticle.pdgCode());
      if (!pdgInfo) {
        continue;
      }
      if (pdgInfo->Charge() == 0) {
        continue;
      }
      if (mcParticle.eta() < -3.3 || mcParticle.eta() > 4.9 || (mcParticle.eta() > -2.1 && mcParticle.eta() < 3.5)) {
        continue; // select on T0M Nch region
      }
      nchFT0++; // increment
    }

    for (const auto& mcParticle : mcParticles) {
      float sign = 0;
      if (mcParticle.pdgCode() == -3312 || mcParticle.pdgCode() == -3334) {
        sign = 1;
      } else if (mcParticle.pdgCode() == 3312 || mcParticle.pdgCode() == 3334) {
        sign = -1;
      } else {
        continue;
      }
      myMCcascades(mcCollision.globalIndex(),
                   mcCollision.posZ(), sign, mcParticle.pdgCode(),
                   mcParticle.y(), mcParticle.eta(), mcParticle.phi(), mcParticle.pt(),
                   mcParticle.isPhysicalPrimary(), nAssocColl, nchFT0,
                   flagsAssoc,
                   flagsGen);
    }
  }
  PROCESS_SWITCH(cascqaanalysis, processMCgen, "Process Run 3 mc, genereated", false);
};

struct myCascades {

  HistogramRegistry registry{"registry"};

  // QA
  Configurable<bool> doQA{"doQA", 0, "Fill QA histograms"};

  void init(InitContext const&)
  {
    TString PGDlabels[3] = {"Unknown", "3312", "3334"};
    registry.add("hPt", "hPt", {HistType::kTH1F, {{100, 0.0f, 10.0f}}});
    registry.add("hMassXi", "hMassXi", {HistType::kTH1F, {{1000, 1.0f, 2.0f}}});
    registry.add("hMassOmega", "hMassOmega", {HistType::kTH1F, {{1000, 1.0f, 2.0f}}});
    if (doQA) {
      registry.add("hCascRadius", "hCascRadius", {HistType::kTH1D, {{100, 0.0f, 40.0f}}});
      registry.add("hV0Radius", "hV0Radius", {HistType::kTH1D, {{100, 0.0f, 40.0f}}});
      registry.add("hCascCosPA", "hCascCosPA", {HistType::kTH1F, {{100, 0.9f, 1.0f}}});
      registry.add("hV0CosPA", "hV0CosPA", {HistType::kTH1F, {{100, 0.9f, 1.0f}}});
      registry.add("hDCANegToPV", "hDCANegToPV", {HistType::kTH1F, {{100, -1.0f, 1.0f}}});
      registry.add("hDCAPosToPV", "hDCAPosToPV", {HistType::kTH1F, {{100, -1.0f, 1.0f}}});
      registry.add("hDCABachToPV", "hDCABachToPV", {HistType::kTH1F, {{100, -1.0f, 1.0f}}});
      registry.add("hDCACascDaughters", "hDCACascDaughters", {HistType::kTH1F, {{55, 0.0f, 2.20f}}});
      registry.add("hDCAV0Daughters", "hDCAV0Daughters", {HistType::kTH1F, {{55, 0.0f, 2.20f}}});
      registry.add("hCtauXi", "hCtauXi", {HistType::kTH1F, {{100, 0.0f, 40.0f}}});
      registry.add("hCtauOmega", "hCtauOmega", {HistType::kTH1F, {{100, 0.0f, 40.0f}}});
      registry.add("hTPCNSigmaPosPi", "hTPCNSigmaPosPi", {HistType::kTH1F, {{100, -10.0f, 10.0f}}});
      registry.add("hTPCNSigmaNegPi", "hTPCNSigmaNegPi", {HistType::kTH1F, {{100, -10.0f, 10.0f}}});
      registry.add("hTPCNSigmaPosPr", "hTPCNSigmaPosPr", {HistType::kTH1F, {{100, -10.0f, 10.0f}}});
      registry.add("hTPCNSigmaNegPr", "hTPCNSigmaNegPr", {HistType::kTH1F, {{100, -10.0f, 10.0f}}});
      registry.add("hTPCNSigmaBachPi", "hTPCNSigmaBachPi", {HistType::kTH1F, {{100, -10.0f, 10.0f}}});
      registry.add("hTOFNSigmaPosPi", "hTOFNSigmaPosPi", {HistType::kTH1F, {{100, -10.0f, 10.0f}}});
      registry.add("hTOFNSigmaNegPi", "hTOFNSigmaNegPi", {HistType::kTH1F, {{100, -10.0f, 10.0f}}});
      registry.add("hTOFNSigmaPosPr", "hTOFNSigmaPosPr", {HistType::kTH1F, {{100, -10.0f, 10.0f}}});
      registry.add("hTOFNSigmaNegPr", "hTOFNSigmaNegPr", {HistType::kTH1F, {{100, -10.0f, 10.0f}}});
      registry.add("hTOFNSigmaBachPi", "hTOFNSigmaBachPi", {HistType::kTH1F, {{100, -10.0f, 10.0f}}});
      registry.add("hPosITSHits", "hPosITSHits", {HistType::kTH1F, {{8, -0.5f, 7.5f}}});
      registry.add("hNegITSHits", "hNegITSHits", {HistType::kTH1F, {{8, -0.5f, 7.5f}}});
      registry.add("hBachITSHits", "hBachITSHits", {HistType::kTH1F, {{8, -0.5f, 7.5f}}});
      registry.add("hIsPrimary", "hIsPrimary", {HistType::kTH1F, {{3, -1.5f, 1.5f}}});
      registry.add("hPDGcode", "hPDGcode", {HistType::kTH1F, {{3, -1.5f, 1.5f}}});
      for (Int_t n = 1; n <= registry.get<TH1>(HIST("hPDGcode"))->GetNbinsX(); n++) {
        registry.get<TH1>(HIST("hPDGcode"))->GetXaxis()->SetBinLabel(n, PGDlabels[n - 1]);
      }
      registry.add("hBachBaryonCosPA", "hBachBaryonCosPA", {HistType::kTH1F, {{100, 0.0f, 1.0f}}});
      registry.add("hBachBaryonDCAxyToPV", "hBachBaryonDCAxyToPV", {HistType::kTH1F, {{300, -3.0f, 3.0f}}});
    }
  }

  void process(aod::MyCascades const& mycascades)
  {
    for (auto& candidate : mycascades) {

      registry.fill(HIST("hMassXi"), candidate.massxi());
      registry.fill(HIST("hMassOmega"), candidate.massomega());
      registry.fill(HIST("hPt"), candidate.pt());
      if (doQA) {
        registry.fill(HIST("hCascRadius"), candidate.cascradius());
        registry.fill(HIST("hV0Radius"), candidate.v0radius());
        registry.fill(HIST("hCascCosPA"), candidate.casccospa());
        registry.fill(HIST("hV0CosPA"), candidate.v0cospa());
        registry.fill(HIST("hDCANegToPV"), candidate.dcanegtopv());
        registry.fill(HIST("hDCAPosToPV"), candidate.dcapostopv());
        registry.fill(HIST("hDCABachToPV"), candidate.dcabachtopv());
        registry.fill(HIST("hDCACascDaughters"), candidate.dcacascdaughters());
        registry.fill(HIST("hDCAV0Daughters"), candidate.dcav0daughters());
        registry.fill(HIST("hCtauXi"), candidate.ctauxi());
        registry.fill(HIST("hCtauOmega"), candidate.ctauomega());
        registry.fill(HIST("hTPCNSigmaPosPi"), candidate.ntpcsigmapospi());
        registry.fill(HIST("hTPCNSigmaNegPi"), candidate.ntpcsigmanegpi());
        registry.fill(HIST("hTPCNSigmaPosPr"), candidate.ntpcsigmapospr());
        registry.fill(HIST("hTPCNSigmaNegPr"), candidate.ntpcsigmanegpr());
        registry.fill(HIST("hTPCNSigmaBachPi"), candidate.ntpcsigmabachpi());
        registry.fill(HIST("hTOFNSigmaPosPi"), candidate.ntofsigmapospi());
        registry.fill(HIST("hTOFNSigmaNegPi"), candidate.ntofsigmanegpi());
        registry.fill(HIST("hTOFNSigmaPosPr"), candidate.ntofsigmapospr());
        registry.fill(HIST("hTOFNSigmaNegPr"), candidate.ntofsigmanegpr());
        registry.fill(HIST("hTOFNSigmaBachPi"), candidate.ntofsigmabachpi());
        registry.fill(HIST("hPosITSHits"), candidate.positshits());
        registry.fill(HIST("hNegITSHits"), candidate.negitshits());
        registry.fill(HIST("hBachITSHits"), candidate.bachitshits());
        registry.fill(HIST("hIsPrimary"), candidate.isPrimary());
        registry.fill(HIST("hBachBaryonCosPA"), candidate.bachBaryonCosPA());
        registry.fill(HIST("hBachBaryonDCAxyToPV"), candidate.bachBaryonDCAxyToPV());

        if (TMath::Abs(candidate.mcPdgCode()) == 3312 || TMath::Abs(candidate.mcPdgCode()) == 3334) {
          registry.fill(HIST("hPDGcode"), TMath::Abs(candidate.mcPdgCode()) == 3312 ? 0 : 1); // 0 if Xi, 1 if Omega
        } else {
          registry.fill(HIST("hPDGcode"), -1); // -1 if unknown
        }
      }
    }
  }
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{
    adaptAnalysisTask<cascqaanalysis>(cfgc, TaskName{"lf-cascqaanalysis"}),
    adaptAnalysisTask<myCascades>(cfgc, TaskName{"lf-mycascades"})};
}
