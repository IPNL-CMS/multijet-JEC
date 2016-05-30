#include <BasicJetVars.hpp>

#include <mensura/core/Dataset.hpp>
#include <mensura/core/FileInPath.hpp>
#include <mensura/core/RunManager.hpp>

#include <mensura/extensions/JetFunctorFilter.hpp>
#include <mensura/extensions/PileUpWeight.hpp>
#include <mensura/extensions/TFileService.hpp>
#include <mensura/extensions/WeightCollector.hpp>

#include <mensura/PECReader/PECInputData.hpp>
#include <mensura/PECReader/PECJetMETReader.hpp>
#include <mensura/PECReader/PECPileUpReader.hpp>
#include <mensura/PECReader/PECTriggerFilter.hpp>

#include <cstdlib>
#include <iostream>
#include <list>


using namespace std;


enum class DatasetGroup
{
    Data,
    MC
};


int main(int argc, char **argv)
{
    // Parse arguments
    if (argc != 2)
    {
        cerr << "Usage: singlejet-test dataset-group\n";
        return EXIT_FAILURE;
    }
    
    
    string dataGroupText(argv[1]);
    DatasetGroup dataGroup;
    
    if (dataGroupText == "data")
        dataGroup = DatasetGroup::Data;
    else if (dataGroupText == "mc")
        dataGroup = DatasetGroup::MC;
    else
    {
        cerr << "Cannot recognize dataset group \"" << dataGroupText << "\".\n";
        return EXIT_FAILURE;
    }
    
    
    
    // Input datasets
    list<Dataset> datasets;
    string const datasetsDir("/gridgroup/cms/popov/Analyses/JetMET/2016.04.11_Grid-campaign/"
      "Results/");
    
    if (dataGroup == DatasetGroup::Data)
    {
        datasets.emplace_back(Dataset({Dataset::Process::ppData, Dataset::Process::pp13TeV},
          Dataset::Generator::Nature, Dataset::ShowerGenerator::Nature));
        datasets.back().AddFile(datasetsDir + "JetHT-Run2015*.root");
    }
    else
    {
        datasets.emplace_back(Dataset(Dataset::Process::QCD, Dataset::Generator::MadGraph,
          Dataset::ShowerGenerator::Pythia));
        datasets.back().AddFile(datasetsDir + "QCD-Ht-100-200-mg_3.1.1_Kah.root",
          27540000, 82095800);
        datasets.back().AddFile(datasetsDir + "QCD-Ht-200-300-mg_3.1.1_ilS.root",
          1717000, 18784379);
        datasets.back().AddFile(datasetsDir + "QCD-Ht-300-500-mg_3.1.1_UpJ_p*.root",
          351300, 16909004);
        datasets.back().AddFile(datasetsDir + "QCD-Ht-500-700-mg_3.1.1_XWW_p*.root",
          31630, 19665695);
        datasets.back().AddFile(datasetsDir + "QCD-Ht-700-1000-mg_3.1.1_mtk_p*.root",
          6802, 13801981);
        datasets.back().AddFile(datasetsDir + "QCD-Ht-1000-1500-mg_3.1.1_MoZ.root",
          1206, 5049267);
        datasets.back().AddFile(datasetsDir + "QCD-Ht-1500-2000-mg_3.1.1_mIr.root",
          120.4, 3939077);
        datasets.back().AddFile(datasetsDir + "QCD-Ht-2000-inf-mg_3.1.1_DTg.root",
          25.25, 1981228);
    }
    
    
    // Triggers
    list<TriggerRange> triggerRanges;
    triggerRanges.emplace_back(0, -1, "PFJet450", 2312.360, "PFJet450");
    
    
    // Add an additional location to seach for data files
    char const *installPath = getenv("MULTIJET_JEC_INSTALL");
    
    if (not installPath)
    {
        cerr << "Mandatory environmental variable MULTIJET_JEC_INSTALL is not defined.\n";
        return EXIT_FAILURE;
    }
    
    FileInPath::AddLocation(string(installPath) + "/data/");
    
    
    // Construct the run manager
    RunManager manager(datasets.begin(), datasets.end());
    
    
    // Register services
    manager.RegisterService(new TFileService("output_test/%"));
    
    
    // Register plugins
    manager.RegisterPlugin(new PECInputData);
    manager.RegisterPlugin(BuildPECTriggerFilter((dataGroup == DatasetGroup::Data),
      triggerRanges));
    
    PECJetMETReader *jetReader = new PECJetMETReader;
    jetReader->SetSelection(30., 2.4);
    jetReader->ConfigureLeptonCleaning("");  // Disabled
    manager.RegisterPlugin(jetReader);
    
    manager.RegisterPlugin(new JetFunctorFilter([](Jet const &j){return (j.Pt() > 1.e3);}, 1.));
    manager.RegisterPlugin(new PECPileUpReader);
    manager.RegisterPlugin(new PileUpWeight("pileup_Run2015CD_PFJet450_finebin.root",
      "simPUProfiles_76X.root", 0.05));
    
    
    // Finally, the plugin to calculate some observables
    manager.RegisterPlugin(new BasicJetVars);
    
    
    // Process the datasets
    manager.Process(6);
    
    
    return EXIT_SUCCESS;
}