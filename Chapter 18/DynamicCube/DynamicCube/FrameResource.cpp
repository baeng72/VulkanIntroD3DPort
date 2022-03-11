#include "FrameResource.h"

FrameResource::FrameResource(PassConstants* pc, ObjectConstants* oc, MaterialData* md) {
	pPCs = pc;
	pOCs = oc;
	pMats = md;
}

FrameResource::~FrameResource() {

}