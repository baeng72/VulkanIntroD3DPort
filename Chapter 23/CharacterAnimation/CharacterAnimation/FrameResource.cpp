#include "FrameResource.h"


FrameResource::FrameResource(PassConstants* pc, ObjectConstants* oc,SkinnedConstants*sc, SsaoConstants* pSsao, MaterialData* md) {
	pPCs = pc;
	pOCs = oc;
	pSCs = sc;
	pSsaoCB = pSsao;
	pMats = md;
}

FrameResource::~FrameResource() {

}