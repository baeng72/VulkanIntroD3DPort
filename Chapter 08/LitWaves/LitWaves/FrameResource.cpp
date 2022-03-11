#include "FrameResource.h"

FrameResource::FrameResource(PassConstants* pc, ObjectConstants* oc, MaterialConstants* mc,Vertex*pWvs) {
	pPCs = pc;
	pOCs = oc;
	pMats = mc;
	pWavesVB = pWvs;
}

FrameResource::~FrameResource() {

}