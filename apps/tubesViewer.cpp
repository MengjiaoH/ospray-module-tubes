#include <vector>
#include <random>

#include "common/sg/SceneGraph.h"
#include "common/sg/Renderer.h"
#include "common/sg/common/Data.h"
#include "common/sg/geometry/Geometry.h"
#include "common/sg/volume/TetVolume.h"

#include "CommandLine.h"
#include "importer.h"

#include "exampleViewer/widgets/imguiViewer.h"

extern int NumFileLoaded;
extern std::vector<ospcommon::vec4f> colors;
#define TensorVolume "/home/sci/mengjiao/Desktop/data/WhiteMatter/volume/019_NAA_013-tensor-mask.osp"
#define LeftVolume "/home/sci/mengjiao/Desktop/data/WhiteMatter/volume/019_NAA_013-TR.osp"

namespace ospray {

  /*! though not required, it is good practice to put any module into
    its own namespace (isnide of ospray:: ). Unlike for the naming of
    library and init function, the naming for this namespace doesn't
    particularlly matter. E.g., 'bilinearPatch', 'module_blp',
    'bilinar_patch' etc would all work equally well. */
  namespace tubes {
    using namespace sg;

    /*! A Simple Triangle Mesh that stores vertex, normal, texcoord,
        and vertex color in separate arrays */
    struct TubeSGNode : public sg::Geometry
    {
      TubeSGNode() : Geometry("tubes") {
              createChild("material", "Material");
      }

    // compute bounding box  
      box3f computeBounds() const
      {
        box3f bounds = empty;
        if (hasChild("nodeData")) {
          auto nodeData = child("nodeData").nodeAs<DataBuffer>();
          auto *base = (byte_t*)nodeData->base(); // base_ptr
          size_t size =  nodeData -> size();
          // std::cout << "size is: " << size << std::endl;

          node *base_ptr = (node*) base;

          for (uint32_t i = 0; i < size; i++){
            vec3f pos = base_ptr[i].pos;
            bounds.extend(pos);
          }
          std::cout << "bound is: " << bounds << std::endl;
        }
        return bounds;
      }
      void setFromXML(const xml::Node &node,  const unsigned char *binBasePtr) override {}
    };

    // use ospcommon for vec3f etc
    using namespace ospcommon;

    extern "C" int main(int ac, const char **av)
    {
      int init_error = ospInit(&ac, av);
      if (init_error != OSP_NO_ERROR) {
        std::cerr << "FATAL ERROR DURING INITIALIZATION!" << std::endl;
        return init_error;
      }

      auto device = ospGetCurrentDevice();
      if (device == nullptr) {
        std::cerr << "FATAL ERROR DURING GETTING CURRENT DEVICE!" << std::endl;
        return 1;
      }

      ospDeviceSetStatusFunc(device, [](const char *msg) { std::cout << msg; });
      ospDeviceSetErrorFunc(device,
                            [](OSPError e, const char *msg) {
                              std::cout << "OSPRAY ERROR [" << e << "]: "
                                        << msg << std::endl;
                              std::exit(1);
                            });

      ospDeviceCommit(device);

      // access/load symbols/sg::Nodes dynamically
      loadLibrary("ospray_sg");
      ospLoadModule("tubes");

      ospray::imgui3D::init(&ac,av);

      // parse the commandline; complain about anything we do not
      // recognize
      CommandLine args(ac,av);

      box3f worldBounds;
      tube tubes;
      tubes = readTubesFromFiles(args.inputFiles, worldBounds);

      // color list
      std::vector<vec4f> colorList = colors;

      auto renderer_ptr = sg::createNode("renderer", "Renderer");
      auto &renderer = *renderer_ptr;

      auto &win_size = ospray::imgui3D::ImGui3DWidget::defaultInitSize;
      renderer["frameBuffer"]["size"] = win_size;

      renderer["rendererType"] = std::string("scivis");
      auto &lights = renderer["lights"];
      auto &sun = lights.createChild("sun", "DirectionalLight");
      sun["color"] = vec3f(1.f,232.f/255.f,166.f/255.f);
      sun["direction"] = vec3f(0.462f,-1.f,-.1f);
      sun["intensity"] = 1.5f;
      auto &ambient = lights.createChild("ambient", "AmbientLight");
      ambient["intensity"] = 0.9f;
      ambient["color"] = vec3f(174.f/255.f,218.f/255.f,255.f/255.f);

      auto &world = renderer["world"];

    // tubes model
      auto tubeNode = std::make_shared<TubeSGNode>();
      tubeNode->setName("loaded_tubes");
      tubeNode->setType("TubeSGNode");
      auto nodeData = std::make_shared<DataVectorT<tubes::node, OSP_RAW>>();
      auto linkData = std::make_shared<DataVectorT<tubes::link, OSP_RAW>>();
      auto colorData = std::make_shared<DataVectorT<vec4f, OSP_RAW>>();

      for(size_t i = 0; i < tubes.nodeData.size(); i++){
        nodeData -> v.push_back(tubes.nodeData[i]);
      }
      for(size_t i = 0; i < tubes.linkData.size(); i++){
        linkData -> v.push_back(tubes.linkData[i]);
      }
      if(colors.size() == 0){
        for(size_t i = 0; i < colorList.size(); i++){
          colorData -> v.push_back(colorList[i]);
       }
      }else{
        for(size_t i = 0; i < colors.size(); i++){
          colorData -> v.push_back(colors[i]);
        }
      }


      nodeData -> setName("nodeData");
      linkData -> setName("linkData");
      colorData -> setName("colorData");

      tubeNode -> add(nodeData);
      tubeNode -> add(linkData);
      tubeNode -> add(colorData);

      box3f bounds = empty;
      bounds = tubeNode -> computeBounds();
      // std::cout << bounds.lower.x << bounds.lower.y << bounds.lower.z << std::endl;
       

      auto tube_model = createNode("tube_model", "Model");
      // auto volume_model = createNode("volume", "TetVolume");
      tube_model->add(tubeNode);
      // volume_model-> add(volumeNode);

      auto tube_instance = createNode("tube_instance", "Instance");
      tube_instance->setChild("model", tube_model);
      tube_model->setParent(tube_instance);

      world.add(tube_instance);
      // world.add(Tensor_Volume);
      // world.add(TR_Volume);

      ospray::ImGuiViewer window(renderer_ptr);
      // vec3f dir (vec3f(0.0f, 0.0f, 0.0f));
      auto &viewPort = window.viewPort;
      // XXX SG is too restrictive: OSPRay cameras accept non-normalized directions
      auto dir = normalize(viewPort.at - viewPort.from);
      renderer["camera"]["dir"] = dir;
      renderer["camera"]["pos"] = viewPort.from;
      renderer["camera"]["up"]  = viewPort.up;
      renderer["camera"]["fovy"] = viewPort.openingAngle;
      renderer["camera"]["apertureRadius"] = viewPort.apertureRadius;
      if (renderer["camera"].hasChild("focusdistance"))
        renderer["camera"]["focusdistance"] = length(viewPort.at - viewPort.from);

      window.create("OSPRay Example Viewer (module) App");

      ospray::imgui3D::run();
      return 0;
    }

  } // ::ospray::bilinearPatch
} // ::ospray
