#include <fstream>
#include "neuron.h"
#include "common/xml/XML.h"

template<typename T>
T readBinary(std::ifstream& file) {
  T x; file.read((char*)&x, sizeof(T));
  return x;
}


namespace ospray{
    namespace tubes{
        using namespace ospcommon;

/*
 *** Parse SWC
 */
 bool parseswc(FILE *file, neuron &Neuron)
{
    // std::vector<vec4f> colorList;
    // colorList.push_back(vec4f(1.0f, 0.3f, 0.1f, 1.0f));     colorList.push_back(vec4f(0.5f, 0.4f, 0.4f, 1.0f));      colorList.push_back(vec4f(0.9f, 0.3f, 0.6f, 1.0f));
    // colorList.push_back(vec4f(0.5f, 0.3f, 1.0f, 1.0f));

    size_t numNeuronRead = 0;
    static const size_t lineSize = 10000;
    char line[lineSize];

    int numNodes = Neuron.nodeData.size();
    std::cout << "File Info: " << std::endl;
    std::cout << "numNodes: " << numNodes << std::endl;

    // while (fgets(line,10000,file)){
    for (char line[10000]; fgets(line, 10000, file) && !feof(file);){
      if (line[0] == '#') {
	std::cout << "comment line " <<std::endl;
        continue;}


        vec3f p;
        float radius;
        int first, second;
        int partIndex;
        link tempNodeLink;
        node tempNode;

        sscanf(line, "%i %i %f %f %f %f %i\n", &first, &partIndex, &p.x, &p.y, &p.z, &radius, &second);

        if (second == -1 ){
            second = first;
            }

        tempNodeLink = {first - 1, second - 1};
        Neuron.linkData.push_back(tempNodeLink);
        tempNode = {p, radius};
        Neuron.nodeData.push_back(tempNode);

        }// end of while

        std::cout << "#osp:neuron: done parsing " << file<< " (" << Neuron.nodeData.size() << " nodes)" << std::endl;
        return true;
}

bool parseRaw(std::string file, neuron &Neuron){
  std::ifstream myfile;
  myfile.open(file, std::ios::binary);
  myfile.seekg(0);

  int numStreamLines = readBinary<int>(myfile);
  std::cout << numStreamLines << std::endl;

  for (int i = 0; i < numStreamLines; ++i) {
      int numPoints = readBinary<int>(myfile);
      //std::cout << numPoints << std::endl;
    for(int j = 0; j < numPoints; j++){
           node tempNode;
           link tempLink;
           vec3f p = readBinary<vec3f>(myfile);
           tempNode = {p, 0.1};
           size_t curNodeID = Neuron.nodeData.size();
           if(j == 0){
               tempLink = {curNodeID,curNodeID};
           }else{
               tempLink = {curNodeID,curNodeID-1};
           }
           Neuron.nodeData.push_back(tempNode);
           Neuron.linkData.push_back(tempLink);
    }
  }
  std::cout << "done parsing raw data" << std::endl;
} // end parse raw


            static const char *delim = "\n\t\r ";

            void osxParseInt(std::vector<int> &vec, const std::string &content)
            {
              char *s = strdup(content.c_str());
              char *tok = strtok(s,delim);
              while (tok) {
                vec.push_back(atol(tok));
                tok = strtok(NULL,delim);
              }
              free(s);
            }
            void osxParseFloat(std::vector<float> &vec, const std::string &content)
            {
              char *s = strdup(content.c_str());
              char *tok = strtok(s,delim);
              while (tok) {
                vec.push_back(atof(tok));
                tok = strtok(NULL,delim);

              }
              free(s);
            }

            template<typename T> T ato(const char *);
            template<> inline int ato<int>(const char *s) { return atol(s); }
            template<> inline float ato<float>(const char *s) { return atof(s); }

            template<typename T>
            vec_t<T,3> osxParseVec3(char * &tok) {
              vec_t<T,3> v;
              assert(tok);
              v.x = ato<T>(tok);
              tok = strtok(NULL,delim);

              assert(tok);
              v.y = ato<T>(tok);
              tok = strtok(NULL,delim);

              assert(tok);
              v.z = ato<T>(tok);
              tok = strtok(NULL,delim);

              return v;
            }

            template<typename T>
            vec_t<T,4> osxParseVec4(char * &tok) {
              vec_t<T,4> v;
              assert(tok);
              v.x = ato<T>(tok);
              tok = strtok(NULL,delim);

              assert(tok);
              v.y = ato<T>(tok);
              tok = strtok(NULL,delim);

              assert(tok);
              v.z = ato<T>(tok);
              tok = strtok(NULL,delim);

              assert(tok);
              v.w = ato<T>(tok);
              tok = strtok(NULL,delim);

              return v;
            }


            void osxParseVec3i(std::vector<vec3i> &vec, const std::string &content)
            {
              char *s = strdup(content.c_str());
              char *tok = strtok(s,delim);
              while (tok)
                vec.push_back(osxParseVec3<int>(tok));
              free(s);
            }

            void osxParseVec3fa(std::vector<vec3fa> &vec, const std::string &content)
            {
              char *s = strdup(content.c_str());
              char *tok = strtok(s,delim);
              while (tok)
                vec.push_back(osxParseVec3<float>(tok));
              free(s);
            }
            void osxParseVec3f(std::vector<vec3f> &vec, const std::string &content)
            {
              char *s = strdup(content.c_str());
              char *tok = strtok(s,delim);
              while (tok)
                vec.push_back(osxParseVec3<float>(tok));
              free(s);
            }

            void osxParseVec4i(std::vector<vec4i> &vec, const std::string &content)
            {
              char *s = strdup(content.c_str());
              char *tok = strtok(s,delim);
              while (tok)
                vec.push_back(osxParseVec4<int>(tok));
              free(s);
            }


void parseOSX(std::string file, neuron &Neuron){
  std::vector<vec3fa> vertex;
  std::vector<int> index;
  std::cout << " == Start Parse [osx] data: " << std:: endl;
  std::shared_ptr<xml::XMLDoc> doc = xml::readXML(file);
  if (doc->child.size() != 1 || doc->child[0]->name != "OSPRay")
    throw std::runtime_error("could not parse osx file: Not in OSPRay format!?");
  const xml::Node &root_element = *doc->child[0];
  xml::for_each_child_of(root_element,[&](const xml::Node &node){
    if (node.name == "Model") {
      const xml::Node &model_node = node;
      xml::for_each_child_of(model_node, [&](const xml::Node &node){
        if (node.name == "StreamLines") {
          const xml::Node &sl_node = node;
          xml::for_each_child_of(sl_node, [&](const xml::Node &node){
            if (node.name == "vertex")
              osxParseVec3fa(vertex, node.content);
            else if (node.name == "index")
              osxParseInt(index, node.content);
          });
        }
      });
    }
  });
  // debug
  std::cout << "vertex size is: " << vertex.size() << std::endl;
  std::cout << "index size is: " << index.size() << std::endl;
  std::cout << "  == Done parse [osx], Start format to neuron" << std::endl;

  std::vector<help> line_index;
  int start = index[0]; int end = index[0];
  for(int i = 1; i <= index.size(); i++){
    if(index[i] != index[i-1] + 1){
      end = index[i -1] + 1;
      help h = {start, end};
      line_index.push_back(h);
      start = index[i];
    }else{
      end = end + 1;
    }
  }
  std::cout << line_index[0].start << line_index[0].end << std::endl;
for(int i = 0; i < line_index.size(); i++){
  help h = line_index[i];
  float r = 0.1;
  for(int j = h.start; j <= h.end; j++){
    node n = {vertex[j], r};
    link l;
    if(j == h.start){
      l = {Neuron.nodeData.size(), Neuron.nodeData.size()};
    }else{
      l = {Neuron.nodeData.size(), Neuron.nodeData.size()-1};
    }
    Neuron.nodeData.push_back(n);
    Neuron.linkData.push_back(l);
    if(r - 0.0008 > 0.0001f){
          r = r - 0.008;
    }
  }
}
std::cout << Neuron.nodeData[0].pos.x << " " << Neuron.nodeData[0].pos.y << " " << Neuron.nodeData[0].pos.z << std::endl;
std::cout << Neuron.linkData[0].first << " " << Neuron.linkData[0].second << std::endl;
std::cout << Neuron.linkData[1].first << " " << Neuron.linkData[1].second << std::endl;
std::cout << Neuron.linkData[2].first << " " << Neuron.linkData[2].second << std::endl;
std::cout << Neuron.linkData[3].first << " " << Neuron.linkData[3].second << std::endl;

}

void parseVolume(std::string file, volume &Volume){
  std::vector<vec3f> vertex;
  std::vector<float> field;
  std::vector<vec4i> tetrahedra;
  std::cout << " == Start Parse [osx] data: " << std:: endl;
  std::shared_ptr<xml::XMLDoc> doc = xml::readXML(file);
  if (doc->child.size() != 1 || doc->child[0]->name != "OSPRay")
    throw std::runtime_error("could not parse osx file: Not in OSPRay format!?");
  const xml::Node &root_element = *doc->child[0];
  xml::for_each_child_of(root_element,[&](const xml::Node &node){
    if (node.name == "Model") {
      const xml::Node &model_node = node;
      xml::for_each_child_of(model_node, [&](const xml::Node &node){
        if (node.name == "UnstructuredVolume") {
          const xml::Node &sl_node = node;
          xml::for_each_child_of(sl_node, [&](const xml::Node &node){
            if (node.name == "vertex")
              osxParseVec3f(vertex, node.content);
            else if (node.name == "field")
              osxParseFloat(field, node.content);
            else if(node.name == "tetrahedra")
              osxParseVec4i(tetrahedra, node.content);
          });
        }
      });
    }
  });

  //debug
  std::cout << "field size is: " << field.size() << std::endl;
  std::cout << "tetrahedra size is: " << tetrahedra.size() << std::endl;
  std::cout << "vertices size is: " << vertex.size() << std::endl;
  for(int i = 0; i < tetrahedra.size(); i++)
{
  tetrahedra[i].x = tetrahedra[i].x - 1;
  tetrahedra[i].y = tetrahedra[i].y - 1;
  tetrahedra[i].z = tetrahedra[i].z - 1;
  tetrahedra[i].w = tetrahedra[i].w - 1;
}
  std::cout << "first tetrahedra is: " << tetrahedra[0].x << " " << tetrahedra[0].y << " " << tetrahedra[0].z << " " << tetrahedra[0].w << std::endl;
  Volume = {vertex, field, tetrahedra};
}


        neuron readNeuronFromFiles(const std::vector<std::string> &fileNames, box3f &worldBounds)
                // neuron readNeuronFromFiles(const std::string &fileNames, box3f &worldBounds)
        {
            neuron  neurons;

            for(int i = 0; i < fileNames.size(); i++){
                std::string fileName = fileNames[i];
                readNeuronFromFile(neurons, fileName);

            }

            worldBounds = empty;

            for (int i=0;i<neurons.nodeData.size();i++) {
               worldBounds.extend(neurons.nodeData[i].pos-vec3f(neurons.nodeData[i].rad));
               worldBounds.extend(neurons.nodeData[i].pos+vec3f(neurons.nodeData[i].rad));
                }

            return neurons;
        }

        void readNeuronFromFile(neuron &Neuron, const std::string &fileName)
        {
            FILE *file = fopen(fileName.c_str(), "r");
            FileName filename = fileName;
                if(!file){
                    std::cout << "load fail" <<std::endl;
                    }
            // swc file
            if(filename.ext() == "swc"){
                parseswc(file, Neuron);
            }else if(filename.ext() == "slraw"){
                parseRaw(fileName, Neuron);
            }else if(filename.ext() == "osx"){
              parseOSX(fileName, Neuron);
            }
             fclose(file);
      }

     volume readVolumeFromFile(volume &Volume, const std::string &fileName){
          FileName filename = fileName;
          if(filename.ext() == "volume") {parseVolume(fileName, Volume);}
          return Volume;
      }

    } //ospray::neuron
}// ospray
