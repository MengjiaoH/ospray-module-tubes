"Tubes" Geometry Module
=======================

What are tubes?
---------------

"Tubes" are basically a wide generalization of ospray's "StreamLines"
geometry type. Whereas ospray's original stream lines only support a
fixed radius for all control points, "tubes" allow a per-node radius,
as well as Y-forks within the tubes.

Compile with OSPRay
-------------------
Clone the repo into module folder inside OSPRay. 
Change "OSPRAY_MODULE_TUBES" to "ON".
Re-compile.

Data Structure
-------------- 
In this module, the name of geometry is "tubes" and we need commit a "nodeData" and a "lineData" to OSPRay.
nodeData and lineData are both vector contains a list of node and a list of line. 
The basic node struct contains: position, radius, partIndex(used for neuron color).
The basic line struct contains: current node index, its predecessor index and partIndex.

All struct you can find in 'importer.h' under apps folder or in 'Tubes.h' under ospray/geometry folder.


