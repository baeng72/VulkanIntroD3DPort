#include "FrameResource.h"


FrameResource::FrameResource(PassConstants* pc, ObjectConstants* oc,SsaoConstants*pSsao, MaterialData* md) {
	pPCs = pc;
	pOCs = oc;
	pSsaoCB = pSsao;
	pMats = md;
}

FrameResource::~FrameResource() {

}