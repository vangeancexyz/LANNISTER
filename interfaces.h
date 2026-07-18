#ifndef INTERFACES_H
#define INTERFACES_H

// engine Msg(), from libtier0.so
typedef void (*MsgFn)(const char* pMsg, ...);

// CreateInterface() -- every engine .so exports one. version string in,
// singleton ptr out. NULL on version mismatch.
typedef void* (*CreateInterfaceFn)(const char *pName, int *pReturnCode);

#endif /* INTERFACES_H */
