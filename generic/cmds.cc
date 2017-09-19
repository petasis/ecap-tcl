#include "ecap-tcl.h"

namespace Adapter {
extern const Tcl_ObjType *bytearrayType;
extern const Tcl_ObjType *proper_bytearrayType;
}

class ValuesToDict: public libecap::NamedValueVisitor {
  public:
    ValuesToDict(Tcl_Interp *i, Tcl_Obj *obj): interp(i), object(obj) {}
    virtual void visit(const libecap::Name &name,
                       const libecap::Area &value) {
    Tcl_DictObjPut(interp, object,
        Tcl_NewStringObj(name.image().c_str(), -1),
        Tcl_NewStringObj(value.start, value.size));
    }
    Tcl_Interp *interp;
    Tcl_Obj *object;
};

int TcleCAP_InitialiseInterpreter(Tcl_Interp *interp) {
  Tcl_Namespace *ecap, *action;

  if (interp == NULL) return TCL_ERROR;
  /* Create namespace ::ecap-tcl */
  ecap = Tcl_CreateNamespace(interp, "::ecap-tcl", NULL, NULL);
  if (ecap == NULL) return TCL_ERROR;
  Tcl_Export(interp, ecap, "*", 0);
  /* Create namespace ::ecap-tcl::action */
  action = Tcl_CreateNamespace(interp, "::ecap-tcl::action", NULL, NULL);
  if (action == NULL) return TCL_ERROR;
  Tcl_Export(interp, action, "*", 0);

  /* Create the commands */
  Tcl_CreateObjCommand(interp, "::ecap-tcl::action::header",
                       TcleCAP_ActionHeaderCmd , NULL, NULL);
  Tcl_CreateObjCommand(interp, "::ecap-tcl::action::headers",
                       TcleCAP_ActionHeaderCmd , NULL, NULL);
  Tcl_CreateObjCommand(interp, "::ecap-tcl::action::host",
                       TcleCAP_ActionHostCmd , NULL, NULL);
  Tcl_CreateObjCommand(interp, "::ecap-tcl::action::content",
                       TcleCAP_ActionContentCmd , NULL, NULL);
  Tcl_CreateObjCommand(interp, "::ecap-tcl::action::client",
                       TcleCAP_ActionClientCmd , NULL, NULL);

  /* Create the ensemble ::ecap-tcl::action */
  Tcl_CreateEnsemble(interp, "::ecap-tcl::action", action, 0);

  return TCL_OK;
}; /* TcleCAP_InitialiseInterpreter */

int TcleCAP_ActionHeaderCmd(ClientData clientData, Tcl_Interp *interp,
                            int objc, Tcl_Obj *const objv[]) {
  ClientData data;
  Adapter::Xaction *action;
  int index, i;

  static const char *const optionStrings[] = {
      "add", "exists", "get", "remove", "set",
      NULL
  };
  enum options {
      HEADER_ADD, HEADER_EXISTS, HEADER_GET, HEADER_REMOVE, HEADER_SET
  };

  /* Get the action pointer from the interpreter... */
  data = Tcl_GetAssocData(interp, TCLECAP_INTERP_KEY_ACTION, NULL);
  if (data == NULL) {
    Tcl_SetResult(interp, (char *) "called ouside an action context: "
                          "no action poiner found", TCL_STATIC);
    return TCL_ERROR;
  }
  action = (Adapter::Xaction *) data;
  if (action->host() == NULL) {
    Tcl_SetResult(interp, (char *) "called ouside an action context: "
                          "no host pointer found", TCL_STATIC);
    return TCL_ERROR;
  }

  if (objc < 2) {
      Tcl_WrongNumArgs(interp, 1, objv, "option ?arg ...?");
      return TCL_ERROR;
  }
  if (Tcl_GetIndexFromObj(interp, objv[1], optionStrings, "option", 0,
          &index) != TCL_OK) {
      return TCL_ERROR;
  }

  switch ((enum options) index) {
    case HEADER_SET:
    case HEADER_ADD: {
      if (objc < 4 || objc % 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "name value ?name value ...?");
        return TCL_ERROR;
      }
      for (i = 2; i < objc; i += 2) {
        const libecap::Name name(Tcl_GetString(objv[i]));
        const libecap::Header::Value value =
          libecap::Area::FromTempString(Tcl_GetString(objv[i+1]));
        if ((enum options) index == HEADER_SET) {
          // If the command is set, remove any other headers with the same name
          action->/*host()->*/adapted().header().removeAny(name);
        }
        action->/*host()->*/adapted().header().add(name, value);
      }
      break;
    }
    case HEADER_EXISTS: {
      const libecap::Name name(Tcl_GetString(objv[2]));
      if (objc > 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "name");
        return TCL_ERROR;
      }
      if (action->/*host()->*/adapted().header().hasAny(name)) {
        Tcl_SetObjResult(interp, Tcl_NewBooleanObj(1));
      } else {
        Tcl_SetObjResult(interp, Tcl_NewBooleanObj(0));
      }
      break;
    }
    case HEADER_GET: {
      if (objc > 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "?name?");
        return TCL_ERROR;
      }
      if (objc == 2) {
        // Visit all nodes...
        ValuesToDict visitor(interp, Tcl_NewDictObj());
        action->/*host()->*/adapted().header().visitEach(visitor);
        Tcl_SetObjResult(interp, visitor.object);
      } else {
        // Get a specific header, if exists...
        const libecap::Name name(Tcl_GetString(objv[2]));
        if (!action->/*host()->*/adapted().header().hasAny(name)) {
          Tcl_ResetResult(interp);
        } else {
          const libecap::Area value =
                action->/*host()->*/adapted().header().value(name);
          Tcl_SetObjResult(interp,
              Tcl_NewStringObj((char *) value.start, value.size));
        }
      }
      break;
    }
    case HEADER_REMOVE: {
      if (objc < 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "name ?name ...?");
        return TCL_ERROR;
      }
      for (i = 2; i < objc; i ++) {
        const libecap::Name name(Tcl_GetString(objv[i]));
        action->/*host()->*/adapted().header().removeAny(name);
      }
      break;
    }
  }
  return TCL_OK;
}

int TcleCAP_ActionHostCmd(ClientData clientData, Tcl_Interp *interp,
                          int objc, Tcl_Obj *const objv[]) {
  ClientData data;
  Adapter::Xaction *action;
  int index;

  static const char *const optionStrings[] = {
      "uri",
      NULL
  };
  enum options {
      HOST_URI
  };

  /* Get the action pointer from the interpreter... */
  data = Tcl_GetAssocData(interp, TCLECAP_INTERP_KEY_ACTION, NULL);
  if (data == NULL) {
    Tcl_SetResult(interp, (char *) "called ouside an action context: "
                          "no action poiner found", TCL_STATIC);
    return TCL_ERROR;
  }
  action = (Adapter::Xaction *) data;
  if (action->host() == NULL) {
    Tcl_SetResult(interp, (char *) "called ouside an action context: "
                          "no host pointer found", TCL_STATIC);
    return TCL_ERROR;
  }

  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "option ?arg ...?");
    return TCL_ERROR;
  }
  if (Tcl_GetIndexFromObj(interp, objv[1], optionStrings, "option", 0,
        &index) != TCL_OK) {
    return TCL_ERROR;
  }

  switch ((enum options) index) {
    case HOST_URI:
      if (objc > 2) {
        Tcl_WrongNumArgs(interp, 2, objv, "");
        return TCL_ERROR;
      }
      Tcl_SetObjResult(interp,
        Tcl_NewStringObj((char *) libecap::MyHost().uri().c_str(), -1));
      break;
  }
  return TCL_OK;
}

int TcleCAP_ActionContentCmd(ClientData clientData, Tcl_Interp *interp,
                          int objc, Tcl_Obj *const objv[]) {
  int index, len = 0;

  static const char *const optionStrings[] = {
      "bytelength",
      NULL
  };
  enum options {
      CONTENT_BYTELENGTH
  };

  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "option ?arg ...?");
    return TCL_ERROR;
  }
  if (Tcl_GetIndexFromObj(interp, objv[1], optionStrings, "option", 0,
        &index) != TCL_OK) {
    return TCL_ERROR;
  }

  switch ((enum options) index) {
    case CONTENT_BYTELENGTH:
      if (objc > 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "data");
        return TCL_ERROR;
      }
      // printf("%p (%s)\n", objv[2]->typePtr, objv[2]->typePtr->name);
      if (objv[2]->typePtr == Adapter::bytearrayType  ||
          objv[2]->typePtr == Adapter::proper_bytearrayType) {
        Tcl_GetByteArrayFromObj(objv[2], &len);
        // printf("%s Got a byte array (%d) (%p -> %s)\n",
        //        Tcl_GetString(objv[1]), len, Adapter::bytearrayType,
        //        Adapter::bytearrayType->name); fflush(0);
      } else {
        Tcl_GetStringFromObj(objv[2], &len);
        // printf("%s Got a string (%d) (%p -> %s)\n",
        //        Tcl_GetString(objv[1]), len, Adapter::bytearrayType,
        //        Adapter::bytearrayType->name); fflush(0);
      }
      Tcl_SetObjResult(interp, Tcl_NewIntObj(len));
      break;
  }
  return TCL_OK;
}

int TcleCAP_ActionClientCmd(ClientData clientData, Tcl_Interp *interp,
                          int objc, Tcl_Obj *const objv[]) {
  ClientData data;
  Adapter::Xaction *action;
  int index;

  static const char *const optionStrings[] = {
      "request_uri",
      NULL
  };
  enum options {
      CLIENT_REQUEST_URI
  };

  /* Get the action pointer from the interpreter... */
  data = Tcl_GetAssocData(interp, TCLECAP_INTERP_KEY_ACTION, NULL);
  action = (Adapter::Xaction *) data;
  if (action->host() == NULL) {
    Tcl_SetResult(interp, (char *) "called ouside an action context: "
                          "no host pointer found", TCL_STATIC);
    return TCL_ERROR;
  }

  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "option ?arg ...?");
    return TCL_ERROR;
  }
  if (Tcl_GetIndexFromObj(interp, objv[1], optionStrings, "option", 0,
        &index) != TCL_OK) {
    return TCL_ERROR;
  }

  switch ((enum options) index) {
    case CLIENT_REQUEST_URI:
      if (objc > 2) {
        Tcl_WrongNumArgs(interp, 2, objv, "");
        return TCL_ERROR;
      }
      const libecap::Area value = action->getUri();
      Tcl_SetObjResult(interp,
              Tcl_NewStringObj((char *) value.start, value.size));
      break;
  }
  return TCL_OK;
}
