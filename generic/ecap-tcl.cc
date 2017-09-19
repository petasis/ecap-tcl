/*
 *  eCAP Tcl Adapter: eCAPTcl
 *  -----------------------------------------------------------------
 *
 *  This eCAP adapter is based on the eCAP adapter sample,
 *  available under the following license:
 *
 *  Copyright 2008 The Measurement Factory.
 *  All rights reserved.
 *
 *  This Software is licensed under the terms of the eCAP library (libecap),
 *  including warranty disclaimers and liability limitations.
 *
 *  http://www.e-cap.org/
 */

#include "ecap-tcl.h"

/* Declare a mutex for coordinating multiple threads. */
TCL_DECLARE_MUTEX(eCAPTcl)

namespace Adapter { // not required, but adds clarity

/* The following variable will be set to true the first time Tcl
 * gets initialised. */
  static bool              TclInitialized = false;
  static Tcl_Interp           *mainInterp = NULL;
  const Tcl_ObjType        *bytearrayType = NULL;
  const Tcl_ObjType *proper_bytearrayType = NULL;

// Calls Service::setOne() for each host-provided configuration option.
// See Service::configure().
class Cfgtor: public libecap::NamedValueVisitor {
  public:
    Cfgtor(Service &aSvc): svc(aSvc) {}
    virtual void visit(const libecap::Name &name,
                       const libecap::Area &value) {
      svc.setOne(name, value);
    }
    Service &svc;
};

static char *packData(char *c, void *ptr, size_t sz);
static char *packVoidPtr(char *buff, void *ptr, const char *name, size_t bsz);
static void initialiseThread(Tcl_Interp *interp, void *data);
static void evalThreadScript(Tcl_Interp *interp, void *data);
static void evalInThread(Tcl_Interp *interp, void *data);

static const std::string CfgErrorPrefix = ECAPTCL_ERROR_CONFIGURATION;
static const std::string ErrorPrefix    = ECAPTCL_ERROR_PREFIX;

static std::string getErrorMsg(Tcl_Interp *interp, int code = TCL_ERROR) {
  std::string msg(ErrorPrefix);
  Tcl_Obj *options = Tcl_GetReturnOptions(interp, code);
  Tcl_Obj *key = Tcl_NewStringObj("-errorinfo", -1);
  Tcl_Obj *stackTrace;
  Tcl_IncrRefCount(key);
  Tcl_DictObjGet(NULL, options, key, &stackTrace);
  Tcl_DecrRefCount(key);
  Tcl_IncrRefCount(stackTrace);
  msg += Tcl_GetString(stackTrace);
  Tcl_DecrRefCount(stackTrace);
  Tcl_DecrRefCount(options);
  return msg;
}; /* getErrorMsg */

} // namespace Adapter

Adapter::Service::Service(const std::string &uri_suffix):
    adapter_id_suffix(uri_suffix)
{
}

std::string Adapter::Service::uri() const {
  // printf("%s\n", __PRETTY_FUNCTION__); fflush(0);
  return ECAPTCL_IDENTITY_URI + adapter_id_suffix;
}

std::string Adapter::Service::tag() const {
  // printf("%s\n", __PRETTY_FUNCTION__); fflush(0);
  return PACKAGE_VERSION;
}

void Adapter::Service::describe(std::ostream &os) const {
  // printf("%s\n", __PRETTY_FUNCTION__); fflush(0);
  os << ECAPTCL_IDENTITY_DESCRIBE << PACKAGE_NAME << " v" << PACKAGE_VERSION;
}

void Adapter::Service::configure(const libecap::Options &cfg) {
  // printf("%s\n", __PRETTY_FUNCTION__); fflush(0);
  Cfgtor cfgtor(*this);
  cfg.visitEachOption(cfgtor);

  // check for post-configuration errors and inconsistencies
  if (nthread == 0 && service_init_script.empty()) {
    throw libecap::TextException(CfgErrorPrefix +
      "no threads mode, service_init_script must be set");
  }
  if (nthread != 0 && service_thread_init_script.empty()) {
    throw libecap::TextException(CfgErrorPrefix +
      "threads mode, service_thread_init_script must be set");
  }
}

void Adapter::Service::reconfigure(const libecap::Options &cfg) {
  service_init_script.clear();
  service_start_script.clear();
  service_stop_script.clear();
  service_retire_script.clear();
  service_thread_init_script.clear();
  service_thread_retire_script.clear();
  threads_number.clear();
  nthread = 0;
  freePool();
  configure(cfg);
}

void Adapter::Service::setOne(const libecap::Name &name, const libecap::Area &valArea) {
  const std::string value = valArea.toString();
  if (name == "service_init_script") {
    service_init_script = value;
  } else if (name == "service_start_script") {
    service_start_script = value;
  } else if (name == "service_stop_script") {
    service_stop_script = value;
  } else if (name == "service_retire_script") {
    service_retire_script = value;
  } else if (name == "service_thread_init_script") {
    service_thread_init_script = value;
  } else if (name == "service_thread_retire_script") {
    service_thread_retire_script = value;
  } else if (name == "threads_number") {
    setThreadsNumber(value);
  } else if (name.assignedHostId()) {
    // skip host-standard options we do not know or care about
  } else {
    throw libecap::TextException(CfgErrorPrefix +
      "unsupported configuration parameter: " + name.image());
  }
}

void Adapter::Service::setThreadsNumber(const std::string &value) {

  freePool();
  nthread = 0;
  threads_number = value;
  if (threads_number.empty()) return;

  try {
    std::string::size_type sz;   // alias of size_t
    nthread = std::stoi(value,&sz);
  } catch (int e) {
    throw libecap::TextException(CfgErrorPrefix +
      "invalid integer value for threads_number: " + value);
  }
}

void Adapter::Service::freePool(void) {
  // Call free scripts...
  if (pool != NULL) {
    TPoolFree(pool);
  }
  pool = NULL;
}

void Adapter::Service::initPool(void) {
  TPoolThread *t;
  if (pool != NULL) {
    // We already have a pool..
    if (pool->nthread == nthread) return;
    freePool();
  }
  pool = TPoolInit(nthread);
  if (service_thread_init_script.empty()) return;
  // Initialise thread interprerter...
  for (unsigned int i = 0; i < nthread; i++) {
    t = TPoolStartInThreadPosition(pool, i, initialiseThread,
                           (void *) NULL);
    TPoolThreadWait(t);
  }
  // Call init scripts...
  for (unsigned int i = 0; i < nthread; i++) {
    t = TPoolStartInThreadPosition(pool, i, evalThreadScript,
                           (void *) service_thread_init_script.c_str());
    TPoolThreadWait(t);
  }
}

void Adapter::Service::evalScript(const std::string &path) {
  if (path.empty()) return;
  if (TclInitialized != true) {
    throw libecap::TextException(ErrorPrefix +
      "Tcl is not properly initialised");
  }
  evalThreadScript(mainInterp, (void *) path.c_str());
}

void Adapter::initialiseThread(Tcl_Interp *interp, void *data) {
  if (TclInitialized != true) {
    throw libecap::TextException(ErrorPrefix +
      "Tcl is not properly initialised");
  }
  if (TcleCAP_InitialiseInterpreter(interp) != TCL_OK) {
    throw libecap::TextException(ErrorPrefix + getErrorMsg(interp));
  }
}

void Adapter::evalThreadScript(Tcl_Interp *interp, void *data) {
  Tcl_Obj *value;
  Tcl_DString ds;
  int code;
  const char *path = (const char *) data;
  std::string error;
  if (TclInitialized != true) {
    throw libecap::TextException(ErrorPrefix +
      "Tcl is not properly initialised");
  }
  Tcl_ExternalToUtfDString(NULL, path, -1, &ds);
  value = Tcl_NewStringObj(Tcl_DStringValue(&ds), Tcl_DStringLength(&ds));
  Tcl_IncrRefCount(value);
  Tcl_DStringFree(&ds);
  if (interp == mainInterp) Tcl_MutexLock(&eCAPTcl);
  Tcl_ResetResult(interp);
  code = Tcl_FSEvalFileEx(interp, value, "utf-8");
  if (code != TCL_OK) {
    error = getErrorMsg(interp, code);
  }
  if (interp == mainInterp) Tcl_MutexUnlock(&eCAPTcl);
  Tcl_DecrRefCount(value);
  if (code != TCL_OK) {
    throw libecap::TextException(error);
  }
}

void Adapter::evalInThread(Tcl_Interp *interp, void *clientdata) {
  TclCallClientData *data = (TclCallClientData *) clientdata;
  Tcl_Obj *objv[data->objc], *result;
  unsigned int i;
  int len;
  const char *str;
  if (TclInitialized != true) {
    throw libecap::TextException(ErrorPrefix +
      "Tcl is not properly initialised");
  }
  /* Set the associated data to interp */
  Tcl_SetAssocData(interp, TCLECAP_INTERP_KEY_ACTION, NULL, data->action);
  for (i=0; i<data->objc; i++) {
    switch (data->init[i]) {
      case string:
        objv[i] = Tcl_NewStringObj(data->token[i], -1);
        break;
      case bytes:
        objv[i] = Tcl_NewStringObj(data->token[i], data->size[i]);
        break;
      case bytearray:
        objv[i] = Tcl_NewByteArrayObj((const unsigned char*)
                                      data->token[i], data->size[i]);
        break;
      case boolean:
        objv[i] = Tcl_NewBooleanObj(data->token[i] == NULL ? 0 : 1);
        break;
    }
    Tcl_IncrRefCount(objv[i]);
  }
  if (interp == mainInterp) Tcl_MutexLock(&eCAPTcl);
  data->code = Tcl_EvalObjv(interp, data->objc, objv,
                            TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
  if (data->code == TCL_OK) {
    result = Tcl_GetObjResult(interp);
    Tcl_IncrRefCount(result);
    switch (data->expects) {
      case result_string: {
        // Get its type...
        if (result->typePtr == bytearrayType ||
            result->typePtr == proper_bytearrayType) {
          str = (const char*) Tcl_GetByteArrayFromObj(result, &len);
          // printf("%s Got a byte array (%d) (%p)\n", Tcl_GetString(objv[0]),
          //        len, bytearrayType); fflush(0);
        } else {
          str = Tcl_GetStringFromObj(result, &len);
          // printf("%s Got a string (%d) (%p)\n", Tcl_GetString(objv[0]),
          //        len, bytearrayType); fflush(0);
        }
        data->result.assign(str, len);
        break;
      }
      case result_boolean: {
        data->result_boolean = 0;
        data->code = Tcl_GetBooleanFromObj(interp, result,
                                         &(data->result_boolean));
        break;
      }
    }
    Tcl_DecrRefCount(result);
  }
  Tcl_SetAssocData(interp, TCLECAP_INTERP_KEY_ACTION, NULL, NULL);
  if (interp == mainInterp) Tcl_MutexUnlock(&eCAPTcl);
  for (i=0; i<data->objc; i++) Tcl_DecrRefCount(objv[i]);
}

int Adapter::Service::actionStart(Xaction *action) const {
  TclCallClientData data;
  data.objc     = 2;
  data.token[0] = "::ecap-tcl::actionStart"; data.init[0] = string;
  data.token[1] = action->token;             data.init[1] = string;
  data.action   = action;
  data.expects  = result_string;

  if (nthread && pool) {
    // Use the thread pool...
    thread = TPoolThreadStart(pool, evalInThread,
                                      (void *) &data);
    TPoolThreadWait(thread);
  } else {
    // Use main interpreter...
    evalInThread(mainInterp, (void *) &data);
  }
  size = 0;
  return data.code;
}

int Adapter::Service::actionStop(Xaction *action) const {
  TclCallClientData data;
  data.objc     = 2;
  data.token[0] = "::ecap-tcl::actionStop"; data.init[0] = string;
  data.token[1] = action->token;            data.init[1] = string;
  data.action   = action;
  data.expects  = result_string;

  if (nthread && pool) {
    // Use the thread pool...
    if (thread) {
      TPoolStartInThread(thread, evalInThread, (void *) &data);
    } else {
      thread = TPoolThreadStart(pool, evalInThread, (void *) &data);
    }
    TPoolThreadWait(thread);
    thread = NULL;
  } else {
    // Use main interpreter...
    evalInThread(mainInterp, (void *) &data);
  }
  return data.code;
}

int Adapter::Service::contentAdapt(Xaction *action,
                                   std::string &chunk) const {
  TclCallClientData data;
  data.objc     = 3;
  data.token[0] = "::ecap-tcl::contentAdapt"; data.init[0] = string;
  data.token[1] = action->token;              data.init[1] = string;
  data.token[2] = chunk.data();               data.init[2] = bytearray;
  data.size[2]  = chunk.size();
  data.action   = action;
  data.expects  = result_string;
  size += chunk.size();

  if (nthread && pool) {
    // Use the thread pool...
    if (thread) {
      TPoolStartInThread(thread, evalInThread, (void *) &data);
    } else {
      thread = TPoolThreadStart(pool, evalInThread, (void *) &data);
    }
    TPoolThreadWait(thread);
  } else {
    // Use main interpreter...
    evalInThread(mainInterp, (void *) &data);
  }
  if (data.code == TCL_OK) {
    chunk = data.result;
  }
  return data.code;
}

int Adapter::Service::contentDone(Xaction *action, bool atEnd,
                                  std::string &chunk) const {
  TclCallClientData data;
  data.objc     = 3;
  data.token[0] = "::ecap-tcl::contentDone"; data.init[0] = string;
  data.token[1] = action->token;             data.init[1] = string;
  if (atEnd) data.token[2] = "1"; else data.token[2] = NULL;
  data.init[2]  = boolean;
  data.action   = action;
  data.expects  = result_string;

  if (nthread && pool) {
    // Use the thread pool...
    if (thread) {
      TPoolStartInThread(thread, evalInThread, (void *) &data);
    } else {
      thread = TPoolThreadStart(pool, evalInThread, (void *) &data);
    }
    TPoolThreadWait(thread);
  } else {
    // Use main interpreter...
    evalInThread(mainInterp, (void *) &data);
  }
  if (data.code == TCL_OK) {
    chunk = data.result;
  }
  return data.code;
}

void Adapter::Service::start() {
  Tcl_Obj *byteArrayObject;
  int status;
  libecap::adapter::Service::start();
  // custom code would go here, but this service does not have one
  /* We must initialise Tcl, if this has not already been done. */
  if (TclInitialized == true) {
    evalScript(service_start_script);
    initPool();
    return;
  }

  /* Make sure we have a valid interpreter */
  if (mainInterp == NULL) {
    //Tcl_MutexLock(&eCAPTcl);
    Tcl_FindExecutable(NULL);
    mainInterp = Tcl_CreateInterp();
    //Tcl_MutexUnlock(&eCAPTcl);
  }
  if (mainInterp == NULL) {
    throw libecap::TextException("Cannot create a Tcl Interpreter");
  }

  /* We can initialise the interpreter... */
  Tcl_MutexLock(&eCAPTcl);
  status = Tcl_Init(mainInterp);
  if (status == TCL_ERROR) {
    Tcl_MutexUnlock(&eCAPTcl);
    throw libecap::TextException(getErrorMsg(mainInterp, status));
  }
  bytearrayType = Tcl_GetObjType("bytearray");
  byteArrayObject = Tcl_NewObj();
  Tcl_SetByteArrayObj(byteArrayObject, NULL, 0);
  proper_bytearrayType = byteArrayObject->typePtr;
  Tcl_DecrRefCount(byteArrayObject);
  TclInitialized = true;
  Tcl_MutexUnlock(&eCAPTcl);
  evalScript(service_init_script);
  evalScript(service_start_script);
  initPool();
}

void Adapter::Service::stop() {
  freePool();
  libecap::adapter::Service::stop();
  evalScript(service_stop_script);
}

void Adapter::Service::retire() {
  libecap::adapter::Service::stop();
  evalScript(service_retire_script);
}

bool Adapter::Service::wantsUrl(const char *url) const {
  TclCallClientData data;
  data.objc     = 2;
  data.token[0] = "::ecap-tcl::wantsUrl"; data.init[0] = string;
  data.token[1] = url;                    data.init[1] = string;
  data.action   = NULL;
  data.expects  = result_boolean;
  bool wanted = true;

  // printf("wantsUrl:  %s\n", url);
  if (nthread && pool) {
    // Use the thread pool...
    if (thread) {
      TPoolStartInThread(thread, evalInThread, (void *) &data);
    } else {
      thread = TPoolThreadStart(pool, evalInThread, (void *) &data);
    }
    TPoolThreadWait(thread);
  } else {
    // Use main interpreter...
    evalInThread(mainInterp, (void *) &data);
  }
  switch (data.code) {
    case TCL_OK:
      wanted = data.result_boolean == 0 ? false : true;
      break;
    case TCL_ERROR:
    case TCL_CONTINUE:
      wanted = true;
      break;
    case TCL_BREAK:
      wanted = false;
      break;
  }
  // printf("  wantsUrl: %d (%d)\n", wanted ? 1 : 0, data.code);
  return wanted;
}

#if HAVE_ECAP_VERSION >= 100
Adapter::Service::MadeXactionPointer
Adapter::Service::makeXaction(libecap::host::Xaction *hostx) {
  return Adapter::Service::MadeXactionPointer(
    new Adapter::Xaction(std::tr1::static_pointer_cast<Service>(self), hostx));
}

#else
libecap::adapter::Xaction *
Adapter::Service::makeXaction(libecap::host::Xaction *hostx) {
  return new Adapter::Xaction(std::tr1::static_pointer_cast<Service>(self),
                              hostx);
}
#endif

Adapter::Xaction::Xaction(libecap::shared_ptr<Service> aService,
                          libecap::host::Xaction *x):
                          service(aService),
                          hostx(x),
                          receivingVb(opUndecided), sendingAb(opUndecided) {
}

Adapter::Xaction::~Xaction() {
  if (libecap::host::Xaction *x = hostx) {
    hostx = 0;
    x->adaptationAborted();
  }
}

const libecap::Area Adapter::Xaction::option(const libecap::Name &) const {
  return libecap::Area(); // this transaction has no meta-information
}

void Adapter::Xaction::visitEachOption(libecap::NamedValueVisitor &) const {
  // this transaction has no meta-information to pass to the visitor
}

libecap::host::Xaction *Adapter::Xaction::host() const {return hostx;}
libecap::Message &Adapter::Xaction::adapted() const {
  Must(adaptedx != 0);
  return *adaptedx;
}

void Adapter::Xaction::start() {
  Must(hostx);
  if (hostx->virgin().body()) {
    receivingVb = opOn;
    hostx->vbMake(); // ask host to supply virgin body
  } else {
    // we are not interested in vb if there is not one
    receivingVb = opNever;
  }

  /* adapt message header */

  // libecap::shared_ptr<libecap::Message> adapted = hostx->virgin().clone();
  storeUri();
  adaptedx = hostx->virgin().clone();
  Must(adaptedx != 0);

  // delete ContentLength header because we may change the length
  // unknown length may have performance implications for the host
  // adapted->header().removeAny(libecap::headerContentLength);

  // add a custom header
  // static const libecap::Name name("X-Ecap");
  // const libecap::Header::Value value =
  //   libecap::Area::FromTempString(libecap::MyHost().uri());
  // adapted->header().add(name, value);

  if (!adaptedx->body()) {
    sendingAb = opNever; // there is nothing to send
    lastHostCall()->useAdapted(adaptedx);
  } else {
    // hostx->useAdapted(adaptedx);
    tcl_action_start = true;
    packVoidPtr(token, (void *) this, "_", ACTION_TOKEN_SIZE);
    if (service->actionStart(this) != TCL_OK) {
      // Do what?
    }
  }
}

void Adapter::Xaction::stop() {
  if (tcl_action_start && hostx) {
    tcl_action_start = false;
    service->actionStop(this);
  }
  hostx = 0;
  // the caller will delete
}

void Adapter::Xaction::abDiscard() {
  Must(sendingAb == opUndecided); // have not started yet
  sendingAb = opNever;
  // we do not need more vb if the host is not interested in ab
  stopVb();
}

void Adapter::Xaction::abMake() {
  Must(sendingAb == opUndecided); // have not yet started or decided not to send
  Must(hostx->virgin().body()); // that is our only source of ab content

  // we are or were receiving vb
  Must(receivingVb == opOn || receivingVb == opComplete);

  sendingAb = opOn;
  if (!buffer.empty())
    hostx->noteAbContentAvailable();
}

void Adapter::Xaction::abMakeMore() {
  Must(receivingVb == opOn); // a precondition for receiving more vb
  hostx->vbMakeMore();
}

void Adapter::Xaction::abStopMaking() {
  sendingAb = opComplete;
  // we do not need more vb if the host is not interested in more ab
  stopVb();
}

libecap::Area Adapter::Xaction::abContent(size_type offset, size_type size) {
  Must(sendingAb == opOn || sendingAb == opComplete);
  return libecap::Area::FromTempString(buffer.substr(offset, size));
}

void Adapter::Xaction::abContentShift(size_type size) {
  Must(sendingAb == opOn || sendingAb == opComplete);
  buffer.erase(0, size);
}

void Adapter::Xaction::noteVbContentDone(bool atEnd) {
  Must(receivingVb == opOn);
  std::string chunk;
  service->contentDone(this, atEnd, chunk);
  hostx->useAdapted(adaptedx);
  if (chunk.size()) {
    buffer += chunk; // buffer what we got
    if (sendingAb == opOn)
      hostx->noteAbContentAvailable();
  }
  stopVb();
  if (sendingAb == opOn) {
    hostx->noteAbContentDone(atEnd);
    sendingAb = opComplete;
  }
}

void Adapter::Xaction::noteVbContentAvailable() {
  Must(receivingVb == opOn);

  const libecap::Area vb = hostx->vbContent(0, libecap::nsize); // get all vb
  std::string chunk = vb.toString(); // expensive, but simple
  hostx->vbContentShift(vb.size); // we have a copy; do not need vb any more
  service->contentAdapt(this, chunk);
  buffer += chunk; // buffer what we got

  if (sendingAb == opOn)
    hostx->noteAbContentAvailable();
}

bool Adapter::Xaction::callable() const {
  return hostx != 0; // no point to call us if we are done
}

void Adapter::Xaction::storeUri() {
  typedef const libecap::RequestLine *CLRLP;
  if (!hostx) return;
  if (CLRLP virginLine = dynamic_cast<CLRLP>(&hostx->virgin().firstLine()))
      uri = virginLine->uri();
  else
  if (CLRLP causeLine = dynamic_cast<CLRLP>(&hostx->cause().firstLine()))
      uri = causeLine->uri();
}

const libecap::Area Adapter::Xaction::getUri() const {
  return uri;
}

// tells the host that we are not interested in [more] vb
// if the host does not know that already
void Adapter::Xaction::stopVb() {
  if (receivingVb == opOn) {
    hostx->vbStopMaking(); // we will not call vbContent() any more
    receivingVb = opComplete;
  } else {
    // we already got the entire body or refused it earlier
    Must(receivingVb != opUndecided);
  }
}

// this method is used to make the last call to hostx transaction
// last call may delete adapter transaction if the host no longer needs it
// TODO: replace with hostx-independent "done" method
libecap::host::Xaction *Adapter::Xaction::lastHostCall() {
  libecap::host::Xaction *x = hostx;
  Must(x);
  hostx = 0;
  return x;
}

// create the adapter and register with libecap to reach the host application
#if HAVE_ECAP_VERSION >= 100
static bool Register(const std::string &suffix) {
  return libecap::RegisterVersionedService(new Adapter::Service(suffix));
}

static const bool Registered = Register("");
#else
static bool Register(const std::string &suffix) {
  return (libecap::RegisterService(new Adapter::Service(suffix)), true);
}
static const bool Registered = Register("");
#endif

char *Adapter::packData(char *c, void *ptr, size_t sz) {
  static const char hex[17] = "0123456789abcdef";
  const unsigned char *u = (unsigned char *) ptr;
  const unsigned char *eu =  u + sz;
  for (; u != eu; ++u) {
    unsigned char uu = *u;
    *(c++) = hex[(uu & 0xf0) >> 4];
    *(c++) = hex[uu & 0xf];
  }
  return c;
}

char *Adapter::packVoidPtr(char *buff, void *ptr,
                           const char *name, size_t bsz) {
  char *r = buff;
  if ((2*sizeof(void *) + 2) > bsz) return 0;
  *(r++) = '_';
  r = packData(r,&ptr,sizeof(void *));
  if (strlen(name) + 1 > (bsz - (r - buff))) return 0;
  strcpy(r,name);
  return buff;
}
