#include <tcl.h>

#define TCLECAP_INTERP_KEY_ACTION "::ecap-tcl::action"

#ifdef __cplusplus
extern "C" {
#endif

int TcleCAP_InitialiseInterpreter(Tcl_Interp *interp);
int TcleCAP_ActionHeaderCmd(ClientData clientData, Tcl_Interp *interp,
                                  int objc, Tcl_Obj *const objv[]);
int TcleCAP_ActionHostCmd(ClientData clientData, Tcl_Interp *interp,
                          int objc, Tcl_Obj *const objv[]);
#ifdef __cplusplus
}
#endif
