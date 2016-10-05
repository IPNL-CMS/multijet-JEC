#include <BalanceVars.hpp>
#include <DumpEventID.hpp>
#include <DynamicTriggerFilter.hpp>
#include <FirstJetFilter.hpp>
#include <PileUpVars.hpp>
#include <RecoilBuilder.hpp>
#include <TriggerBin.hpp>

#include <mensura/core/Dataset.hpp>
#include <mensura/core/FileInPath.hpp>
#include <mensura/core/RunManager.hpp>
#include <mensura/core/SystService.hpp>

#include <mensura/extensions/DatasetBuilder.hpp>
#include <mensura/extensions/JetCorrectorService.hpp>
#include <mensura/extensions/JetFilter.hpp>
#include <mensura/extensions/JetMETUpdate.hpp>
#include <mensura/extensions/PileUpWeight.hpp>
#include <mensura/extensions/TFileService.hpp>

#include <mensura/PECReader/PECGenJetMETReader.hpp>
#include <mensura/PECReader/PECInputData.hpp>
#include <mensura/PECReader/PECJetMETReader.hpp>
#include <mensura/PECReader/PECPileUpReader.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include <cstdlib>
#include <iostream>
#include <list>
#include <sstream>
#include <regex>


using namespace std;
namespace po = boost::program_options;


enum class DatasetGroup
{
    Data,
    MC
};


int main(int argc, char **argv)
{
    // Parse arguments
    po::options_description options("Allowed options");
    options.add_options()
      ("help,h", "Prints help message")
      ("datasetGroup", po::value<string>(), "Dataset group (required)")
      ("pt-cuts,c", po::value<string>(), "Jet pt cuts")
      ("syst,s", po::value<string>(), "Systematic shift");
    //^ This is some severe abuse of C++ syntax...
    
    po::positional_options_description positionalOptions;
    positionalOptions.add("datasetGroup", 1);
    
    po::variables_map optionsMap;
    
    po::store(
      po::command_line_parser(argc, argv).options(options).positional(positionalOptions).run(),
      optionsMap);
    po::notify(optionsMap);
    
    if (optionsMap.count("help"))
    {
        cerr << "Produces tuples with observables for the multijet method.\n";
        cerr << "Usage: multijet datasetGroup [options]\n";
        cerr << options << endl;
        return EXIT_FAILURE;
    }
    
    
    if (not optionsMap.count("datasetGroup"))
    {
        cerr << "Required argument datasetGroup is missing.\n";
        return EXIT_FAILURE;
    }
    
    
    string dataGroupText(optionsMap["datasetGroup"].as<string>());
    DatasetGroup dataGroup;
    
    if (dataGroupText == "data")
        dataGroup = DatasetGroup::Data;
    else if (dataGroupText == "mc" or dataGroupText == "sim")
        dataGroup = DatasetGroup::MC;
    else
    {
        cerr << "Cannot recognize dataset group \"" << dataGroupText << "\".\n";
        return EXIT_FAILURE;
    }
    
    
    // Parse list of jet pt cuts used to construct the recoil. Their values are assumed to be
    //integer.
    list<unsigned> jetPtCuts;
    
    if (optionsMap.count("pt-cuts"))
    {
        istringstream cutList(optionsMap["pt-cuts"].as<string>());
        string cut;
        
        while (getline(cutList, cut, ','))
            jetPtCuts.emplace_back(stod(cut));
    }
    else
        jetPtCuts = {30};
    
    
    // Parse requested systematic uncertainty
    string systType("None");
    SystService::VarDirection systDirection = SystService::VarDirection::Undefined;
    
    if (optionsMap.count("syst"))
    {
        string systArg(optionsMap["syst"].as<string>());
        boost::to_lower(systArg);
        
        std::regex systRegex("(jec|jer|metuncl)[-_]?(up|down)", std::regex::extended);
        std::smatch matchResult;
        
        if (not std::regex_match(systArg, matchResult, systRegex))
        {
            cerr << "Cannot recognize systematic variation \"" << systArg << "\".\n";
            return EXIT_FAILURE;
        }
        else
        {
            if (matchResult[1] == "jec")
                systType = "JEC";
            else if (matchResult[1] == "jer")
                systType = "JER";
            else if (matchResult[1] == "metuncl")
                systType = "METUncl";
            
            if (matchResult[2] == "up")
                systDirection = SystService::VarDirection::Up;
            else if (matchResult[2] == "down")
                systDirection = SystService::VarDirection::Down;
        }
    }
    
    
    
    // Input datasets
    list<Dataset> datasets;
    DatasetBuilder datasetBuilder("/gridgroup/cms/popov/Analyses/JetMET/"
      "2016.09.10_Grid-campaign-80X/Results/samples_v1.json");
    
    if (dataGroup == DatasetGroup::Data)
        datasets = datasetBuilder({
          /*"JetHT-Run2016B_SRE", "JetHT-Run2016C_wta", "JetHT-Run2016D_xZC",*/
          /*"JetHT-Run2016E_QOo", "JetHT-Run2016F_QDF",*/
          "JetHT-Run2016G_oYl"});
    else
        datasets = datasetBuilder({"QCD-Ht-100-200-mg_dvx", "QCD-Ht-200-300-mg_rrz",
          "QCD-Ht-300-500-mg_Mia", "QCD-Ht-500-700-mg_Zth", "QCD-Ht-700-1000-mg_aYC",
          "QCD-Ht-1000-1500-mg_sDu", "QCD-Ht-1500-2000-mg_szQ", "QCD-Ht-2000-inf-mg_LTF"});
    
    
    // Add an additional locations to seach for data files
    char const *installPath = getenv("MULTIJET_JEC_INSTALL");
    
    if (not installPath)
    {
        cerr << "Mandatory environmental variable MULTIJET_JEC_INSTALL is not defined.\n";
        return EXIT_FAILURE;
    }
    
    FileInPath::AddLocation(string(installPath) + "/data/");
    FileInPath::AddLocation("/gridgroup/cms/popov/Analyses/JetMET/2016.09.17_MJB-24invfb/Tuples/");
    
    
    // Construct the run manager
    RunManager manager(datasets.begin(), datasets.end());
    
    
    // Register services and plugins
    ostringstream outputNameStream;
    outputNameStream << "output/" << ((dataGroup == DatasetGroup::Data) ? "data" : "sim");
    
    if (systType != "None")
        outputNameStream << "_" << systType << "_" <<
          ((systDirection == SystService::VarDirection::Up) ? "up" : "down");
    
    outputNameStream << "/%";
    
    manager.RegisterService(new TFileService(outputNameStream.str()));
    
    manager.RegisterPlugin(new PECInputData);
    manager.RegisterPlugin(new PECPileUpReader);
    
    if (dataGroup == DatasetGroup::Data)
    {
        // Read original jets and MET, which have outdated corrections
        PECJetMETReader *jetmetReader = new PECJetMETReader("OrigJetMET");
        jetmetReader->ConfigureLeptonCleaning("");  // Disabled
        jetmetReader->ReadRawMET();
        manager.RegisterPlugin(jetmetReader);
        
        
        // Corrections to be applied to jets. They will also be propagated to MET.
        JetCorrectorService *jetCorrFull = new JetCorrectorService("JetCorrFull");
        jetCorrFull->SetJEC({"Spring16_25nsV6_DATA_L1FastJet_AK4PFchs.txt",
          "Spring16_25nsV6_DATA_L2Relative_AK4PFchs.txt",
          "Spring16_25nsV6_DATA_L3Absolute_AK4PFchs.txt",
          // "Run2016BCD_Spring16_25ns_MPF_LOGLIN_L2Residual_pythia8_v7_AK4PFchs.txt"});
          // "Run2016EF_Spring16_25ns_MPF_LOGLIN_L2Residual_pythia8_v7_AK4PFchs.txt"});
          "Run2016G_Spring16_25ns_MPF_LOGLIN_L2Residual_pythia8_v7_AK4PFchs.txt"});
        manager.RegisterService(jetCorrFull);
        
        // L1 corrections to be used in T1 MET corrections
        JetCorrectorService *jetCorrL1 = new JetCorrectorService("JetCorrL1");
        jetCorrL1->SetJEC({"Spring16_25nsV6_DATA_L1RC_AK4PFchs.txt"});
        manager.RegisterService(jetCorrL1);
        
        
        // Recorrect jets and apply T1 MET corrections to raw MET
        JetMETUpdate *jetmetUpdater = new JetMETUpdate;
        jetmetUpdater->SetJetCorrection("JetCorrFull");
        jetmetUpdater->SetJetCorrectionForMET("JetCorrFull", "JetCorrL1", "", "");
        jetmetUpdater->UseRawMET();
        manager.RegisterPlugin(jetmetUpdater);
    }
    else
    {
        manager.RegisterService(new SystService(systType, systDirection));
        manager.RegisterPlugin(new PECGenJetMETReader);
        
        
        // Read original jets and MET
        PECJetMETReader *jetmetReader = new PECJetMETReader("OrigJetMET");
        jetmetReader->ReadRawMET();
        jetmetReader->ConfigureLeptonCleaning("");  // Disabled
        jetmetReader->SetGenJetReader();  // Default one
        manager.RegisterPlugin(jetmetReader);
        
        
        // Corrections to be applied to jets and also to be propagated to MET. Although original
        //jets in simulation already have up-to-date corrections, they will be reapplied in order
        //to have a consistent impact on MET from the stochastic JER smearing. The random-number
        //seed for the smearing is fixed for the sake of reproducibility.
        JetCorrectorService *jetCorrFull = new JetCorrectorService("JetCorrFull");
        jetCorrFull->SetJEC({"Spring16_25nsV6_MC_L1FastJet_AK4PFchs.txt",
          "Spring16_25nsV6_MC_L2Relative_AK4PFchs.txt",
          "Spring16_25nsV6_MC_L3Absolute_AK4PFchs.txt"});
        jetCorrFull->SetJER("Spring16_25nsV6_MC_SF_AK4PFchs.txt",
          "Spring16_25nsV6_MC_PtResolution_AK4PFchs.txt", 4913);
        jetCorrFull->SetJECUncertainty("Spring16_25nsV6_MC_Uncertainty_AK4PFchs.txt");
        manager.RegisterService(jetCorrFull);
        
        // L1 corrections to be used in T1 MET corrections
        JetCorrectorService *jetCorrL1 = new JetCorrectorService("JetCorrL1");
        jetCorrL1->SetJEC({"Spring16_25nsV6_MC_L1RC_AK4PFchs.txt"});
        manager.RegisterService(jetCorrL1);
        
        
        // Recorrect jets and apply T1 MET corrections to raw MET
        JetMETUpdate *jetmetUpdater = new JetMETUpdate;
        jetmetUpdater->SetJetCorrection("JetCorrFull");
        jetmetUpdater->SetJetCorrectionForMET("JetCorrFull", "JetCorrL1", "", "");
        jetmetUpdater->UseRawMET();
        manager.RegisterPlugin(jetmetUpdater);
    }
    
    manager.RegisterPlugin(new TriggerBin({200., 250., 300., 370., 450., 510.}));
    manager.RegisterPlugin(new FirstJetFilter(0., 1.3));
    
    if (dataGroup == DatasetGroup::Data)
    {
        // Integrated luminosities are not used when collision data are processed. Only their
        //placeholders are given.
        manager.RegisterPlugin(new DynamicTriggerFilter({{"PFJet140", 1}, {"PFJet200", 1},
          {"PFJet260", 1}, {"PFJet320", 1}, {"PFJet400", 1}, {"PFJet450", 1}}));
    }
    
    
    for (auto const &jetPtCut: jetPtCuts)
    {
        string const ptCutText(to_string(jetPtCut));
        
        RecoilBuilder *recoilBuilder = new RecoilBuilder("RecoilBuilderPt"s + ptCutText, jetPtCut);
        recoilBuilder->SetBalanceSelection(0.6, 0.3, 1.);
        recoilBuilder->SetBetaPtFraction(0.05);
        
        if (dataGroup == DatasetGroup::Data)
            manager.RegisterPlugin(recoilBuilder, {"TriggerFilter"});
        else
            manager.RegisterPlugin(recoilBuilder, {"FirstJetFilter"});
        
        manager.RegisterPlugin(new DumpEventID);
        
        BalanceVars *balanceVars = new BalanceVars("BalanceVarsPt"s + ptCutText);
        balanceVars->SetRecoilBuilderName(recoilBuilder->GetName());
        manager.RegisterPlugin(balanceVars);
        
        manager.RegisterPlugin(new PileUpVars("PileUpVarsPt"s + ptCutText));
    }
    
    
    // Process the datasets
    manager.Process(10);
    
    std::cout << '\n';
    manager.PrintSummary();
    
    
    return EXIT_SUCCESS;
}
