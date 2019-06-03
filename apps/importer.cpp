#include <fstream>
#include "importer.h"
#include "common/xml/XML.h"

int NumFileLoaded = 0; // count how many file has beed loaded
int pre_numStreamLines = 0; // count previous file streamlines num
std::vector<ospcommon::vec4f> colors;

// need to read binary file
template<typename T>
    T readBinary(std::ifstream& file) {
        T x; file.read((char*)&x, sizeof(T));
        return x;
    }

namespace ospray{
    namespace tubes{
        using namespace ospcommon;

        /******************************************************
         * Parse SWC File
         *****************************************************/
        bool parseSWC(const std::string file, tube &Tube, std::vector<vec4f> &colors){
            NumFileLoaded++;
            int lineNum = 0;

            std::ifstream infile(file);
            vec3f p;
            float radius;
            int first, second;
            int partIndex;
            vec4f color;
            link tempNodeLink;
            node tempNode;

            while(infile >> first >> partIndex >> p.x >> p.y >> p.z >> radius >> second >> color.x >> color.y >> color.z >> color.w){
                //std::cout << lineNum++ << std::endl;
                lineNum++; // the number of nodes
                if (second == -1 ){
                    second = first;
                }

                tempNodeLink = {first - 1, second - 1, 0, 0};
                Tube.linkData.push_back(tempNodeLink);
                tempNode = {p, radius, partIndex};
                Tube.nodeData.push_back(tempNode);
                colors.push_back(color);
            }
            // add partIndex to link
            // std::cout << "link size: " << Tube.linkData.size() << std::endl;
            for(size_t j = 0; j < Tube.linkData.size(); j++){
                int pre = Tube.linkData[j].second;
                int partIndex = Tube.nodeData[pre].partIndex;
                Tube.linkData[j].partIndex = partIndex;
                Tube.linkData[j].preNumStreamLines;
            }
            pre_numStreamLines += lineNum - 1;
            // //debug
            // for(size_t j = 0; j < Tube.linkData.size(); j++){
            //     std::cout << "first: " << Tube.linkData[j].first << " second: " << Tube.linkData[j].second << std::endl;
            //     std::cout << "partIndex: " << Tube.linkData[j].partIndex << std::endl;
            // }
            // for(int k = 0; k < Tube.nodeData.size(); k++){
            //     std::cout << "pos: " << Tube.nodeData[k].pos.x << " " << Tube.nodeData[k].pos.y << " " << Tube.nodeData[k].pos.z << std::endl;
            //     std::cout << "rad: " << Tube.nodeData[k].rad << std::endl;
            // }

            std::cout << "#osp:tube: done parsing " << file << " (" << Tube.nodeData.size() << " nodes)" << std::endl;
            return true;
        } // end of parse SWC

        bool parseOSP(const std::string file, volume &volume){
            std::shared_ptr<xml::XMLDoc> doc;
            std::cout << "#osp: starting to read OSPRay XML file '" << file << "'" << std::endl;
            doc = xml::readXML(file);
            std::cout << "#osp:sg: XML file read, starting to parse content..." << std::endl;
            assert(doc); 
            if (doc->child.empty())
            throw std::runtime_error("ospray xml input file does not contain any nodes!?");

            if (doc->child[0]->name == "volume") {
                vec3i dimensions(-1);
                std::string fileName = "";
                std::string voxelType = "";
                xml::for_each_child_of(*doc->child[0],[&](const xml::Node &child){
                    if (child.name == "dimensions")
                    dimensions = toVec3i(child.content.c_str());
                    else if (child.name == "voxelType")
                    voxelType = child.content;
                    else if (child.name == "filename")
                    fileName = child.content;
                    else if (child.name == "samplingRate") {

                    } else {
                        throw std::runtime_error("unknown old-style osp file " "component volume::" + child.name);
                    }
                });
                volume.fileName = fileName;
                volume.fileNameOfCorrespondingXmlDoc = doc->fileName;
                volume.dimensions = dimensions;
                volume.voxelType = voxelType;
            }
            return true;
        }


        /*****************************************************
         * Parse Raw Data
         ****************************************************/
        bool parseRAW(const std::string file, tube &Tube){
            NumFileLoaded++;
            std::cout << NumFileLoaded << std::endl;
            std::ifstream myfile;
            myfile.open(file, std::ios::binary);
            myfile.seekg(0);

            int numStreamLines = readBinary<int>(myfile);
            // std::cout << numStreamLines << std::endl;

            for (int i = 0; i < numStreamLines; ++i) {
                int numPoints = readBinary<int>(myfile);
                //std::cout << "num of points: " << numPoints << std::endl;
                for(int j = 0; j < numPoints; j++){
                    node tempNode;
                    link tempLink;
                    vec3f p = readBinary<vec3f>(myfile);
                    tempNode = {p, 0.15, NumFileLoaded};
                    size_t curNodeID = Tube.nodeData.size();
                    if(j == 0){
                        tempLink = {curNodeID,curNodeID, NumFileLoaded, pre_numStreamLines};
                    }else{
                        tempLink = {curNodeID,curNodeID-1, NumFileLoaded, pre_numStreamLines};
                    }
                    Tube.nodeData.push_back(tempNode);
                    Tube.linkData.push_back(tempLink);
                }
            }

            pre_numStreamLines += numStreamLines;
            std::cout << "done parsing raw data" << std::endl;
            return true;
        } // end of parse Raw

        /**
         * Read Tubes From Files
         **/

        tube  readTubesFromFiles(const std::vector<std::string> &fileNames, box3f &worldBounds)
        {
            tube tubes;
            volume volumes;
            for(size_t i = 0; i < fileNames.size(); i++){
                std::string fileName = fileNames[i];
                readTubeFromFile(tubes, fileName);
            }
            worldBounds = empty;
            for (size_t i = 0; i < tubes.nodeData.size(); i++) {
               worldBounds.extend(tubes.nodeData[i].pos-vec3f(tubes.nodeData[i].rad));
               worldBounds.extend(tubes.nodeData[i].pos+vec3f(tubes.nodeData[i].rad));
            }

            return tubes;
        }

        void readTubeFromFile(tube &Tube, const std::string &fileName)
        {
            FILE *file = fopen(fileName.c_str(), "r");
            FileName filename = fileName;
            if(!file){
                std::cout << "load fail" <<std::endl;
            }
            // swc file
            if(filename.ext() == "swc"){
                parseSWC(fileName, Tube, colors);
            }
            else if(filename.ext() == "slraw"){
                parseRAW(fileName, Tube);
            }
            fclose(file);
        }

        volume readVolumeFromFiles(const std::string &fileName){
            FILE *file = fopen(fileName.c_str(), "r");
            FileName filename = fileName;
            volume volume;
            if(!file){
                std::cout << "load fail" <<std::endl;
            }
            if(filename.ext() == "osp"){
                parseOSP(fileName, volume);
            }
            return volume;
        }
    } // ospray::tubes
} // ospray
