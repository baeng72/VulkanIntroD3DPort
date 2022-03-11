#include "FrameResource.h"

FrameResource::FrameResource(PassConstants* pc, InstanceData*id, MaterialData* md) {
	pPCs = pc;
	pInstances = id;
	pMats = md;
}

FrameResource::~FrameResource() {

}