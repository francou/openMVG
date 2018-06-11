// This file is part of OpenMVG, an Open Multiple View Geometry C++ library.

// Copyright (c) 2012, 2013, 2015 Pierre MOULON.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "openMVG/matching/indMatch.hpp"
#include "openMVG/matching/indMatch_utils.hpp"
#include "openMVG/matching/svg_matches.hpp"
#include "openMVG/image/image_io.hpp"
#include "openMVG/sfm/pipelines/sfm_features_provider.hpp"
#include "openMVG/sfm/pipelines/sfm_matches_provider.hpp"
#include "openMVG/sfm/sfm_data.hpp"
#include "openMVG/sfm/sfm_data_io.hpp"

#include "third_party/cmdLine/cmdLine.h"
#include "third_party/stlplus3/filesystemSimplified/file_system.hpp"
#include "third_party/progress/progress_display.hpp"

#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>
#include <map>

using namespace openMVG;
using namespace openMVG::features;
using namespace openMVG::matching;
using namespace openMVG::sfm;

int main(int argc, char ** argv)
{
  CmdLine cmd;

  std::string sSfM_Data_Filename;
  std::string sMatchesDir;
  std::string sMatchFile;
  std::string sOutDir = "";

  cmd.add( make_option('i', sSfM_Data_Filename, "input_file") );
  cmd.add( make_option('d', sMatchesDir, "matchdir") );
  cmd.add( make_option('m', sMatchFile, "matchfile") );
  cmd.add( make_option('o', sOutDir, "outdir") );

  try {
      if (argc == 1) throw std::string("Invalid command line parameter.");
      cmd.process(argc, argv);
  } catch (const std::string& s) {
      std::cerr << "Export pairwise matches.\nUsage: " << argv[0] << "\n"
      << "[-i|--input_file file] path to a SfM_Data scene\n"
      << "[-d|--matchdir path]\n"
      << "[-m|--sMatchFile filename]\n"
      << "[-o|--outdir path]\n"
      << std::endl;

      std::cerr << s << std::endl;
      return EXIT_FAILURE;
  }

  if (sOutDir.empty())  {
    std::cerr << "\nIt is an invalid output directory" << std::endl;
    return EXIT_FAILURE;
  }
  if (sMatchesDir.empty()) {
    std::cerr << "\nmatchdir cannot be an empty option" << std::endl;
    return EXIT_FAILURE;
  }
  if (sMatchFile.empty()) {
    std::cerr << "\nmatchfile cannot be an empty option" << std::endl;
    return EXIT_FAILURE;
  }

  //---------------------------------------
  // Read SfM Scene (image view names)
  //---------------------------------------
  SfM_Data sfm_data;
  if (!Load(sfm_data, sSfM_Data_Filename, ESfM_Data(VIEWS|INTRINSICS))) {
    std::cerr << std::endl
      << "The input SfM_Data file \""<< sSfM_Data_Filename << "\" cannot be read." << std::endl;
    return EXIT_FAILURE;
  }

  //---------------------------------------
  // Load SfM Scene regions
  //---------------------------------------
  // Init the regions_type from the image describer file (used for image regions extraction)
  const std::string sImage_describer = stlplus::create_filespec(sMatchesDir, "image_describer", "json");
  std::unique_ptr<Regions> regions_type = Init_region_type_from_file(sImage_describer);
  if (!regions_type)
  {
    std::cerr << "Invalid: "
      << sImage_describer << " regions type file." << std::endl;
    return EXIT_FAILURE;
  }

  // Read the features
  std::shared_ptr<Features_Provider> feats_provider = std::make_shared<Features_Provider>();
  if (!feats_provider->load(sfm_data, sMatchesDir, regions_type)) {
    std::cerr << std::endl
      << "Invalid features." << std::endl;
    return EXIT_FAILURE;
  }
  std::shared_ptr<Matches_Provider> matches_provider = std::make_shared<Matches_Provider>();
  if (!matches_provider->load(sfm_data, sMatchFile)) {
    std::cerr << "\nInvalid matches file." << std::endl;
    return EXIT_FAILURE;
  }

  // ------------
  // For each pair, export the matches
  // ------------

  stlplus::folder_create(sOutDir);
    // Create the Orientation folder
  const std::string homol_str = "Homol";
  const std::string & sOutHomolFolder = stlplus::folder_append_separator(sOutDir) + homol_str;
  if (!stlplus::is_folder(sOutHomolFolder)){
    stlplus::folder_create(sOutHomolFolder);
  }
  std::cout << "\n Export pairwise matches" << std::endl;
  
  const Pair_Set pairs = matches_provider->getPairs();
  C_Progress_display my_progress_bar( pairs.size() );
  for (Pair_Set::const_iterator iter = pairs.begin();
    iter != pairs.end();
    ++iter, ++my_progress_bar)
  {
    const size_t I = iter->first;
    const size_t J = iter->second;

    const View * view_I = sfm_data.GetViews().at(I).get();
    const std::string filename_I = stlplus::filename_part(view_I->s_Img_path);
    const View * view_J = sfm_data.GetViews().at(J).get();
    const std::string filename_J = stlplus::filename_part(view_J->s_Img_path);

    // Get corresponding matches
    const std::vector<IndMatch> & vec_FilteredMatches =
      matches_provider->pairWise_matches_.at(*iter);

    const int num_2 = 2;
    const double num_1 = 1;
    int n_obs = vec_FilteredMatches.size(); //XXX OK?
    
    if (n_obs == 0)
      continue;

    const std::string & sImgI_folder_path = stlplus::folder_append_separator(sOutHomolFolder)+"Pastis"+ filename_I;
    const std::string & sImgJ_folder_path = stlplus::folder_append_separator(sOutHomolFolder)+"Pastis"+ filename_J;
    const std::string & sImgJ_I_file_path = stlplus::folder_append_separator(sImgJ_folder_path)+""+ filename_I+".dat";
    const std::string & sImgI_J_file_path = stlplus::folder_append_separator(sImgI_folder_path)+""+ filename_J+".dat";

    if (!stlplus::is_folder(sImgI_folder_path)){
      stlplus::folder_create(sImgI_folder_path);
    }
    if (!stlplus::is_folder(sImgJ_folder_path)){
      stlplus::folder_create(sImgJ_folder_path);
    }

    // Write header to each file
    std::ofstream file_img_I_J(sImgI_J_file_path.c_str(),std::ofstream::binary);
    file_img_I_J.write(reinterpret_cast<const char *>(&num_2),sizeof(num_2));
    file_img_I_J.write(reinterpret_cast<const char *>(&n_obs),sizeof(n_obs));
    std::ofstream file_img_J_I(sImgJ_I_file_path.c_str(),std::ofstream::binary);
    file_img_J_I.write(reinterpret_cast<const char *>(&num_2),sizeof(num_2));
    file_img_J_I.write(reinterpret_cast<const char *>(&n_obs),sizeof(n_obs));
    
    const auto& left_features = feats_provider->getFeatures(view_I->id_view);
    const auto& right_features = feats_provider->getFeatures(view_J->id_view);
    for (const auto match_it : vec_FilteredMatches) {
      // Get back linked features
      const features::PointFeature & L = left_features[match_it.i_];
      const features::PointFeature & R = right_features[match_it.j_];
      const double im_I_x =  L.x();
      const double im_I_y =  L.y();
      const double im_J_x =  R.x();
      const double im_J_y =  R.y();
      // Write the feature pair
      file_img_I_J.write(reinterpret_cast<const char *>(&num_2),sizeof(num_2));
      file_img_I_J.write(reinterpret_cast<const char *>(&num_1),sizeof(num_1));
      file_img_J_I.write(reinterpret_cast<const char *>(&num_2),sizeof(num_2));
      file_img_J_I.write(reinterpret_cast<const char *>(&num_1),sizeof(num_1));

      // Save to files
      file_img_I_J.write(reinterpret_cast<const char *>(&im_I_x),sizeof(im_I_x));
      file_img_I_J.write(reinterpret_cast<const char *>(&im_I_y),sizeof(im_I_y));
      file_img_I_J.write(reinterpret_cast<const char *>(&im_J_x),sizeof(im_J_x));
      file_img_I_J.write(reinterpret_cast<const char *>(&im_J_y),sizeof(im_J_y));

      file_img_J_I.write(reinterpret_cast<const char *>(&im_J_x),sizeof(im_J_x));
      file_img_J_I.write(reinterpret_cast<const char *>(&im_J_y),sizeof(im_J_y));
      file_img_J_I.write(reinterpret_cast<const char *>(&im_I_x),sizeof(im_I_x));
      file_img_J_I.write(reinterpret_cast<const char *>(&im_I_y),sizeof(im_I_y));
    }
  file_img_I_J.close();
  file_img_J_I.close();
  }
  return EXIT_SUCCESS;
}
