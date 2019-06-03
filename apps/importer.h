#pragma once
// ospray side
#include <ospcommon/vec.h>
#include <ospcommon/box.h>
#include <ospcommon/FileName.h>
// c++ side
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace ospray{
    namespace tubes{
        using namespace ospcommon;

        struct node{
            vec3f pos;
            float rad;
            int partIndex;
        };
        struct link{
            int first, second;
            int partIndex;
            int preNumStreamLines;
        };
        struct tube{
            std::vector<node> nodeData;
            std::vector<link> linkData;
        };
        struct volume{
          FileName fileNameOfCorrespondingXmlDoc;
          std::string fileName;
          std::string voxelType;
          vec3i dimensions;
        };


        tube readTubesFromFiles(const std::vector<std::string> &fileNames,box3f &worldBounds);
        void readTubeFromFile(tube &Tube,const std::string &fileName);

       volume readVolumeFromFiles(const std::string &fileName);


    }
}
