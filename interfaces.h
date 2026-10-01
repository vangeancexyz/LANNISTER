#ifndef INTERFACES_H
#define INTERFACES_H

#include <stddef.h>
#include <stdbool.h>

// engine Msg(), from libtier0.so
typedef void (*MsgFn)(const char* pMsg, ...);

// defined once in main.c, used everywhere else
extern MsgFn EngineMsg;

// CreateInterface() -- every engine .so exports one. version string in,
// singleton ptr out. NULL on version mismatch.
typedef void* (*CreateInterfaceFn)(const char *pName, int *pReturnCode);

// generic vtable-holder view -- vtable[0] is the func ptr array
typedef struct {
	void **vtable;
} Interface_t;

// IVModelRender::DrawModelExecute -- 3 args + this (state, info, bone)
typedef void (*DrawModelExecuteFn)(Interface_t* thisptr, const void* state, const void* pInfo, void* pCustomBoneToWorld);

// IMaterialSystem::FindMaterial
typedef void* (*FindMaterialFn)(Interface_t* thisptr, const char* pMaterialName, const char* pTextureGroupName, bool complain, const char* pComplainPrefix);

// IMaterial lifecycle and validation
typedef void (*IncrementRefCountFn)(void* thisptr);
typedef void (*DecrementRefCountFn)(void* thisptr);
typedef bool (*IsErrorMaterialFn)(void* thisptr);

// IVModelRender::ForcedMaterialOverride
typedef void (*ForcedMaterialOverrideFn)(Interface_t* thisptr, void* pMaterial, int nOverrideType, int nOverrides);

// IVRenderView global state used for upcoming model draws
typedef void (*SetBlendFn)(Interface_t* thisptr, float blend);
typedef void (*SetColorModulationFn)(Interface_t* thisptr, const float* rgb);

#endif /* INTERFACES_H */
