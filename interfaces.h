#ifndef INTERFACES_H
#define INTERFACES_H

// engine Msg(), from libtier0.so
typedef void (*MsgFn)(const char* pMsg, ...);

// CreateInterface() -- every engine .so exports one. version string in,
// singleton ptr out. NULL on version mismatch.
typedef void* (*CreateInterfaceFn)(const char *pName, int *pReturnCode);

// generic vtable-holder view -- vtable[0] is the func ptr array
typedef struct {
	void **vtable;
} Interface_t;

// IVModelRender::DrawModelExecute -- 3 args + this (state, info, bone)
typedef void (*DrawModelExecuteFn)(Interface_t* thisptr, const void* state, const void* pInfo, void* pCustomBoneToWorld);

#endif /* INTERFACES_H */
