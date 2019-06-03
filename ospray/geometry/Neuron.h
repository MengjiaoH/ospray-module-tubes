// ======================================================================== //
// Copyright 2009-2016 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

// ospray
#include "ospray/geometry/Geometry.h"

namespace ospray {
  namespace tubes {

    struct NeuronGeometry : public ospray::Geometry {
      struct Node {
        vec3f pos;
        float rad;
      };
      struct Link {
        uint32 first, second;
      };

      struct Comp {
        typedef enum { SPHERE, CYLINDER, CONE } CompType;

        affine3f unitXfm;
        linear3f backXfm;
        float    z0, z1;
        CompType type;
        int32    node0, node1;

        /*! initialize this comp to accelerate a link; return true if this is a valid comp */
        bool initLink(const Node *nodeArray, const Link &link);
        /*! initialize this comp to accelerate a node; return true if this is a valid comp */
        bool initNode(const Node *nodeArray, const uint32 nodeID);
      };

      NeuronGeometry();

      //! \brief common function to help printf-debugging 
      virtual std::string toString() const { return "ospray::neuron::NeuronGeometry"; }
      
      /*! \brief integrates this geometry's primitives into the respective
        model's comperation structure */
      virtual void finalize(Model *model);

      Ref<Data> linkData;
      Ref<Data> nodeData;
      Ref<Data> colorData;
 
      vec4f *colorArray{nullptr};

      /*! @{ \brief Comp's created for the neurons components */
      Comp *comp;
      size_t numComps;
      /*! @} */

    };


  }
}
